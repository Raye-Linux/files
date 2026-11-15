/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// First run of Files (explorer spec 3.5, 3.14, 3.18): the shared places list
// made like Windows' Quick access, the Recycle Bin defaults, Templates and
// Public out of sight, all of it once per account; and the system defaults
// in config/ checked against the keys Dolphin really reads.

#include "firstrun/firstrun.h"

#include <KConfig>
#include <KConfigGroup>
#include <KFilePlacesModel>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace
{
// Everything the test touches lives here: home, data and config folders.
QTemporaryDir *sandbox()
{
    static QTemporaryDir dir;
    return &dir;
}

QString home()
{
    return sandbox()->path() + QStringLiteral("/home");
}

QString placesFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/user-places.xbel");
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void writeText(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(text);
}

QUrl folder(QStandardPaths::StandardLocation location)
{
    return QUrl::fromLocalFile(QStandardPaths::writableLocation(location));
}

bool sameUrl(const QUrl &a, const QUrl &b)
{
    return a.matches(b, QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
}

int rowOf(const KFilePlacesModel &model, const QUrl &url)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        if (sameUrl(model.url(model.index(row, 0)), url)) {
            return row;
        }
    }
    return -1;
}

// The visible entries of the Places group, top to bottom.
QList<QUrl> visiblePlaces(const KFilePlacesModel &model)
{
    QList<QUrl> urls;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        if (model.groupType(index) == KFilePlacesModel::PlacesType && !model.isHidden(index)) {
            urls << model.url(index);
        }
    }
    return urls;
}

void dump(const KFilePlacesModel &model)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        qDebug() << row << model.groupType(index) << model.url(index).toString() << model.text(index)
                 << (model.isHidden(index) ? "hidden" : "") << (model.isDevice(index) ? "device" : "");
    }
}

// A stock places list, as KDE writes it the first time anything opens it.
void freshPlaces()
{
    QFile::remove(placesFile());
    KFilePlacesModel model;
    QVERIFY(model.rowCount() > 0);
    QVERIFY(QFile::exists(placesFile()));
}

const QList<QStandardPaths::StandardLocation> windowsOrder = {
    QStandardPaths::DesktopLocation,
    QStandardPaths::DownloadLocation,
    QStandardPaths::DocumentsLocation,
    QStandardPaths::PicturesLocation,
    QStandardPaths::MusicLocation,
    QStandardPaths::MoviesLocation,
};
}

class FirstRunTest : public QObject
{
    Q_OBJECT

public:
    // Before the application exists: nothing here may see the real home.
    static void initMain()
    {
        if (!sandbox()->isValid()) {
            qFatal("no temporary folder for the test home");
        }
        const QString root = sandbox()->path();
        qputenv("HOME", home().toUtf8());
        qputenv("XDG_DATA_HOME", (root + QStringLiteral("/data")).toUtf8());
        qputenv("XDG_CONFIG_HOME", (root + QStringLiteral("/config")).toUtf8());
        qputenv("XDG_CACHE_HOME", (root + QStringLiteral("/cache")).toUtf8());
        // No system dolphinrc from the machine running the tests.
        qputenv("XDG_CONFIG_DIRS", (root + QStringLiteral("/etc-xdg")).toUtf8());
        for (const char *name : {"Desktop", "Downloads", "Documents", "Pictures", "Music", "Videos", "Templates", "Public"}) {
            QDir().mkpath(home() + QLatin1Char('/') + QLatin1String(name));
        }
    }

private Q_SLOTS:
    void stockPlacesBecomeQuickAccess();
    void secondRunChangesNothing();
    void userPlacesAreKept();
    void runOnceIsGuarded();
    void trashDefaultsOnlyWhenUnset();
    void hiddenUserDirsFromUserDirsFile();
    void hiddenFileKeepsWhatIsThere();
    void systemDolphinrcUsesRealKeys();
    void automounterIsOff();
};

void FirstRunTest::stockPlacesBecomeQuickAccess()
{
    freshPlaces();
    {
        KFilePlacesModel model;
        qDebug() << "stock places:";
        dump(model);
        QVERIFY(Raye::FirstRun::migratePlaces(&model));
    }

    // A new model reads the file again: the change was saved, not only shown.
    KFilePlacesModel model;
    qDebug() << "after migration:";
    dump(model);

    QVERIFY(model.isGroupHidden(KFilePlacesModel::RemoteType));
    QVERIFY(model.isGroupHidden(KFilePlacesModel::RecentlySavedType));
    QVERIFY(model.isGroupHidden(KFilePlacesModel::SearchForType));
    QVERIFY(!model.isGroupHidden(KFilePlacesModel::PlacesType));
    QVERIFY(!model.isGroupHidden(KFilePlacesModel::RemovableDevicesType));

    for (const QUrl &url : {QUrl::fromLocalFile(home()), QUrl(QStringLiteral("trash:/")), QUrl(QStringLiteral("file:///"))}) {
        const int row = rowOf(model, url);
        if (row >= 0) {
            QVERIFY2(model.isHidden(model.index(row, 0)), qPrintable(url.toString()));
        }
    }
    // Home and the Recycle Bin are always in the stock list.
    QVERIFY(rowOf(model, QUrl::fromLocalFile(home())) >= 0);
    QVERIFY(rowOf(model, QUrl(QStringLiteral("trash:/"))) >= 0);

    const QList<QUrl> visible = visiblePlaces(model);
    QCOMPARE(visible.size(), windowsOrder.size());
    for (int i = 0; i < windowsOrder.size(); ++i) {
        QVERIFY2(sameUrl(visible.at(i), folder(windowsOrder.at(i))), qPrintable(visible.at(i).toString()));
    }
}

void FirstRunTest::secondRunChangesNothing()
{
    freshPlaces();
    {
        KFilePlacesModel model;
        QVERIFY(Raye::FirstRun::migratePlaces(&model));
    }
    const QByteArray before = readAll(placesFile());
    QVERIFY(!before.isEmpty());
    {
        KFilePlacesModel model;
        QVERIFY(!Raye::FirstRun::migratePlaces(&model));
    }
    QCOMPARE(readAll(placesFile()), before);
}

void FirstRunTest::userPlacesAreKept()
{
    freshPlaces();
    const QUrl projects = QUrl::fromLocalFile(home() + QStringLiteral("/Projects"));
    QDir().mkpath(projects.toLocalFile());
    {
        KFilePlacesModel model;
        model.addPlace(QStringLiteral("Projects"), projects);
        // The user moved Music to the top: the six still end in Windows' order.
        model.movePlace(rowOf(model, folder(QStandardPaths::MusicLocation)), 0);
        QVERIFY(Raye::FirstRun::migratePlaces(&model));
    }
    KFilePlacesModel model;
    const QList<QUrl> visible = visiblePlaces(model);
    QCOMPARE(visible.size(), windowsOrder.size() + 1);
    for (int i = 0; i < windowsOrder.size(); ++i) {
        QVERIFY(sameUrl(visible.at(i), folder(windowsOrder.at(i))));
    }
    QVERIFY(sameUrl(visible.last(), projects));
}

void FirstRunTest::runOnceIsGuarded()
{
    freshPlaces();
    const QString rcPath = sandbox()->path() + QStringLiteral("/config/dolphinrc");
    QFile::remove(rcPath);
    QFile::remove(home() + QStringLiteral("/.hidden"));
    QFile::remove(sandbox()->path() + QStringLiteral("/config/ktrashrc"));

    KSharedConfig::Ptr rc = KSharedConfig::openConfig(rcPath, KConfig::SimpleConfig);
    Raye::FirstRun::runOnce(rc);

    rc->reparseConfiguration();
    const KConfigGroup raye(rc, QStringLiteral("Raye"));
    QCOMPARE(raye.readEntry("PlacesMigrated", false), true);
    QCOMPARE(raye.readEntry("DefaultsVersion", 0), Raye::FirstRun::DefaultsVersion);
    {
        KFilePlacesModel model;
        QVERIFY(model.isGroupHidden(KFilePlacesModel::RemoteType));
    }

    // The trash group of this account and the .hidden list were written.
    KConfig ktrashrc(QStringLiteral("ktrashrc"), KConfig::SimpleConfig);
    const QString trashPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash");
    QCOMPARE(ktrashrc.group(trashPath).readEntry("LimitReachedAction", -1), 1);
    const QByteArray hidden = readAll(home() + QStringLiteral("/.hidden"));
    // No user-dirs.dirs in this home: nothing is known to be Templates.
    QVERIFY(hidden.isEmpty());

    // Once done, a stock list stays stock: the user may want KDE's places back.
    freshPlaces();
    const QByteArray stock = readAll(placesFile());
    Raye::FirstRun::runOnce(rc);
    QCOMPARE(readAll(placesFile()), stock);
}

void FirstRunTest::trashDefaultsOnlyWhenUnset()
{
    const QString path = sandbox()->path() + QStringLiteral("/trash-test-rc");
    QFile::remove(path);
    const QString trash = QStringLiteral("/home/someone/.local/share/Trash");
    {
        KConfig rc(path, KConfig::SimpleConfig);
        QVERIFY(Raye::FirstRun::applyTrashDefaults(rc, trash));
        const KConfigGroup group = rc.group(trash);
        QCOMPARE(group.readEntry("UseSizeLimit", false), true);
        QCOMPARE(group.readEntry("Percent", 0.0), 5.0);
        QCOMPARE(group.readEntry("LimitReachedAction", -1), 1);
        QVERIFY(!Raye::FirstRun::applyTrashDefaults(rc, trash));
    }
    // A user who chose something keeps it, even a single key.
    QFile::remove(path);
    {
        KConfig rc(path, KConfig::SimpleConfig);
        rc.group(trash).writeEntry("Percent", 20.0);
        QVERIFY(!Raye::FirstRun::applyTrashDefaults(rc, trash));
        QCOMPARE(rc.group(trash).readEntry("Percent", 0.0), 20.0);
        QVERIFY(!rc.group(trash).hasKey("LimitReachedAction"));
    }
    KConfig rc(path, KConfig::SimpleConfig);
    QVERIFY(!Raye::FirstRun::applyTrashDefaults(rc, QString()));
}

void FirstRunTest::hiddenUserDirsFromUserDirsFile()
{
    const QString h = home();
    // The file xdg-user-dirs writes, with comments and quotes.
    QCOMPARE(Raye::FirstRun::hiddenUserDirs(h,
                                            QStringLiteral("# written by xdg-user-dirs-update\n"
                                                           "XDG_DESKTOP_DIR=\"$HOME/Desktop\"\n"
                                                           "XDG_TEMPLATES_DIR=\"$HOME/Templates\"\n"
                                                           "XDG_PUBLICSHARE_DIR=\"$HOME/Public\"\n")),
             (QStringList{QStringLiteral("Templates"), QStringLiteral("Public")}));
    // Absolute paths and a trailing slash.
    QCOMPARE(Raye::FirstRun::hiddenUserDirs(h, QStringLiteral("XDG_TEMPLATES_DIR=\"%1/Templates/\"\n").arg(h)),
             QStringList{QStringLiteral("Templates")});
    // Moved out of sight already (GP-45: ~/.local/share/Templates), set to the
    // home itself (turned off), missing on disk, or outside the home: nothing.
    QVERIFY(Raye::FirstRun::hiddenUserDirs(h,
                                           QStringLiteral("XDG_TEMPLATES_DIR=\"$HOME/.local/share/Templates\"\n"
                                                          "XDG_PUBLICSHARE_DIR=\"$HOME/\"\n"))
                .isEmpty());
    QVERIFY(Raye::FirstRun::hiddenUserDirs(h, QStringLiteral("XDG_TEMPLATES_DIR=\"$HOME/Nowhere\"\n")).isEmpty());
    QVERIFY(Raye::FirstRun::hiddenUserDirs(h, QStringLiteral("XDG_PUBLICSHARE_DIR=\"/srv/Public\"\n")).isEmpty());
    // Nothing that climbs out of the home or breaks a line of .hidden.
    QVERIFY(Raye::FirstRun::hiddenUserDirs(h, QStringLiteral("XDG_TEMPLATES_DIR=\"$HOME/../home\"\n")).isEmpty());
    QVERIFY(Raye::FirstRun::hiddenUserDirs(h, QStringLiteral("XDG_TEMPLATES_DIR=\"$HOME/.\"\n")).isEmpty());
    // Other keys are not ours.
    QVERIFY(Raye::FirstRun::hiddenUserDirs(h, QStringLiteral("XDG_DESKTOP_DIR=\"$HOME/Desktop\"\n")).isEmpty());
    QVERIFY(Raye::FirstRun::hiddenUserDirs(h, QString()).isEmpty());
}

void FirstRunTest::hiddenFileKeepsWhatIsThere()
{
    const QString path = sandbox()->path() + QStringLiteral("/hidden-test/.hidden");
    QFile::remove(path);
    QVERIFY(Raye::FirstRun::addToHiddenFile(path, {QStringLiteral("Templates"), QStringLiteral("Public")}));
    QCOMPARE(readAll(path), QByteArray("Templates\nPublic\n"));
    QVERIFY(!Raye::FirstRun::addToHiddenFile(path, {QStringLiteral("Public")}));

    writeText(path, "snap\nPublic"); // no newline at the end
    QVERIFY(Raye::FirstRun::addToHiddenFile(path, {QStringLiteral("Templates"), QStringLiteral("Public")}));
    QCOMPARE(readAll(path), QByteArray("snap\nPublic\nTemplates\n"));

    // A name that is not a plain folder name is refused, the file untouched.
    QVERIFY(!Raye::FirstRun::addToHiddenFile(path, {QStringLiteral("a/b"), QStringLiteral("x\ny"), QString()}));
    QCOMPARE(readAll(path), QByteArray("snap\nPublic\nTemplates\n"));
    QVERIFY(!Raye::FirstRun::addToHiddenFile(path, {}));
}

void FirstRunTest::systemDolphinrcUsesRealKeys()
{
    const QString source = QStringLiteral(RAYE_FILES_SOURCE_DIR);
    const QString rcPath = source + QStringLiteral("/config/dolphinrc");
    QVERIFY(QFile::exists(rcPath));
    KConfig rc(rcPath, KConfig::SimpleConfig);

    // Every key of an upstream group must be one Dolphin declares, or it
    // would quietly do nothing.
    QSet<QString> known;
    const QDir settings(source + QStringLiteral("/src/settings"));
    static const QRegularExpression entry(QStringLiteral("<entry name=\"([^\"]+)\""));
    for (const QString &kcfg : settings.entryList({QStringLiteral("*.kcfg")})) {
        const QString text = QString::fromUtf8(readAll(settings.filePath(kcfg)));
        auto it = entry.globalMatch(text);
        while (it.hasNext()) {
            known.insert(it.next().captured(1));
        }
    }
    QVERIFY(known.size() > 50);
    // Read by code, not declared in a .kcfg.
    known.insert(QStringLiteral("Plugins"));

    for (const QString &group : rc.groupList()) {
        if (group == QLatin1String("Raye")) {
            continue;
        }
        for (const QString &key : rc.group(group).keyList()) {
            QVERIFY2(known.contains(key), qPrintable(group + QLatin1Char('/') + key));
        }
    }

    const KConfigGroup general = rc.group(QStringLiteral("General"));
    QCOMPARE(general.readEntry("RememberOpenedTabs", true), false);
    QCOMPARE(general.readEntry("BrowseThroughArchives", false), true);
    QCOMPARE(general.readEntry("ShowStatusBar", 0), 1); // full width
    QCOMPARE(general.readEntry("ShowSelectionToggle", true), false);
    QCOMPARE(general.readEntry("RenameInline", false), true);
    QCOMPARE(general.readEntry("SortingChoice", 1), 0);

    const QStringList previews = rc.group(QStringLiteral("PreviewSettings")).readEntry("Plugins", QStringList());
    QVERIFY(previews.contains(QStringLiteral("windowsexethumbnail")));
    QVERIFY(previews.contains(QStringLiteral("directorythumbnail")));
    QVERIFY(previews.contains(QStringLiteral("ffmpegthumbs")));
    QVERIFY(!previews.contains(QStringLiteral("textthumbnail"))); // Windows shows text files as icons

    const KConfigGroup details = rc.group(QStringLiteral("DetailsMode"));
    QCOMPARE(details.readEntry("PreviewSize", 0), 16);
    QCOMPARE(details.readEntry("ExpandableFolders", true), false);

    const KConfigGroup menu = rc.group(QStringLiteral("ContextMenu"));
    QVERIFY(!menu.keyList().isEmpty());
    for (const QString &key : menu.keyList()) {
        QCOMPARE(menu.readEntry(key, true), false);
    }

    const KConfigGroup raye = rc.group(QStringLiteral("Raye"));
    QCOMPARE(raye.readEntry("StartPage"), QStringLiteral("home"));
    QCOMPARE(raye.readEntry("ShowExtensions", true), false);
    // Not set: the decoration decides (TitleBar::readOwnTitleBar).
    QVERIFY(!raye.hasKey("OwnTitleBar"));
    QCOMPARE(raye.readEntry("PreviewPane"), QStringLiteral("none"));
    QCOMPARE(raye.readEntry("NavPaneWidth", 0), 240);
    QCOMPARE(raye.readEntry("PlacesMigrated", true), false);
    QCOMPARE(raye.readEntry("AutoPlayEnabled", false), true);
    QCOMPARE(raye.readEntry("AutoPlayRemovable"), QStringLiteral("ask"));
}

void FirstRunTest::automounterIsOff()
{
    KConfig rc(QStringLiteral(RAYE_FILES_SOURCE_DIR "/config/kded_device_automounterrc"), KConfig::SimpleConfig);
    QCOMPARE(rc.group(QStringLiteral("General")).readEntry("AutomountEnabled", true), false);
}

QTEST_MAIN(FirstRunTest)

#include "test_firstrun.moc"
