/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// "Open with" (explorer spec 3.15): the apps listed for fixture types from
// a temporary XDG_DATA_DIRS, "Always" written to mimeapps.list, and a
// program browsed to.

#include "raye/menu/menuactions.h"
#include "raye/openwith/appchoices.h"
#include "raye/openwith/openwithdialog.h"
#include "raye/ui/carddialog.h"
#include "raye/ui/commandbutton.h"

#include <KApplicationTrader>
#include <KSycoca>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QPointer>
#include <QtMath>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTest>

using Raye::OpenWith::AppChoicesModel;

namespace
{
QTemporaryDir *s_data = nullptr;

// The app cache is rebuilt when mimeapps.list is newer than it, compared
// to the second, and it looks at most every 1.5 s: wait past both.
void refreshCache()
{
    QTest::qWait(1600);
    QFile list(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/mimeapps.list"));
    if (list.open(QIODevice::ReadWrite)) {
        list.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
        list.close();
    }
    KSycoca::self()->ensureCacheValid();
}

void writeDesktop(const QString &dir, const QString &id, const QByteArray &body)
{
    QFile f(dir + QLatin1Char('/') + id);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[Desktop Entry]\nType=Application\n" + body);
}

QStringList fixtureIds(const AppChoicesModel &m, bool recommended)
{
    QStringList ids;
    for (int i = 0; i < m.rowCount(); ++i) {
        const QModelIndex idx = m.index(i);
        const QString id = idx.data(AppChoicesModel::StorageIdRole).toString();
        if (id.startsWith(QLatin1String("rayetest-")) && idx.data(AppChoicesModel::RecommendedRole).toBool() == recommended) {
            ids.append(id);
        }
    }
    return ids;
}
}

class TestOpenWith : public QObject
{
    Q_OBJECT

public:
    // Before the application exists: the fixture apps come from a
    // temporary data folder put in front of the system ones (the MIME
    // database stays the system one).
    static void initMain()
    {
        s_data = new QTemporaryDir;
        const QString apps = s_data->path() + QStringLiteral("/applications");
        QDir().mkpath(apps);
        writeDesktop(apps, QStringLiteral("rayetest-editor.desktop"), "Name=Raye Test Editor\nExec=rayetest-editor %f\nMimeType=text/plain;\nIcon=accessories-text-editor\n");
        writeDesktop(apps, QStringLiteral("rayetest-viewer.desktop"), "Name=Raye Test Viewer\nExec=rayetest-viewer %U\nMimeType=text/plain;image/png;\n");
        writeDesktop(apps, QStringLiteral("rayetest-paint.desktop"), "Name=Raye Test Paint\nExec=rayetest-paint %F\nMimeType=image/png;\n");
        writeDesktop(apps, QStringLiteral("rayetest-hidden.desktop"), "Name=Raye Test Hidden\nExec=rayetest-hidden %f\nMimeType=text/plain;\nNoDisplay=true\n");
        writeDesktop(apps, QStringLiteral("rayetest-nofiles.desktop"), "Name=Raye Test Clock\nExec=rayetest-clock\n");
        qputenv("XDG_DATA_DIRS", (s_data->path() + QStringLiteral(":/usr/share")).toLocal8Bit());
        // A home of its own: test mode writes under ~/.qttest, which other
        // test programs running at the same time share otherwise.
        qputenv("HOME", s_data->path().toLocal8Bit());
        qunsetenv("XDG_CONFIG_HOME");
        qunsetenv("XDG_DATA_HOME");
        qunsetenv("XDG_CACHE_HOME");
        QStandardPaths::setTestModeEnabled(true);
    }

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void recommendedForType();
    void otherApps();
    void hiddenAndNoFiles();
    void alwaysWritesMimeapps();
    void currentFollowsDefault();
    void browsedProgram();
    void browsedProgramNames();
    void browsedRefusesNonPrograms();
    void unknownTypeForChooseApp();
    void dialogSizeIsSetBeforeItShows_data();
    void dialogSizeIsSetBeforeItShows();
    void dialogNeverOverflowsItsCard_data();
    void dialogNeverOverflowsItsCard();
    void dialogCancelGivesNoApp();
    void unknownFilesGoToTheRayeDialog_data();
    void unknownFilesGoToTheRayeDialog();
    void askingAppStaysOutOfTheLists();
    void preferredAppsComeFirst();
};

void TestOpenWith::initTestCase()
{
    // Each run starts from no user associations: an empty mimeapps.list,
    // as on a new account (the app cache watches the file once it exists).
    const QString mimeapps = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/mimeapps.list");
    QDir().mkpath(QFileInfo(mimeapps).path());
    QFile empty(mimeapps);
    QVERIFY(empty.open(QIODevice::WriteOnly | QIODevice::Truncate));
    empty.close();
    QDir(QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)).removeRecursively();
    // The build has no applications.menu; KDE's minimal one for tests.
    KSycoca::setupTestMenu();
    KSycoca::self()->ensureCacheValid();
    QVERIFY(KService::serviceByStorageId(QStringLiteral("rayetest-editor.desktop")));
}

void TestOpenWith::cleanupTestCase()
{
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/mimeapps.list"));
    QDir(QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)).removeRecursively();
    delete s_data;
    s_data = nullptr;
}

void TestOpenWith::recommendedForType()
{
    AppChoicesModel m;
    m.setMimeType(QStringLiteral("text/plain"));
    const QStringList rec = fixtureIds(m, true);
    QVERIFY2(rec.contains(QLatin1String("rayetest-editor.desktop")), qPrintable(rec.join(QLatin1Char(','))));
    QVERIFY(rec.contains(QLatin1String("rayetest-viewer.desktop")));
    QVERIFY(!rec.contains(QLatin1String("rayetest-paint.desktop")));
    QVERIFY(m.recommendedCount() >= 2);
    // recommended apps come before the others
    for (int i = 0; i < m.rowCount(); ++i) {
        QCOMPARE(m.index(i).data(AppChoicesModel::RecommendedRole).toBool(), i < m.recommendedCount());
    }
    m.setMimeType(QStringLiteral("image/png"));
    const QStringList png = fixtureIds(m, true);
    QVERIFY(png.contains(QLatin1String("rayetest-paint.desktop")));
    QVERIFY(png.contains(QLatin1String("rayetest-viewer.desktop")));
    QVERIFY(!png.contains(QLatin1String("rayetest-editor.desktop")));
}

void TestOpenWith::otherApps()
{
    AppChoicesModel m;
    m.setMimeType(QStringLiteral("image/png"));
    // the text editor opens files: it is among "More apps"
    QVERIFY(fixtureIds(m, false).contains(QLatin1String("rayetest-editor.desktop")));
    // no app twice
    QSet<QString> seen;
    for (int i = 0; i < m.rowCount(); ++i) {
        const QString id = m.index(i).data(AppChoicesModel::StorageIdRole).toString();
        QVERIFY2(!seen.contains(id), qPrintable(id));
        seen.insert(id);
    }
}

void TestOpenWith::hiddenAndNoFiles()
{
    AppChoicesModel m;
    m.setMimeType(QStringLiteral("text/plain"));
    for (bool rec : {true, false}) {
        const QStringList ids = fixtureIds(m, rec);
        QVERIFY(!ids.contains(QLatin1String("rayetest-hidden.desktop")));
        QVERIFY(!ids.contains(QLatin1String("rayetest-nofiles.desktop")));
    }
}

void TestOpenWith::alwaysWritesMimeapps()
{
    const KService::Ptr viewer = KService::serviceByStorageId(QStringLiteral("rayetest-viewer.desktop"));
    QVERIFY(viewer);
    QVERIFY(Raye::OpenWith::setAlways(QStringLiteral("text/plain"), viewer));
    QFile f(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/mimeapps.list"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(f.readAll());
    const int defaults = text.indexOf(QLatin1String("[Default Applications]"));
    QVERIFY2(defaults >= 0, qPrintable(text));
    const int line = text.indexOf(QLatin1String("text/plain=rayetest-viewer.desktop"), defaults);
    QVERIFY2(line > defaults, qPrintable(text));
    // and the system now opens text with it
    refreshCache();
    const KService::Ptr preferred = KApplicationTrader::preferredService(QStringLiteral("text/plain"));
    QVERIFY(preferred);
    QCOMPARE(preferred->storageId(), QStringLiteral("rayetest-viewer.desktop"));
    // nothing to remember without an app or a type
    QVERIFY(!Raye::OpenWith::setAlways(QString(), viewer));
    QVERIFY(!Raye::OpenWith::setAlways(QStringLiteral("text/plain"), KService::Ptr()));
}

void TestOpenWith::currentFollowsDefault()
{
    AppChoicesModel m;
    m.setMimeType(QStringLiteral("text/plain"));
    // set by the test before: the default is first, marked, and picked
    QCOMPARE(m.index(0).data(AppChoicesModel::StorageIdRole).toString(), QStringLiteral("rayetest-viewer.desktop"));
    QVERIFY(m.index(0).data(AppChoicesModel::DefaultRole).toBool());
    QCOMPARE(m.currentIndex(), 0);
    QCOMPARE(m.currentService()->storageId(), QStringLiteral("rayetest-viewer.desktop"));
    m.setCurrentIndex(1);
    QCOMPARE(m.currentIndex(), 1);
    m.setCurrentIndex(m.rowCount());
    QCOMPARE(m.currentIndex(), 1);
}

void TestOpenWith::browsedProgram()
{
    QTemporaryDir bin;
    const QString program = bin.filePath(QStringLiteral("My Tool $(x).sh"));
    {
        QFile f(program);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("#!/bin/sh\n");
    }
    QVERIFY(QFile::setPermissions(program, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    const KService::Ptr s = Raye::OpenWith::serviceForProgram(program, QStringLiteral("text/plain"));
    QVERIFY(s);
    QVERIFY(s->storageId().startsWith(QLatin1String("raye-openwith-")));
    QVERIFY2(!s->storageId().contains(QLatin1Char('$')) && !s->storageId().contains(QLatin1Char(' ')), qPrintable(s->storageId()));
    // the path stays one quoted program name, the file goes after it
    QVERIFY2(s->exec().startsWith(QLatin1Char('\'')) && s->exec().endsWith(QLatin1String(" %f")), qPrintable(s->exec()));
    QVERIFY(s->exec().contains(QLatin1String("My Tool $(x).sh")));

    AppChoicesModel m;
    m.setMimeType(QStringLiteral("text/plain"));
    const int before = m.rowCount();
    m.addBrowsed(s);
    QCOMPARE(m.currentIndex(), 0);
    QCOMPARE(m.currentService()->storageId(), s->storageId());
    QVERIFY(m.rowCount() >= before);
    // "Always" with it: next time it is listed for the type
    QVERIFY(Raye::OpenWith::setAlways(QStringLiteral("text/plain"), s));
    refreshCache();
    AppChoicesModel again;
    again.setMimeType(QStringLiteral("text/plain"));
    QCOMPARE(again.index(0).data(AppChoicesModel::StorageIdRole).toString(), s->storageId());
}

void TestOpenWith::browsedProgramNames()
{
    QTemporaryDir a;
    QTemporaryDir b;
    auto program = [](const QString &path) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write("#!/bin/sh\n");
            f.close();
        }
        f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        return path;
    };
    // a % in the path is never read as a field code
    const KService::Ptr percent = Raye::OpenWith::serviceForProgram(program(a.filePath(QStringLiteral("50%f.sh"))), QStringLiteral("text/plain"));
    QVERIFY(percent);
    QVERIFY2(percent->exec().contains(QLatin1String("50%%f.sh")), qPrintable(percent->exec()));
    // two programs of the same name each keep their own entry
    const KService::Ptr first = Raye::OpenWith::serviceForProgram(program(a.filePath(QStringLiteral("tool"))), QStringLiteral("text/plain"));
    const KService::Ptr second = Raye::OpenWith::serviceForProgram(program(b.filePath(QStringLiteral("tool"))), QStringLiteral("text/plain"));
    QVERIFY(first && second);
    QVERIFY(first->storageId() != second->storageId());
    QVERIFY(first->exec().contains(a.path()));
    QVERIFY(second->exec().contains(b.path()));
    // the same program again reuses its entry
    const KService::Ptr again = Raye::OpenWith::serviceForProgram(a.filePath(QStringLiteral("tool")), QStringLiteral("text/plain"));
    QCOMPARE(again->storageId(), first->storageId());
}

void TestOpenWith::browsedRefusesNonPrograms()
{
    QTemporaryDir dir;
    const QString plain = dir.filePath(QStringLiteral("notes.txt"));
    QFile f(plain);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();
    QVERIFY(!Raye::OpenWith::serviceForProgram(plain, QStringLiteral("text/plain")));
    QVERIFY(!Raye::OpenWith::serviceForProgram(dir.path(), QStringLiteral("text/plain")));
    QVERIFY(!Raye::OpenWith::serviceForProgram(dir.filePath(QStringLiteral("missing")), QStringLiteral("text/plain")));
}

void TestOpenWith::unknownTypeForChooseApp()
{
    QVERIFY(!Raye::OpenWith::chooseDefaultApp(QStringLiteral("no/such-type")));
}

void TestOpenWith::dialogSizeIsSetBeforeItShows_data()
{
    QTest::addColumn<QString>("mime");
    // No app for the type: every app of the system fills the list.
    QTest::newRow("no recommended app") << QStringLiteral("application/octet-stream");
    QTest::newRow("with recommended apps") << QStringLiteral("text/plain");
}

// On Wayland a window resized just after it is mapped can be put back at its
// first size by the compositor's first configure: the card then stays too
// short, the list runs over the button row and no button shows. So the size
// has to be right before the window exists, and stay.
void TestOpenWith::dialogSizeIsSetBeforeItShows()
{
    QFETCH(QString, mime);
    const QList<QUrl> urls{QUrl::fromLocalFile(QStringLiteral("/tmp/inconnu.qqq"))};
    QScopedPointer<Raye::Ui::CardDialog> dialog(Raye::OpenWith::openWithDialog(nullptr, urls, mime, [](const KService::Ptr &, bool) { }));
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);
    const QSize before = dialog->sizeHint();
    dialog->show();
    QVERIFY(QTest::qWaitForWindowExposed(dialog.data()));
    QTest::qWait(300);
    QCOMPARE(dialog->size(), before);
    QCOMPARE(dialog->sizeHint(), before);
    // The whole list of a busy system has to scroll, not to grow the card.
    QVERIFY2(before.height() <= 640, qPrintable(QString::number(before.height())));
}

void TestOpenWith::dialogNeverOverflowsItsCard_data()
{
    QTest::addColumn<QString>("name");
    QTest::newRow("short title") << QStringLiteral("/tmp/inconnu.qqq");
    // The French title of an ordinary file is about this long.
    QTest::newRow("title on two lines") << QStringLiteral("/tmp/inconnu.a-long-extension-that-will-take-two-lines-of-the-title");
}

void TestOpenWith::dialogNeverOverflowsItsCard()
{
    QFETCH(QString, name);
    const QList<QUrl> urls{QUrl::fromLocalFile(name)};
    QScopedPointer<Raye::Ui::CardDialog> dialog(
        Raye::OpenWith::openWithDialog(nullptr, urls, QStringLiteral("application/octet-stream"), [](const KService::Ptr &, bool) { }));
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);
    dialog->show();
    QVERIFY(QTest::qWaitForWindowExposed(dialog.data()));
    QTest::qWait(300);

    auto *view = qobject_cast<QQuickWidget *>(dialog->contentWidget());
    QVERIFY(view);
    QVERIFY(view->rootObject());
    // The card's 24 px of padding are still there under the page.
    QVERIFY2(view->geometry().bottom() + 24 < dialog->buttonRowTop() + 1,
             qPrintable(QStringLiteral("page ends at %1, buttons start at %2").arg(view->geometry().bottom()).arg(dialog->buttonRowTop())));
    QCOMPARE(view->height(), qCeil(view->rootObject()->implicitHeight()));
    // The list keeps its room, six rows, even with few apps to show.
    QVERIFY2(view->height() >= 240 + 100, qPrintable(QString::number(view->height())));

    for (auto role : {Raye::Ui::CardDialog::Primary, Raye::Ui::CardDialog::Secondary, Raye::Ui::CardDialog::Close}) {
        Raye::Ui::CommandButton *button = dialog->button(role);
        QVERIFY2(button, "the card has to offer Always, Just once and Cancel");
        QVERIFY(button->isVisible());
        const QRect inCard(button->mapTo(dialog.data(), QPoint(0, 0)), button->size());
        QVERIFY2(dialog->rect().contains(inCard), qPrintable(QStringLiteral("button at %1,%2").arg(inCard.x()).arg(inCard.y())));
        QVERIFY(!inCard.intersects(view->geometry()));
    }
}

void TestOpenWith::dialogCancelGivesNoApp()
{
    int calls = 0;
    KService::Ptr chosen;
    const QList<QUrl> urls{QUrl::fromLocalFile(QStringLiteral("/tmp/inconnu.qqq"))};
    auto *dialog = Raye::OpenWith::openWithDialog(nullptr, urls, QStringLiteral("text/plain"), [&calls, &chosen](const KService::Ptr &service, bool) {
        ++calls;
        chosen = service;
    });
    QPointer<Raye::Ui::CardDialog> guard(dialog);
    dialog->show();
    QVERIFY(QTest::qWaitForWindowExposed(dialog));
    QVERIFY(dialog->button(Raye::Ui::CardDialog::Close));
    QCOMPARE(dialog->button(Raye::Ui::CardDialog::Close)->text(), QStringLiteral("Cancel"));
    dialog->button(Raye::Ui::CardDialog::Close)->click();
    QTRY_VERIFY(!guard);
    QCOMPARE(calls, 1);
    QVERIFY(!chosen);
}

namespace
{
// The entry the package installs, as the app cache would read it.
void installAskingApp()
{
    QFile source(QStringLiteral(RAYE_FILES_SOURCE_DIR "/data/portal/org.raye.files.openwith.desktop.in"));
    QVERIFY(source.open(QIODevice::ReadOnly));
    QByteArray body = source.readAll();
    body.replace("@RAYE_PORTAL_BINARY@", "/bin/true");
    const QString apps = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QVERIFY(QDir().mkpath(apps));
    QFile out(apps + QStringLiteral("/org.raye.files.openwith.desktop"));
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(body);
    out.close();
    refreshCache();
}
}

void TestOpenWith::unknownFilesGoToTheRayeDialog_data()
{
    QTest::addColumn<QString>("mime");
    // What a file of unknown content is.
    QTest::newRow("no known type") << QStringLiteral("application/octet-stream");
    // Known to the system with no app on it: every binary type inherits
    // octet-stream, so it reaches the same place.
    QTest::newRow("disk image") << QStringLiteral("application/x-cd-image");
}

void TestOpenWith::unknownFilesGoToTheRayeDialog()
{
    QFETCH(QString, mime);
    installAskingApp();
    const KService::Ptr preferred = KApplicationTrader::preferredService(mime);
    QVERIFY2(preferred, qPrintable(mime));
    QCOMPARE(preferred->storageId(), QStringLiteral("org.raye.files.openwith.desktop"));
    QVERIFY(preferred->exec().contains(QLatin1String("--open-with")));
}

void TestOpenWith::askingAppStaysOutOfTheLists()
{
    installAskingApp();
    // A type with an app of its own keeps it: octet-stream only comes last.
    for (const QString &mime : {QStringLiteral("image/png"), QStringLiteral("text/plain")}) {
        const KService::Ptr preferred = KApplicationTrader::preferredService(mime);
        QVERIFY2(preferred, qPrintable(mime));
        QVERIFY2(preferred->storageId() != QLatin1String("org.raye.files.openwith.desktop"), qPrintable(mime));
    }
    // It must never offer itself: picking it would ask again, for ever.
    AppChoicesModel m;
    m.setMimeType(QStringLiteral("application/octet-stream"));
    for (int i = 0; i < m.rowCount(); ++i) {
        QVERIFY(m.index(i).data(AppChoicesModel::StorageIdRole).toString() != QLatin1String("org.raye.files.openwith.desktop"));
    }
    QMenu parent;
    QMenu *menu = Raye::Menu::openWithMenu(&parent, {}, QStringLiteral("application/octet-stream"), [] { });
    const auto actions = menu->actions();
    for (QAction *a : actions) {
        QVERIFY2(a->data().toString() != QLatin1String("org.raye.files.openwith.desktop"), qPrintable(a->text()));
    }
}

void TestOpenWith::preferredAppsComeFirst()
{
    AppChoicesModel m;
    m.setMimeType(QStringLiteral("text/plain"));
    const int recommended = m.recommendedCount();
    QString other;
    for (int i = recommended; i < m.rowCount() && other.isEmpty(); ++i) {
        other = m.index(i).data(AppChoicesModel::StorageIdRole).toString();
    }
    QVERIFY(!other.isEmpty());
    QString id = other;
    id.chop(int(qstrlen(".desktop")));
    m.prefer({id, QStringLiteral("no.such.app")});
    QCOMPARE(m.index(0).data(AppChoicesModel::StorageIdRole).toString(), other);
    QCOMPARE(m.currentIndex(), 0);
    QCOMPARE(m.recommendedCount(), recommended + 1);
    QVERIFY(m.index(0).data(AppChoicesModel::RecommendedRole).toBool());
}

QTEST_MAIN(TestOpenWith)

#include "test_openwith.moc"
