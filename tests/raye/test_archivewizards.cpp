/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The archive wizards (explorer spec 1.12, 3.12): what Extract all and
// Additional options offer by default, what they refuse and how they say
// it, and the three QML pages loading with their models.

#include "archive/archivewizardmodels.h"

#include <QDir>
#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <functional>

#include <unistd.h>

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
#include <KLocalizedContext>
QT_WARNING_POP

using namespace Raye::Archive;

class ArchiveWizardsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void extractDefaults();
    void extractValidation();
    void compressDefaults();
    void compressFormats();
    void compressValidation();
    void compressMemoryFollowsSize();
    void pagesLoad_data();
    void pagesLoad();

private:
    QTemporaryDir m_temp;
};

void ArchiveWizardsTest::initTestCase()
{
    QVERIFY(m_temp.isValid());
    registerQmlTypes();
}

void ArchiveWizardsTest::extractDefaults()
{
    ExtractAllModel model;
    QSignalSpy destination(&model, &ExtractAllModel::destinationChanged);
    model.setArchivePath(QStringLiteral("/home/ana/Downloads/Holiday photos.tar.gz"));
    QCOMPARE(model.destination(), QStringLiteral("/home/ana/Downloads/Holiday photos"));
    QCOMPARE(model.archiveName(), QStringLiteral("Holiday photos.tar.gz"));
    QVERIFY(model.showWhenDone());
    QCOMPARE(destination.size(), 1);
    QCOMPARE(ExtractAllModel::defaultDestination(QStringLiteral("/tmp/a.zip")), QStringLiteral("/tmp/a"));
}

void ArchiveWizardsTest::extractValidation()
{
    ExtractAllModel model;
    model.setArchivePath(m_temp.filePath(QStringLiteral("Holiday.zip")));
    // a folder that does not exist yet is fine: it is made
    QVERIFY(model.validate());
    QCOMPARE(model.errorText(), QString());
    QCOMPARE(model.resolvedDestination(), m_temp.filePath(QStringLiteral("Holiday")));

    // relative: read from the archive's folder; "~" is the home folder
    model.setDestination(QStringLiteral("sub/out"));
    QCOMPARE(model.resolvedDestination(), m_temp.filePath(QStringLiteral("sub/out")));
    model.setDestination(QStringLiteral("~/Extracted"));
    QCOMPARE(model.resolvedDestination(), QDir::homePath() + QStringLiteral("/Extracted"));

    model.setDestination(QStringLiteral("  "));
    QSignalSpy error(&model, &ExtractAllModel::errorTextChanged);
    QVERIFY(!model.validate());
    QCOMPARE(model.errorText(), QStringLiteral("Enter a folder to extract the files to."));
    QCOMPARE(error.size(), 1);

    QFile file(m_temp.filePath(QStringLiteral("taken")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    model.setDestination(file.fileName());
    // typing clears the old message
    QCOMPARE(model.errorText(), QString());
    QVERIFY(!model.validate());
    QCOMPARE(model.errorText(), QStringLiteral("There's already a file with this name. Choose another folder."));
    model.setDestination(file.fileName() + QStringLiteral("/below"));
    QVERIFY(!model.validate());

    if (::geteuid() != 0) {
        const QString locked = m_temp.filePath(QStringLiteral("locked"));
        QVERIFY(QDir().mkpath(locked));
        QVERIFY(QFile::setPermissions(locked, QFile::ReadOwner | QFile::ExeOwner));
        model.setDestination(locked + QStringLiteral("/new"));
        QVERIFY(!model.validate());
        QCOMPARE(model.errorText(), QStringLiteral("You don't have permission to extract to this folder."));
        QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
}

void ArchiveWizardsTest::compressDefaults()
{
    const QString folder = m_temp.filePath(QStringLiteral("docs"));
    QVERIFY(QDir().mkpath(folder + QStringLiteral("/Photos")));
    QFile report(folder + QStringLiteral("/Report.docx"));
    QVERIFY(report.open(QIODevice::WriteOnly));
    report.close();

    CompressOptionsModel model;
    model.setSources({report.fileName(), folder + QStringLiteral("/Photos")});
    QCOMPARE(model.name(), QStringLiteral("Report"));
    QCOMPARE(model.folder(), folder);
    QCOMPARE(model.fileName(), QStringLiteral("Report.zip"));
    QCOMPARE(model.archivePath(), folder + QStringLiteral("/Report.zip"));
    QCOMPARE(model.format(), int(Format::Zip));
    QCOMPARE(model.level(), int(Level::Normal));
    QVERIFY(!model.keepSymlinks());

    // a name already taken: the next free one, like Windows
    QFile taken(folder + QStringLiteral("/Report.zip"));
    QVERIFY(taken.open(QIODevice::WriteOnly));
    taken.close();
    model.setSources({report.fileName()});
    QCOMPARE(model.name(), QStringLiteral("Report (2)"));

    // a folder keeps its whole name
    model.setSources({folder + QStringLiteral("/Photos")});
    QCOMPARE(model.name(), QStringLiteral("Photos"));
}

void ArchiveWizardsTest::compressFormats()
{
    CompressOptionsModel model;
    QSignalSpy changed(&model, &CompressOptionsModel::changed);
    model.setFormat(int(Format::SevenZip));
    QCOMPARE(model.extension(), QStringLiteral(".7z"));
    QVERIFY(model.levelEnabled());
    QVERIFY(!model.isTar());

    model.setLevel(int(Level::Store));
    model.setFormat(int(Format::Tar));
    QVERIFY(model.isTar());
    // TAR has no Store: it becomes Normal, and a TAR picked here is a tar.zst
    QCOMPARE(model.level(), int(Level::Normal));
    QVERIFY(model.levelEnabled());
    QCOMPARE(model.extension(), QStringLiteral(".tar.zst"));
    QCOMPARE(model.options().compressionLevel(), 5);
    // with no compression there is no level to choose
    model.setTarFilter(int(TarFilter::None));
    QVERIFY(!model.levelEnabled());
    QCOMPARE(model.extension(), QStringLiteral(".tar"));
    model.setTarFilter(int(TarFilter::Zstd));

    // out of range values are clamped, never stored as nonsense
    model.setFormat(7);
    QCOMPARE(model.format(), int(Format::Tar));
    model.setLevel(-3);
    QCOMPARE(model.level(), int(Level::Store));
    model.setTarFilter(99);
    QCOMPARE(model.tarFilter(), int(TarFilter::Zstd));
    QVERIFY(changed.size() >= 4);
}

void ArchiveWizardsTest::compressMemoryFollowsSize()
{
    const QString folder = m_temp.filePath(QStringLiteral("memory"));
    QVERIFY(QDir().mkpath(folder));
    QFile small(folder + QStringLiteral("/small.txt"));
    QVERIFY(small.open(QIODevice::WriteOnly));
    small.write(QByteArray(200 * 1024, 'r'));
    small.close();

    CompressOptionsModel model;
    model.setSources({small.fileName()});
    model.setFormat(int(Format::SevenZip));
    model.setLevel(int(Level::Ultra));
    // 200 KB never needs the 64 MB dictionary, nor every core
    QVERIFY(model.openMemory() < 8 * 1024 * 1024);
    QVERIFY(model.compressMemory() < 128 * 1024 * 1024);
    QVERIFY(model.estimate().contains(QStringLiteral(" in 1 file")));

    // a store archive costs next to nothing to open
    model.setLevel(int(Level::Store));
    QCOMPARE(model.openMemory(), qint64(2 * 1024 * 1024));
}

void ArchiveWizardsTest::compressValidation()
{
    const QString folder = m_temp.filePath(QStringLiteral("valid"));
    QVERIFY(QDir().mkpath(folder));
    QFile source(folder + QStringLiteral("/a.txt"));
    QVERIFY(source.open(QIODevice::WriteOnly));
    source.close();

    CompressOptionsModel model;
    model.setSources({source.fileName()});
    QVERIFY(model.validate());
    QCOMPARE(model.errorText(), QString());

    model.setName(QStringLiteral("   "));
    QVERIFY(!model.validate());
    QCOMPARE(model.errorText(), QStringLiteral("Enter a name for the archive."));

    model.setName(QStringLiteral("a/b"));
    QVERIFY(!model.validate());
    QCOMPARE(model.errorText(), QStringLiteral("This name can't be used. Choose another one."));

    model.setName(QStringLiteral("fine"));
    model.setFolder(folder + QStringLiteral("/missing"));
    QVERIFY(!model.validate());
    QCOMPARE(model.errorText(), QStringLiteral("This folder doesn't exist."));

    model.setFolder(folder);
    QFile clash(folder + QStringLiteral("/fine.zip"));
    QVERIFY(clash.open(QIODevice::WriteOnly));
    clash.close();
    QVERIFY(!model.validate());
    QCOMPARE(model.errorText(), QStringLiteral("There's already a file with this name."));
    model.setFormat(int(Format::SevenZip));
    QVERIFY(model.validate());
    QCOMPARE(model.archivePath(), folder + QStringLiteral("/fine.7z"));
}

void ArchiveWizardsTest::pagesLoad_data()
{
    QTest::addColumn<QString>("type");
    QTest::newRow("extract") << QStringLiteral("ExtractAllPage");
    QTest::newRow("compress") << QStringLiteral("CompressOptionsPage");
    QTest::newRow("password") << QStringLiteral("ArchivePasswordPage");
}

void ArchiveWizardsTest::pagesLoad()
{
    QFETCH(QString, type);
    QQmlEngine engine;
    engine.addImportPath(qEnvironmentVariable("RAYE_FILES_BINARY_DIR", QStringLiteral(RAYE_FILES_BINARY_DIR)) + QStringLiteral("/bin"));
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    auto *context = new KLocalizedContext(&engine);
    QT_WARNING_POP
    context->setTranslationDomain(QStringLiteral("raye-files"));
    engine.rootContext()->setContextObject(context);

    QQmlComponent component(&engine);
    component.loadFromModule("org.raye.files", type);
    if (component.isError() && component.errorString().contains(QLatin1String("module \"org.raye.files\" is not installed"))
        && !qEnvironmentVariableIsSet("RAYE_TESTS_STRICT")) {
        QSKIP("the org.raye.files module is not built here");
    }
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(page.get());
    QVERIFY(item);
    // Layouts settle in the window's polish: the page needs one.
    QQuickWindow window;
    window.resize(520, 600);
    item->setParentItem(window.contentItem());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCOMPARE(item->implicitWidth(), type == QLatin1String("CompressOptionsPage") ? 672.0 : 472.0);
    QVERIFY(item->implicitHeight() > 40);
    QVERIFY(item->metaObject()->indexOfMethod("focusField()") >= 0);
    QVERIFY(item->metaObject()->indexOfSignal("accepted()") >= 0);

    if (type == QLatin1String("ExtractAllPage")) {
        auto *model = new ExtractAllModel(page.get());
        model->setArchivePath(QStringLiteral("/home/ana/x.zip"));
        QVERIFY(item->setProperty("model", QVariant::fromValue<QObject *>(model)));
        QCOMPARE(item->property("model").value<QObject *>(), model);
        // the error line appears when the model has one: the page grows
        QTest::qWait(50);
        const qreal before = item->implicitHeight();
        model->setDestination(QString());
        model->validate();
        QTRY_VERIFY(item->implicitHeight() > before);
    } else if (type == QLatin1String("CompressOptionsPage")) {
        auto *model = new CompressOptionsModel(page.get());
        QVERIFY(item->setProperty("model", QVariant::fromValue<QObject *>(model)));
        QVERIFY(item->setProperty("advanced", true));
        const std::function<bool(QQuickItem *, const QString &)> shows = [&shows](QQuickItem *at, const QString &text) {
            if (at->isVisible() && at->property("text").toString() == text) {
                return true;
            }
            const auto children = at->childItems();
            return std::any_of(children.begin(), children.end(), [&](QQuickItem *c) {
                return shows(c, text);
            });
        };
        QTRY_VERIFY(shows(item, QStringLiteral("Method")));
        model->setFormat(int(Format::Tar));
        // TAR shows its Compression row in place of the method
        QTRY_VERIFY(shows(item, QStringLiteral("Compression")));
        QVERIFY(!shows(item, QStringLiteral("Method")));
    } else {
        QVERIFY(item->setProperty("archiveName", QStringLiteral("a.zip")));
        QVERIFY(item->setProperty("wrongPassword", true));
        QCOMPARE(item->property("password").toString(), QString());
    }
}

QTEST_MAIN(ArchiveWizardsTest)

#include "test_archivewizards.moc"
