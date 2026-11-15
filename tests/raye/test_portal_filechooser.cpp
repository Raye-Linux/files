/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The Open and Save portal, called over a real bus the way xdg-desktop-portal
// calls it: every option goes through D-Bus, the dialog is driven, and the
// answer is read back from the reply.

#include "raye/openwith/openwithdialog.h"
#include "raye/portal/filedialog.h"
#include "raye/portal/portalservice.h"
#include "raye/ui/carddialog.h"
#include "raye/ui/commandbutton.h"

#include <KService>
#include <KSycoca>

#include <QApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using Raye::Portal::FileDialog;

namespace
{
const QString service = QStringLiteral("org.raye.test.portal");
const QString objectPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString handle = QStringLiteral("/org/freedesktop/portal/desktop/request/test");

// What an app sends, declared here on its own: a test that shared the portal's
// types could not see them drift from the documented signatures.
struct AppGlob {
    uint type;
    QString pattern;
};
struct AppFilter {
    QString label;
    QList<AppGlob> globs;
};
struct AppOption {
    QString id;
    QString label;
};
struct AppChoice {
    QString id;
    QString label;
    QList<AppOption> options;
    QString initial;
};
}

Q_DECLARE_METATYPE(AppGlob)
Q_DECLARE_METATYPE(AppFilter)
Q_DECLARE_METATYPE(AppOption)
Q_DECLARE_METATYPE(AppChoice)

namespace
{
QDBusArgument &operator<<(QDBusArgument &arg, const AppGlob &v)
{
    arg.beginStructure();
    arg << v.type << v.pattern;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, AppGlob &v)
{
    arg.beginStructure();
    arg >> v.type >> v.pattern;
    arg.endStructure();
    return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const AppFilter &v)
{
    arg.beginStructure();
    arg << v.label << v.globs;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, AppFilter &v)
{
    arg.beginStructure();
    arg >> v.label >> v.globs;
    arg.endStructure();
    return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const AppOption &v)
{
    arg.beginStructure();
    arg << v.id << v.label;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, AppOption &v)
{
    arg.beginStructure();
    arg >> v.id >> v.label;
    arg.endStructure();
    return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const AppChoice &v)
{
    arg.beginStructure();
    arg << v.id << v.label << v.options << v.initial;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, AppChoice &v)
{
    arg.beginStructure();
    arg >> v.id >> v.label >> v.options >> v.initial;
    arg.endStructure();
    return arg;
}

QVariant filterList(const QList<AppFilter> &filters)
{
    return QVariant::fromValue(filters);
}

QVariant oneFilter(const QString &label, const QList<AppGlob> &globs)
{
    return QVariant::fromValue(AppFilter{label, globs});
}

QVariant choicesOption()
{
    return QVariant::fromValue(QList<AppChoice>{
        {QStringLiteral("encoding"), QStringLiteral("Encoding"), {{QStringLiteral("utf8"), QStringLiteral("UTF-8")}, {QStringLiteral("latin"), QStringLiteral("Western")}}, QStringLiteral("latin")},
        {QStringLiteral("keep"), QStringLiteral("Keep a copy"), {}, QStringLiteral("true")},
    });
}

template<typename T>
T *waitForTopLevel()
{
    T *found = nullptr;
    QTest::qWaitFor(
        [&found] {
            const auto widgets = QApplication::topLevelWidgets();
            for (QWidget *w : widgets) {
                if (auto *d = qobject_cast<T *>(w); d && d->isVisible()) {
                    found = d;
                    return true;
                }
            }
            return false;
        },
        5000);
    return found;
}
}

class TestPortalFileChooser : public QObject
{
    Q_OBJECT
public:
    static void initMain()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void openOneFile();
    void openSeveralFiles();
    void openRefusesMissingFile();
    void openNameWithQuotes();
    void openFolderMode();
    void folderModeShowsOnlyFolders();
    void filtersKeepFolders();
    void currentFilterIsPickedAndAnswered();
    void currentFilterOutsideTheList();
    void acceptLabelAndTitle();
    void cancelAnswersCancelled();
    void closingTheRequestAnswersCancelled();
    void saveFileWithCurrentName();
    void saveAddsTheTypeExtension();
    void saveExtensionWithDotInFolder();
    void saveAsksBeforeReplacing();
    void saveCancelledAtTheQuestionStaysOpen();
    void saveFilesWritesEachNameInTheFolder();
    void choicesComeBack();
    void folderTypedInTheFieldIsEntered();
    void searchNarrowsTheFolder();
    void sizeIsRemembered();
    void appChooserAnswersTheChoice();
    void appChooserCancel();

private:
    QVariantList call(const QString &method, const QVariantMap &options, const std::function<void()> &drive);
    QDBusPendingCall start(const QString &method, const QString &title, const QVariantMap &options);
    QString path(const QString &name) const
    {
        return m_dir->filePath(name);
    }
    QByteArray folderBytes() const
    {
        QByteArray bytes = QFile::encodeName(m_dir->path());
        bytes.append('\0');
        return bytes;
    }
    // a{sv} arrives still marshalled.
    static QVariantMap resultsIn(const QVariantList &answer)
    {
        return qdbus_cast<QVariantMap>(answer.value(1));
    }
    static QStringList urisIn(const QVariantList &answer)
    {
        return resultsIn(answer).value(QStringLiteral("uris")).toStringList();
    }

    QProcess m_bus;
    QObject *m_root = nullptr;
    QScopedPointer<QTemporaryDir> m_dir;
    QDBusConnection m_client{QString()};
};

void TestPortalFileChooser::initTestCase()
{
    // A bus of our own, so the test needs no session and touches none.
    m_bus.setProgram(QStringLiteral("dbus-daemon"));
    m_bus.setArguments({QStringLiteral("--session"), QStringLiteral("--nofork"), QStringLiteral("--print-address")});
    m_bus.start();
    QVERIFY2(m_bus.waitForStarted(), "dbus-daemon is needed to run this test");
    QVERIFY(m_bus.waitForReadyRead(5000));
    const QByteArray address = m_bus.readLine().trimmed();
    QVERIFY(!address.isEmpty());
    qputenv("DBUS_SESSION_BUS_ADDRESS", address);
    // The places list asks the system bus for drives. A build machine has no
    // such bus, and libdbus then writes its "not connected" answers from Qt's
    // bus thread while this thread writes messages, which corrupts its heap.
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", address);
    qputenv("RAYE_FILE_DIALOG_QUIET", "1");

    KSycoca::setupTestMenu();
    qDBusRegisterMetaType<AppGlob>();
    qDBusRegisterMetaType<QList<AppGlob>>();
    qDBusRegisterMetaType<AppFilter>();
    qDBusRegisterMetaType<QList<AppFilter>>();
    qDBusRegisterMetaType<AppOption>();
    qDBusRegisterMetaType<QList<AppOption>>();
    qDBusRegisterMetaType<AppChoice>();
    qDBusRegisterMetaType<QList<AppChoice>>();
    QVERIFY(QDBusConnection::sessionBus().isConnected());
    QVERIFY(QDBusConnection::sessionBus().interface()->registerService(service));
    m_root = new QObject(this);
    Raye::Portal::addPortalInterfaces(m_root);
    QVERIFY(QDBusConnection::sessionBus().registerObject(objectPath, m_root, QDBusConnection::ExportAdaptors));

    // The app side: its own connection, as a separate process would have.
    m_client = QDBusConnection::connectToBus(QString::fromLatin1(address), QStringLiteral("client"));
    QVERIFY(m_client.isConnected());
}

void TestPortalFileChooser::cleanupTestCase()
{
    QDBusConnection::disconnectFromBus(QStringLiteral("client"));
    m_bus.kill();
    m_bus.waitForFinished();
}

void TestPortalFileChooser::init()
{
    m_dir.reset(new QTemporaryDir);
    QVERIFY(m_dir->isValid());
    for (const QString &name : {QStringLiteral("a.txt"), QStringLiteral("b.txt"), QStringLiteral("c.png")}) {
        QFile f(path(name));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello");
    }
    QVERIFY(QDir(m_dir->path()).mkdir(QStringLiteral("sub")));
}

void TestPortalFileChooser::cleanup()
{
    // A test that failed halfway leaves its dialog open: close it, or every
    // test after it finds that one instead of its own.
    const auto widgets = QApplication::topLevelWidgets();
    for (QWidget *w : widgets) {
        if (auto *dialog = qobject_cast<QDialog *>(w); dialog && dialog->isVisible()) {
            dialog->reject();
        }
    }
    QTRY_COMPARE(Raye::Portal::openDialogs(), 0);
}

QDBusPendingCall TestPortalFileChooser::start(const QString &method, const QString &title, const QVariantMap &options)
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, objectPath, QStringLiteral("org.freedesktop.impl.portal.FileChooser"), method);
    message << QVariant::fromValue(QDBusObjectPath(handle)) << QStringLiteral("org.test.App") << QString() << title << options;
    return m_client.asyncCall(message, 60000);
}

QVariantList TestPortalFileChooser::call(const QString &method, const QVariantMap &options, const std::function<void()> &drive)
{
    QDBusPendingCall pending = start(method, QStringLiteral("Test"), options);
    drive();
    QTest::qWaitFor([&pending] {
        return pending.isFinished();
    }, 5000);
    if (!pending.isFinished() || pending.isError()) {
        qWarning() << pending.error();
        return {};
    }
    return pending.reply().arguments();
}

void TestPortalFileChooser::openOneFile()
{
    const QVariantList answer = call(QStringLiteral("OpenFile"), {{QStringLiteral("current_folder"), folderBytes()}}, [this] {
        FileDialog *d = waitForTopLevel<FileDialog>();
        QVERIFY(d);
        QCOMPARE(d->folder(), QUrl::fromLocalFile(m_dir->path()));
        d->setFieldText(QStringLiteral("a.txt"));
        QVERIFY(d->tryAccept());
    });
    QCOMPARE(answer.size(), 2);
    QCOMPARE(answer.at(0).toUInt(), Raye::Portal::responseSuccess);
    QCOMPARE(urisIn(answer), QStringList{QUrl::fromLocalFile(path(QStringLiteral("a.txt"))).toString()});
}

void TestPortalFileChooser::openSeveralFiles()
{
    const QVariantList answer = call(QStringLiteral("OpenFile"), {{QStringLiteral("current_folder"), folderBytes()}, {QStringLiteral("multiple"), true}}, [] {
        FileDialog *d = waitForTopLevel<FileDialog>();
        QVERIFY(d);
        d->setFieldText(QStringLiteral("\"a.txt\" \"b.txt\""));
        QVERIFY(d->tryAccept());
    });
    QCOMPARE(answer.value(0).toUInt(), Raye::Portal::responseSuccess);
    QCOMPARE(urisIn(answer).size(), 2);
}

void TestPortalFileChooser::openNameWithQuotes()
{
    // "report".txt picked in the list must open itself, not "report".
    const QString quoted = QStringLiteral("\"report\".txt");
    for (const QString &name : {quoted, QStringLiteral("report")}) {
        QFile f(path(name));
        QVERIFY(f.open(QIODevice::WriteOnly));
    }
    const QVariantList answer = call(QStringLiteral("OpenFile"), {{QStringLiteral("current_folder"), folderBytes()}}, [quoted] {
        FileDialog *d = waitForTopLevel<FileDialog>();
        QVERIFY(d);
        d->setFieldText(Raye::Portal::fieldFromNames({quoted}));
        QVERIFY(d->tryAccept());
    });
    QCOMPARE(answer.value(0).toUInt(), Raye::Portal::responseSuccess);
    QCOMPARE(urisIn(answer), QStringList{QUrl::fromLocalFile(path(quoted)).toString()});
}

void TestPortalFileChooser::openRefusesMissingFile()
{
    QDBusPendingCall pending = start(QStringLiteral("OpenFile"), QStringLiteral("Test"), {{QStringLiteral("current_folder"), folderBytes()}});
    FileDialog *d = waitForTopLevel<FileDialog>();
    QVERIFY(d);
    d->setFieldText(QStringLiteral("nothing-here.txt"));
    QVERIFY(!d->tryAccept());
    QVERIFY2(d->lastMessage().contains(QStringLiteral("nothing-here.txt")), qPrintable(d->lastMessage()));
    QVERIFY(d->isVisible());
    // Several names are refused when the app asked for one.
    d->setFieldText(QStringLiteral("\"a.txt\" \"b.txt\""));
    QVERIFY(!d->tryAccept());
    d->reject();
    QTRY_VERIFY(pending.isFinished());
}

void TestPortalFileChooser::openFolderMode()
{
    const QVariantList answer = call(QStringLiteral("OpenFile"), {{QStringLiteral("current_folder"), folderBytes()}, {QStringLiteral("directory"), true}}, [] {
        FileDialog *d = waitForTopLevel<FileDialog>();
        QVERIFY(d);
        d->setFieldText(QStringLiteral("sub"));
        QVERIFY(d->tryAccept());
    });
    QCOMPARE(answer.value(0).toUInt(), Raye::Portal::responseSuccess);
    QCOMPARE(urisIn(answer), QStringList{QUrl::fromLocalFile(path(QStringLiteral("sub"))).toString()});
}

void TestPortalFileChooser::folderModeShowsOnlyFolders()
{
    QDBusPendingCall pending = start(QStringLiteral("OpenFile"), QStringLiteral("Test"), {{QStringLiteral("current_folder"), folderBytes()}, {QStringLiteral("directory"), true}});
    FileDialog *d = waitForTopLevel<FileDialog>();
    QVERIFY(d);
    QTRY_COMPARE(d->shownNames(), QStringList{QStringLiteral("sub")});
    // With nothing typed the folder on screen is the answer.
    QVERIFY(d->tryAccept());
    QTRY_VERIFY(pending.isFinished());
    QCOMPARE(urisIn(pending.reply().arguments()), QStringList{QUrl::fromLocalFile(m_dir->path()).toString()});
}

void TestPortalFileChooser::filtersKeepFolders()
{
    const QVariantMap options{{QStringLiteral("current_folder"), folderBytes()},
                              {QStringLiteral("filters"), filterList({{QStringLiteral("Text"), {{0, QStringLiteral("*.[tT][xX][tT]")}}}, {QStringLiteral("All"), {{0, QStringLiteral("*")}}}})}};
    QDBusPendingCall pending = start(QStringLiteral("OpenFile"), QStringLiteral("Test"), options);
    FileDialog *d = waitForTopLevel<FileDialog>();
    QVERIFY(d);
    QTRY_COMPARE(d->shownNames(), (QStringList{QStringLiteral("a.txt"), QStringLiteral("b.txt"), QStringLiteral("sub")}));
    d->setCurrentType(1);
    QTRY_COMPARE(d->shownNames(), (QStringList{QStringLiteral("a.txt"), QStringLiteral("b.txt"), QStringLiteral("c.png"), QStringLiteral("sub")}));
    d->reject();
    QTRY_VERIFY(pending.isFinished());
}

void TestPortalFileChooser::currentFilterIsPickedAndAnswered()
{
    const QList<AppGlob> png{{0, QStringLiteral("*.png")}};
    const QVariantMap options{{QStringLiteral("current_folder"), folderBytes()},
                              {QStringLiteral("filters"), filterList({{QStringLiteral("Text"), {{0, QStringLiteral("*.txt")}}}, {QStringLiteral("Pictures"), png}})},
                              {QStringLiteral("current_filter"), oneFilter(QStringLiteral("Pictures"), png)}};
    const QVariantList answer = call(QStringLiteral("OpenFile"), options, [] {
        FileDialog *d = waitForTopLevel<FileDialog>();
        QVERIFY(d);
        QCOMPARE(d->currentType(), 1);
        QTRY_COMPARE(d->shownNames(), (QStringList{QStringLiteral("c.png"), QStringLiteral("sub")}));
        d->setFieldText(QStringLiteral("c.png"));
        QVERIFY(d->tryAccept());
    });
    QCOMPARE(answer.value(0).toUInt(), Raye::Portal::responseSuccess);
    const auto results = resultsIn(answer);
    QVERIFY(results.contains(QStringLiteral("current_filter")));
    const auto current = qdbus_cast<AppFilter>(results.value(QStringLiteral("current_filter")));
    QCOMPARE(current.label, QStringLiteral("Pictures"));
    QCOMPARE(current.globs.size(), 1);
    QCOMPARE(current.globs.first().pattern, QStringLiteral("*.png"));
}

void TestPortalFileChooser::currentFilterOutsideTheList()
{
    const QVariantMap options{{QStringLiteral("current_folder"), folderBytes()},
                              {QStringLiteral("current_filter"), oneFilter(QStringLiteral("Mine"), {{1, QStringLiteral("text/plain")}})}};
    QDBusPendingCall pending = start(QStringLiteral("OpenFile"), QStringLiteral("Test"), options);
    FileDialog *d = waitForTopLevel<FileDialog>();
    QVERIFY(d);
    QCOMPARE(d->types().size(), 1);
    QCOMPARE(d->currentType(), 0);
    QCOMPARE(d->types().first().mimeTypes, QStringList{QStringLiteral("text/plain")});
    QTRY_COMPARE(d->shownNames(), (QStringList{QStringLiteral("a.txt"), QStringLiteral("b.txt"), QStringLiteral("sub")}));
    d->reject();
    QTRY_VERIFY(pending.isFinished());
}

void TestPortalFileChooser::acceptLabelAndTitle()
{
    QDBusPendingCall pending = start(QStringLiteral("OpenFile"), QStringLiteral("Attach a file"), {{QStringLiteral("accept_label"), QStringLiteral("_Attach")}});
    FileDialog *d = waitForTopLevel<FileDialog>();
    QVERIFY(d);
    QCOMPARE(d->windowTitle(), QStringLiteral("Attach a file"));
    QCOMPARE(d->acceptButton()->text(), QStringLiteral("Attach"));
    d->reject();
    QTRY_VERIFY(pending.isFinished());
}

void TestPortalFileChooser::cancelAnswersCancelled()
{
    const QVariantList answer = call(QStringLiteral("OpenFile"), {}, [] {
        FileDialog *d = waitForTopLevel<FileDialog>();
        QVERIFY(d);
        d->cancelButton()->click();
    });
    QCOMPARE(answer.value(0).toUInt(), Raye::Portal::responseCancelled);
    QVERIFY(urisIn(answer).isEmpty());
}

void TestPortalFileChooser::closingTheRequestAnswersCancelled()
{
    QDBusPendingCall pending = start(QStringLiteral("OpenFile"), QStringLiteral("Test"), {});
    QVERIFY(waitForTopLevel<FileDialog>());
    QDBusMessage close = QDBusMessage::createMethodCall(service, handle, QStringLiteral("org.freedesktop.impl.portal.Request"), QStringLiteral("Close"));
    m_client.asyncCall(close);
    QTRY_VERIFY(pending.isFinished());
    QCOMPARE(pending.reply().arguments().value(0).toUInt(), Raye::Portal::responseCancelled);
}

void TestPortalFileChooser::saveFileWithCurrentName()
{
    const QVariantList answer = call(QStringLiteral("SaveFile"), {{QStringLiteral("current_folder"), folderBytes()}, {QStringLiteral("current_name"), QStringLiteral("new.txt")}}, [] {
        FileDialog *d = waitForTopLevel<FileDialog>();
        QVERIFY(d);
        QCOMPARE(d->fieldText(), QStringLiteral("new.txt"));
        QVERIFY(d->tryAccept());
    });
    QCOMPARE(answer.value(0).toUInt(), Raye::Portal::responseSuccess);
    QCOMPARE(urisIn(answer), QStringList{QUrl::fromLocalFile(path(QStringLiteral("new.txt"))).toString()});
}

void TestPortalFileChooser::saveAddsTheTypeExtension()
{
    const QVariantMap options{{QStringLiteral("current_folder"), folderBytes()},
                              {QStringLiteral("current_name"), QStringLiteral("note")},
                              {QStringLiteral("filters"), filterList({{QStringLiteral("Text"), {{0, QStringLiteral("*.txt")}}}})}};
    const QVariantList answer = call(QStringLiteral("SaveFile"), options, [] {
        FileDialog *d = waitForTopLevel<FileDialog>();
        QVERIFY(d);
        QVERIFY(d->tryAccept());
    });
    QCOMPARE(urisIn(answer), QStringList{QUrl::fromLocalFile(path(QStringLiteral("note.txt"))).toString()});
}

void TestPortalFileChooser::saveExtensionWithDotInFolder()
{
    QVERIFY(QDir(m_dir->path()).mkdir(QStringLiteral("folder.v1")));
    const QVariantMap options{{QStringLiteral("current_folder"), folderBytes()},
                              {QStringLiteral("current_name"), QStringLiteral("folder.v1/report")},
                              {QStringLiteral("filters"), filterList({{QStringLiteral("PDF"), {{0, QStringLiteral("*.pdf")}}}})}};
    const QVariantList answer = call(QStringLiteral("SaveFile"), options, [] {
        FileDialog *d = waitForTopLevel<FileDialog>();
        QVERIFY(d);
        QVERIFY(d->tryAccept());
    });
    QCOMPARE(urisIn(answer), QStringList{QUrl::fromLocalFile(path(QStringLiteral("folder.v1/report.pdf"))).toString()});
}

void TestPortalFileChooser::saveAsksBeforeReplacing()
{
    QStringList asked;
    const QVariantList answer = call(QStringLiteral("SaveFile"), {{QStringLiteral("current_folder"), folderBytes()}, {QStringLiteral("current_name"), QStringLiteral("a.txt")}}, [&asked] {
        FileDialog *d = waitForTopLevel<FileDialog>();
        QVERIFY(d);
        d->setConfirmOverwrite([&asked](const QStringList &existing) {
            asked = existing;
            return true;
        });
        QVERIFY(d->tryAccept());
    });
    QCOMPARE(asked, QStringList{QStringLiteral("a.txt")});
    QCOMPARE(answer.value(0).toUInt(), Raye::Portal::responseSuccess);
}

void TestPortalFileChooser::saveCancelledAtTheQuestionStaysOpen()
{
    QDBusPendingCall pending = start(QStringLiteral("SaveFile"), QStringLiteral("Test"), {{QStringLiteral("current_folder"), folderBytes()}, {QStringLiteral("current_name"), QStringLiteral("a.txt")}});
    FileDialog *d = waitForTopLevel<FileDialog>();
    QVERIFY(d);
    d->setConfirmOverwrite([](const QStringList &) {
        return false;
    });
    QVERIFY(!d->tryAccept());
    QVERIFY(d->isVisible());
    QVERIFY(!pending.isFinished());
    d->reject();
    QTRY_VERIFY(pending.isFinished());
}

void TestPortalFileChooser::saveFilesWritesEachNameInTheFolder()
{
    QByteArray one = QByteArrayLiteral("one.txt");
    one.append('\0');
    QByteArray two = QByteArrayLiteral("a.txt");
    two.append('\0');
    QStringList asked;
    const QVariantMap options{{QStringLiteral("current_folder"), folderBytes()}, {QStringLiteral("files"), QVariant::fromValue(QList<QByteArray>{one, two})}};
    const QVariantList answer = call(QStringLiteral("SaveFiles"), options, [&asked] {
        FileDialog *d = waitForTopLevel<FileDialog>();
        QVERIFY(d);
        d->setConfirmOverwrite([&asked](const QStringList &existing) {
            asked = existing;
            return true;
        });
        QVERIFY(d->tryAccept());
    });
    QCOMPARE(answer.value(0).toUInt(), Raye::Portal::responseSuccess);
    QCOMPARE(asked, QStringList{QStringLiteral("a.txt")});
    QCOMPARE(urisIn(answer), (QStringList{QUrl::fromLocalFile(path(QStringLiteral("one.txt"))).toString(), QUrl::fromLocalFile(path(QStringLiteral("a.txt"))).toString()}));
}

void TestPortalFileChooser::choicesComeBack()
{
    const QVariantMap options{{QStringLiteral("current_folder"), folderBytes()}, {QStringLiteral("current_name"), QStringLiteral("new.txt")}, {QStringLiteral("choices"), choicesOption()}};
    const QVariantList answer = call(QStringLiteral("SaveFile"), options, [] {
        FileDialog *d = waitForTopLevel<FileDialog>();
        QVERIFY(d);
        const auto values = d->choiceValues();
        QCOMPARE(values.value(QStringLiteral("encoding")), QStringLiteral("latin"));
        QCOMPARE(values.value(QStringLiteral("keep")), QStringLiteral("true"));
        QVERIFY(d->tryAccept());
    });
    QCOMPARE(answer.value(0).toUInt(), Raye::Portal::responseSuccess);
    const auto chosen = qdbus_cast<QList<AppOption>>(resultsIn(answer).value(QStringLiteral("choices")));
    QHash<QString, QString> values;
    for (const AppOption &pair : chosen) {
        values.insert(pair.id, pair.label);
    }
    QCOMPARE(values.value(QStringLiteral("encoding")), QStringLiteral("latin"));
    QCOMPARE(values.value(QStringLiteral("keep")), QStringLiteral("true"));
}

void TestPortalFileChooser::folderTypedInTheFieldIsEntered()
{
    QDBusPendingCall pending = start(QStringLiteral("OpenFile"), QStringLiteral("Test"), {{QStringLiteral("current_folder"), folderBytes()}});
    FileDialog *d = waitForTopLevel<FileDialog>();
    QVERIFY(d);
    d->setFieldText(QStringLiteral("sub"));
    QVERIFY(!d->tryAccept());
    QTRY_COMPARE(d->folder(), QUrl::fromLocalFile(path(QStringLiteral("sub"))));
    QVERIFY(d->fieldText().isEmpty());
    d->reject();
    QTRY_VERIFY(pending.isFinished());
}

void TestPortalFileChooser::searchNarrowsTheFolder()
{
    QDBusPendingCall pending = start(QStringLiteral("OpenFile"), QStringLiteral("Test"), {{QStringLiteral("current_folder"), folderBytes()}});
    FileDialog *d = waitForTopLevel<FileDialog>();
    QVERIFY(d);
    QTRY_COMPARE(d->shownNames().size(), 4);
    auto *search = d->findChild<QLineEdit *>(QStringLiteral("RayeFileDialogSearch"));
    QVERIFY(search);
    search->setText(QStringLiteral("a.t"));
    QTRY_COMPARE(d->shownNames(), QStringList{QStringLiteral("a.txt")});
    // Going elsewhere starts over.
    d->setFolder(QUrl::fromLocalFile(path(QStringLiteral("sub"))));
    QTRY_VERIFY(search->text().isEmpty());
    d->reject();
    QTRY_VERIFY(pending.isFinished());
}

void TestPortalFileChooser::sizeIsRemembered()
{
    QDBusPendingCall first = start(QStringLiteral("OpenFile"), QStringLiteral("Test"), {});
    FileDialog *d = waitForTopLevel<FileDialog>();
    QVERIFY(d);
    d->resize(913, 642);
    QTRY_COMPARE(d->size(), QSize(913, 642));
    d->cancelButton()->click();
    QTRY_VERIFY(first.isFinished());
    QTRY_COMPARE(Raye::Portal::openDialogs(), 0);

    QDBusPendingCall second = start(QStringLiteral("OpenFile"), QStringLiteral("Test"), {});
    d = waitForTopLevel<FileDialog>();
    QVERIFY(d);
    QCOMPARE(d->size(), QSize(913, 642));
    d->resize(800, 560);
    d->cancelButton()->click();
    QTRY_VERIFY(second.isFinished());
}

void TestPortalFileChooser::appChooserAnswersTheChoice()
{
    const QString apps = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QVERIFY(QDir().mkpath(apps));
    QFile desktop(apps + QStringLiteral("/org.raye.test.viewer.desktop"));
    QVERIFY(desktop.open(QIODevice::WriteOnly));
    desktop.write("[Desktop Entry]\nType=Application\nName=Viewer\nExec=true %U\nMimeType=text/plain;\n");
    desktop.close();
    QTest::qWait(1600);
    KSycoca::self()->ensureCacheValid();
    QVERIFY(KService::serviceByStorageId(QStringLiteral("org.raye.test.viewer.desktop")));

    QDBusMessage message = QDBusMessage::createMethodCall(service, objectPath, QStringLiteral("org.freedesktop.impl.portal.AppChooser"), QStringLiteral("ChooseApplication"));
    message << QVariant::fromValue(QDBusObjectPath(handle)) << QStringLiteral("org.test.App") << QString() << QStringList{QStringLiteral("org.raye.test.viewer")}
            << QVariantMap{{QStringLiteral("content_type"), QStringLiteral("text/plain")}, {QStringLiteral("filename"), path(QStringLiteral("a.txt"))}};
    QDBusPendingCall pending = m_client.asyncCall(message, 60000);
    auto *d = waitForTopLevel<Raye::Ui::CardDialog>();
    QVERIFY(d);
    QVERIFY(d->button(Raye::Ui::CardDialog::Secondary));
    d->button(Raye::Ui::CardDialog::Secondary)->click();
    QTRY_VERIFY(pending.isFinished());
    QVERIFY2(!pending.isError(), qPrintable(pending.error().message()));
    const QVariantList answer = pending.reply().arguments();
    QCOMPARE(answer.value(0).toUInt(), Raye::Portal::responseSuccess);
    QCOMPARE(resultsIn(answer).value(QStringLiteral("choice")).toString(), QStringLiteral("org.raye.test.viewer"));
    desktop.remove();
}

void TestPortalFileChooser::appChooserCancel()
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, objectPath, QStringLiteral("org.freedesktop.impl.portal.AppChooser"), QStringLiteral("ChooseApplication"));
    message << QVariant::fromValue(QDBusObjectPath(handle)) << QStringLiteral("org.test.App") << QString() << QStringList{}
            << QVariantMap{{QStringLiteral("filename"), path(QStringLiteral("a.txt"))}};
    QDBusPendingCall pending = m_client.asyncCall(message, 60000);
    auto *d = waitForTopLevel<Raye::Ui::CardDialog>();
    QVERIFY(d);
    d->button(Raye::Ui::CardDialog::Close)->click();
    QTRY_VERIFY(pending.isFinished());
    QCOMPARE(pending.reply().arguments().value(0).toUInt(), Raye::Portal::responseCancelled);
}

QTEST_MAIN(TestPortalFileChooser)

#include "test_portal_filechooser.moc"
