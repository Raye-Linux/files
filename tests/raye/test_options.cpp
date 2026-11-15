/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Folder options (explorer spec 3.15, keys of 3.18): what the dialog shows
// comes from the configuration, what it writes goes back to the right keys,
// and "Restore defaults" brings the system values back.

#include "raye/options/folderoptions.h"

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using Raye::Options::FolderOptions;

namespace
{
QTemporaryDir *s_system = nullptr;

QString userFile(const QString &name)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1Char('/') + name;
}

KConfigGroup userGroup(const QString &file, const QString &group)
{
    // Only the account's own file, not the system defaults under it.
    static std::unique_ptr<KConfig> config;
    config = std::make_unique<KConfig>(userFile(file), KConfig::SimpleConfig);
    return config->group(group);
}
}

class TestOptions : public QObject
{
    Q_OBJECT

public:
    // A system dolphinrc like the one Files installs in /etc/xdg.
    static void initMain()
    {
        s_system = new QTemporaryDir;
        QFile f(s_system->path() + QStringLiteral("/dolphinrc"));
        if (f.open(QIODevice::WriteOnly)) {
            f.write("[General]\nShowStatusBar=1\n\n[Raye]\nStartPage=home\nShowExtensions=false\nShowHidden=false\nShowRecent=true\nShowFrequent=true\nShowFavorites=true\n");
        }
        qputenv("XDG_CONFIG_DIRS", s_system->path().toLocal8Bit());
        // A home of its own: test mode writes under ~/.qttest, which other
        // test programs running at the same time share otherwise.
        qputenv("HOME", s_system->path().toLocal8Bit());
        qunsetenv("XDG_CONFIG_HOME");
        qunsetenv("XDG_DATA_HOME");
        qunsetenv("XDG_CACHE_HOME");
        QStandardPaths::setTestModeEnabled(true);
    }

private Q_SLOTS:
    void init();
    void cleanupTestCase();
    void readsSystemDefaults();
    void readsUserValues();
    void roundTrip();
    void writesOnlyWhatChanged();
    void singleClickInKdeglobals();
    void searchContentsInBaloo();
    void dirtyAndSignals();
    void restoreDefaults();
    void restoreDefaultsPerTab();
    void resetFolders();
    void clearHistoryHook();

private:
    FolderOptions::Files files() const;
    QTemporaryDir m_viewProps;
};

void TestOptions::init()
{
    for (const char *name : {"dolphinrc", "kdeglobals", "baloofilerc"}) {
        QFile::remove(userFile(QString::fromLatin1(name)));
    }
    KSharedConfig::openConfig(QStringLiteral("dolphinrc"), KConfig::NoGlobals)->reparseConfiguration();
    KSharedConfig::openConfig(QStringLiteral("kdeglobals"), KConfig::NoGlobals)->reparseConfiguration();
    KSharedConfig::openConfig(QStringLiteral("baloofilerc"), KConfig::NoGlobals)->reparseConfiguration();
}

void TestOptions::cleanupTestCase()
{
    for (const char *name : {"dolphinrc", "kdeglobals", "baloofilerc"}) {
        QFile::remove(userFile(QString::fromLatin1(name)));
    }
    delete s_system;
    s_system = nullptr;
}

FolderOptions::Files TestOptions::files() const
{
    FolderOptions::Files f;
    f.viewPropertiesDir = m_viewProps.filePath(QStringLiteral("view_properties"));
    return f;
}

void TestOptions::readsSystemDefaults()
{
    FolderOptions o(files());
    QCOMPARE(o.startPage(), QStringLiteral("home"));
    QVERIFY(o.hideExtensions());
    QVERIFY(!o.showHidden());
    QVERIFY(o.showStatusBar());
    QVERIFY(o.showRecent() && o.showFrequent() && o.showFavorites());
    QVERIFY(!o.singleClick());
    QVERIFY(o.searchContents());
    QVERIFY(!o.dirty());
}

void TestOptions::readsUserValues()
{
    KConfigGroup r = userGroup(QStringLiteral("dolphinrc"), QStringLiteral("Raye"));
    r.writeEntry("StartPage", "thispc");
    r.writeEntry("ShowExtensions", true);
    r.writeEntry("ItemCheckBoxes", true);
    KConfigGroup g = r.config()->group(QStringLiteral("General"));
    g.writeEntry("ShowStatusBar", 2);
    r.sync();
    FolderOptions o(files());
    QCOMPARE(o.startPage(), QStringLiteral("thispc"));
    QVERIFY(!o.hideExtensions());
    QVERIFY(o.itemCheckBoxes());
    QVERIFY(!o.showStatusBar());
}

void TestOptions::roundTrip()
{
    {
        FolderOptions o(files());
        o.setStartPage(QStringLiteral("thispc"));
        o.setOpenInOwnWindow(true);
        o.setShowRecent(false);
        o.setShowFrequent(false);
        o.setShowFavorites(false);
        o.setAlwaysShowIcons(true);
        o.setShowHidden(true);
        o.setHideExtensions(false);
        o.setShowStatusBar(false);
        o.setCompact(true);
        o.setShowAllFolders(true);
        o.setExpandToOpenFolder(true);
        o.setItemCheckBoxes(true);
        QVERIFY(o.dirty());
        QVERIFY(o.apply());
        QVERIFY(!o.dirty());
    }
    const KConfigGroup r = userGroup(QStringLiteral("dolphinrc"), QStringLiteral("Raye"));
    QCOMPARE(r.readEntry("StartPage"), QStringLiteral("thispc"));
    QCOMPARE(r.readEntry("OpenFoldersInOwnWindow", false), true);
    QCOMPARE(r.readEntry("ShowRecent", true), false);
    QCOMPARE(r.readEntry("ShowExtensions", false), true);
    QCOMPARE(r.readEntry("ShowHidden", false), true);
    QCOMPARE(r.readEntry("AlwaysShowIcons", false), true);
    QCOMPARE(r.readEntry("Compact", false), true);
    QCOMPARE(r.readEntry("ShowAllFolders", false), true);
    QCOMPARE(r.readEntry("ExpandToOpenFolder", false), true);
    QCOMPARE(r.readEntry("ItemCheckBoxes", false), true);
    QCOMPARE(r.config()->group(QStringLiteral("General")).readEntry("ShowStatusBar", 0), 2);

    // a new dialog shows what was written
    KSharedConfig::openConfig(QStringLiteral("dolphinrc"), KConfig::NoGlobals)->reparseConfiguration();
    FolderOptions again(files());
    QCOMPARE(again.startPage(), QStringLiteral("thispc"));
    QVERIFY(again.openInOwnWindow());
    QVERIFY(!again.showRecent() && !again.showFrequent() && !again.showFavorites());
    QVERIFY(again.alwaysShowIcons() && again.showHidden() && !again.hideExtensions());
    QVERIFY(!again.showStatusBar() && again.compact() && again.showAllFolders());
    QVERIFY(again.expandToOpenFolder() && again.itemCheckBoxes());

    // and back on: the status bar is the full-width one
    again.setShowStatusBar(true);
    QVERIFY(again.apply());
    QCOMPARE(userGroup(QStringLiteral("dolphinrc"), QStringLiteral("General")).readEntry("ShowStatusBar", 0), 1);
}

void TestOptions::writesOnlyWhatChanged()
{
    FolderOptions o(files());
    o.setShowHidden(true);
    QVERIFY(o.apply());
    const KConfigGroup r = userGroup(QStringLiteral("dolphinrc"), QStringLiteral("Raye"));
    QCOMPARE(r.keyList(), QStringList{QStringLiteral("ShowHidden")});
    QVERIFY(!userGroup(QStringLiteral("dolphinrc"), QStringLiteral("General")).exists());
    // nothing else was touched
    QVERIFY(!QFile::exists(userFile(QStringLiteral("kdeglobals"))));
    QVERIFY(!QFile::exists(userFile(QStringLiteral("baloofilerc"))));
}

void TestOptions::singleClickInKdeglobals()
{
    FolderOptions o(files());
    o.setSingleClick(true);
    QVERIFY(o.apply());
    QCOMPARE(userGroup(QStringLiteral("kdeglobals"), QStringLiteral("KDE")).readEntry("SingleClick", false), true);
    o.setSingleClick(false);
    QVERIFY(o.apply());
    QCOMPARE(userGroup(QStringLiteral("kdeglobals"), QStringLiteral("KDE")).readEntry("SingleClick", true), false);
}

void TestOptions::searchContentsInBaloo()
{
    FolderOptions o(files());
    o.setSearchContents(false);
    QVERIFY(o.apply());
    QCOMPARE(userGroup(QStringLiteral("baloofilerc"), QStringLiteral("General")).readEntry("only basic indexing", false), true);
    KSharedConfig::openConfig(QStringLiteral("baloofilerc"), KConfig::NoGlobals)->reparseConfiguration();
    FolderOptions again(files());
    QVERIFY(!again.searchContents());
}

void TestOptions::dirtyAndSignals()
{
    FolderOptions o(files());
    QSignalSpy changed(&o, &FolderOptions::changed);
    QSignalSpy applied(&o, &FolderOptions::applied);
    o.setCompact(false); // already false: nothing happens
    QCOMPARE(changed.count(), 0);
    o.setCompact(true);
    QCOMPARE(changed.count(), 1);
    QVERIFY(o.dirty());
    o.setCompact(false);
    QVERIFY(!o.dirty());
    o.setStartPage(QStringLiteral("nonsense"));
    QCOMPARE(o.startPage(), QStringLiteral("home"));
    o.setShowHidden(true);
    QVERIFY(o.apply());
    QCOMPARE(applied.count(), 1);
    // load() drops what was not applied
    o.setCompact(true);
    o.load();
    QVERIFY(!o.compact());
    QVERIFY(o.showHidden());
}

void TestOptions::restoreDefaults()
{
    FolderOptions o(files());
    o.setShowHidden(true);
    o.setStartPage(QStringLiteral("thispc"));
    QVERIFY(o.apply());
    o.restoreDefaults(FolderOptions::General);
    o.restoreDefaults(FolderOptions::View);
    QCOMPARE(o.startPage(), QStringLiteral("home"));
    QVERIFY(!o.showHidden());
    QVERIFY(o.dirty());
    QVERIFY(o.apply());
    KSharedConfig::openConfig(QStringLiteral("dolphinrc"), KConfig::NoGlobals)->reparseConfiguration();
    FolderOptions again(files());
    QCOMPARE(again.startPage(), QStringLiteral("home"));
    QVERIFY(!again.showHidden());
}

void TestOptions::restoreDefaultsPerTab()
{
    FolderOptions o(files());
    o.setStartPage(QStringLiteral("thispc"));
    o.setShowHidden(true);
    o.setSearchContents(false);
    o.restoreDefaults(FolderOptions::View);
    // only the View tab went back
    QCOMPARE(o.startPage(), QStringLiteral("thispc"));
    QVERIFY(!o.showHidden());
    QVERIFY(!o.searchContents());
    o.restoreDefaults(FolderOptions::Search);
    QVERIFY(o.searchContents());
}

void TestOptions::resetFolders()
{
    const QString dir = m_viewProps.filePath(QStringLiteral("view_properties/home/ann/Pictures"));
    QVERIFY(QDir().mkpath(dir));
    QFile f(dir + QStringLiteral("/.directory"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[Dolphin]\nViewMode=1\n");
    f.close();
    const QDateTime before = QDateTime::currentDateTime().addSecs(-1);
    FolderOptions o(files());
    QVERIFY(o.resetFolders());
    QVERIFY(!QFile::exists(m_viewProps.filePath(QStringLiteral("view_properties"))));
    const QDateTime stamp = userGroup(QStringLiteral("dolphinrc"), QStringLiteral("General")).readEntry("ViewPropsTimestamp", QDateTime());
    QVERIFY(stamp.isValid());
    QVERIFY(stamp >= before);
}

void TestOptions::clearHistoryHook()
{
    int calls = 0;
    FolderOptions::setClearHistoryHook([&calls] {
        ++calls;
    });
    FolderOptions o(files());
    o.clearHistory();
    QCOMPARE(calls, 1);
    FolderOptions::setClearHistoryHook({});
    o.clearHistory();
    QCOMPARE(calls, 1);
}

QTEST_MAIN(TestOptions)

#include "test_options.moc"
