/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Home (explorer spec 3.9): the lists built from a fixture stats source
// and fixture pins, the Folder options privacy choices, the services the
// menus use, the pins on a temporary places list and the page host.

#include "raye/pages/homemodel.h"
#include "raye/pages/homesources.h"
#include "raye/pages/host.h"

#include <KConfigGroup>
#include <KFilePlacesModel>
#include <KRecentDocument>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QQuickWidget>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Raye::Pages;

namespace
{
QTemporaryDir *s_home = nullptr;

HomeResource res(const QString &path, int minutesAgo = 0, const QString &mime = QString())
{
    HomeResource r;
    r.url = QUrl::fromLocalFile(path);
    r.mimeType = mime;
    r.used = QDateTime(QDate(2026, 10, 31), QTime(12, 0)).addSecs(-60 * minutesAgo);
    return r;
}

QStringList names(const HomeListModel *m)
{
    QStringList out;
    for (int i = 0; i < m->rowCount(); ++i) {
        out.append(m->index(i).data(HomeListModel::NameRole).toString());
    }
    return out;
}
}

class TestHomeModel : public QObject
{
    Q_OBJECT

public:
    static void initMain()
    {
        // A home of its own: places, recent documents and settings of the
        // test never touch anyone else's.
        s_home = new QTemporaryDir;
        qputenv("HOME", s_home->path().toLocal8Bit());
        // KDE keeps no recent documents from the temporary folder: the
        // test's own one is inside its home, so its files are not "temporary".
        QDir().mkpath(s_home->path() + QStringLiteral("/tmp"));
        qputenv("TMPDIR", (s_home->path() + QStringLiteral("/tmp")).toLocal8Bit());
        qunsetenv("XDG_CONFIG_HOME");
        qunsetenv("XDG_DATA_HOME");
        qunsetenv("XDG_CACHE_HOME");
        QStandardPaths::setTestModeEnabled(true);
    }

private Q_SLOTS:
    void init();
    void cleanupTestCase();

    void quickAccessPinsThenFrequent();
    void recentAndShowMore();
    void continueStrip();
    void favorites();
    void privacySettings();
    void settingsApplyLive();
    void clearHistoryKeepsFavorites();
    void openSignals();
    void roles();
    void locationTexts_data();
    void locationTexts();
    void placesPins();
    void recentDocumentsSource();
    void pageHost();
    void sharedServices();

private:
    void writeSettings(const QMap<QString, bool> &values);
    StaticStatsSource m_stats;
    StaticPins m_pins;
    QString m_config;
};

void TestHomeModel::init()
{
    m_config = QStringLiteral("raye-test-homerc");
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1Char('/') + m_config);
    KSharedConfig::openConfig(m_config, KConfig::NoGlobals)->reparseConfiguration();
    const QString h = QDir::homePath();
    m_pins.list = {res(h + QStringLiteral("/Desktop")), res(h + QStringLiteral("/Downloads"))};
    m_stats.folders = {res(h + QStringLiteral("/Downloads")),
                       res(h + QStringLiteral("/Documents/Work")),
                       res(h + QStringLiteral("/Pictures/Holiday")),
                       res(h + QStringLiteral("/Music/Live")),
                       res(h + QStringLiteral("/Videos/Clips")),
                       res(h + QStringLiteral("/Projects"))};
    m_stats.recent.clear();
    for (int i = 0; i < 60; ++i) {
        m_stats.recent.append(res(h + QStringLiteral("/Documents/file%1.txt").arg(i), i));
    }
    m_stats.favs = {res(h + QStringLiteral("/Documents/file3.txt"), 3)};
    m_stats.forgetCalls = 0;
}

void TestHomeModel::cleanupTestCase()
{
    delete s_home;
    s_home = nullptr;
}

void TestHomeModel::writeSettings(const QMap<QString, bool> &values)
{
    KSharedConfig::Ptr c = KSharedConfig::openConfig(m_config, KConfig::NoGlobals);
    KConfigGroup g = c->group(QStringLiteral("Raye"));
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        g.writeEntry(it.key().toUtf8().constData(), it.value(), KConfig::Normal | KConfig::Notify);
    }
    c->sync();
}

void TestHomeModel::quickAccessPinsThenFrequent()
{
    HomeModel m(&m_stats, &m_pins);
    m.setConfigName(m_config);
    // two pins, then four frequent folders that are not pinned (Downloads is)
    QCOMPARE(names(m.quickAccessModel()),
             (QStringList{QStringLiteral("Desktop"), QStringLiteral("Downloads"), QStringLiteral("Work"), QStringLiteral("Holiday"), QStringLiteral("Live"), QStringLiteral("Clips")}));
    QCOMPARE(m.quickAccessModel()->index(0).data(HomeListModel::PinnedRole).toBool(), true);
    QCOMPARE(m.quickAccessModel()->index(2).data(HomeListModel::PinnedRole).toBool(), false);
    QCOMPARE(m.quickAccessModel()->index(2).data(HomeListModel::FolderRole).toBool(), true);
    // pinning a frequent folder moves it to the pins
    m.togglePin(QUrl::fromLocalFile(QDir::homePath() + QStringLiteral("/Documents/Work")));
    QCOMPARE(names(m.quickAccessModel()).mid(0, 3), (QStringList{QStringLiteral("Desktop"), QStringLiteral("Downloads"), QStringLiteral("Work")}));
    QCOMPARE(m.quickAccessModel()->index(2).data(HomeListModel::PinnedRole).toBool(), true);
    QVERIFY(names(m.quickAccessModel()).contains(QLatin1String("Projects")));
    QCOMPARE(m.quickAccessModel()->rowCount(), 3 + HomeModel::frequentCount);
}

void TestHomeModel::recentAndShowMore()
{
    HomeModel m(&m_stats, &m_pins);
    m.setConfigName(m_config);
    QCOMPARE(m.recentModel()->rowCount(), HomeModel::recentCount);
    QCOMPARE(names(m.recentModel()).first(), QStringLiteral("file0.txt"));
    QVERIFY(m.canShowMoreRecent());
    m.showMoreRecent();
    QCOMPARE(m.recentModel()->rowCount(), HomeModel::recentMoreCount);
    QVERIFY(!m.canShowMoreRecent());
    // few files: nothing more to show
    m_stats.recent = m_stats.recent.mid(0, 5);
    HomeModel few(&m_stats, &m_pins);
    few.setConfigName(m_config);
    QCOMPARE(few.recentModel()->rowCount(), 5);
    QVERIFY(!few.canShowMoreRecent());
}

void TestHomeModel::continueStrip()
{
    HomeModel m(&m_stats, &m_pins);
    m.setConfigName(m_config);
    QVERIFY(m.showContinue());
    QCOMPARE(names(m.continueModel()), (QStringList{QStringLiteral("file0.txt"), QStringLiteral("file1.txt"), QStringLiteral("file2.txt")}));
    writeSettings({{QStringLiteral("ShowContinue"), false}});
    m.reload();
    QVERIFY(!m.showContinue());
    QCOMPARE(m.continueModel()->rowCount(), 0);
    QCOMPARE(m.recentModel()->rowCount(), HomeModel::recentCount);
    // no recent files shown: no Continue either
    writeSettings({{QStringLiteral("ShowContinue"), true}, {QStringLiteral("ShowRecent"), false}});
    m.reload();
    QVERIFY(!m.showContinue());
    QCOMPARE(m.continueModel()->rowCount(), 0);
}

void TestHomeModel::favorites()
{
    HomeModel m(&m_stats, &m_pins);
    m.setConfigName(m_config);
    QCOMPARE(names(m.favoritesModel()), QStringList{QStringLiteral("file3.txt")});
    QCOMPARE(m.recentModel()->index(3).data(HomeListModel::FavoriteRole).toBool(), true);
    QCOMPARE(m.recentModel()->index(4).data(HomeListModel::FavoriteRole).toBool(), false);
    const QUrl f4 = m.recentModel()->index(4).data(HomeListModel::UrlRole).toUrl();
    m.toggleFavorite(f4);
    QCOMPARE(m.favoritesModel()->rowCount(), 2);
    QCOMPARE(m.recentModel()->index(4).data(HomeListModel::FavoriteRole).toBool(), true);
    m.toggleFavorite(f4);
    QCOMPARE(m.favoritesModel()->rowCount(), 1);
    QCOMPARE(m.recentModel()->index(4).data(HomeListModel::FavoriteRole).toBool(), false);
}

void TestHomeModel::privacySettings()
{
    writeSettings({{QStringLiteral("ShowRecent"), false}, {QStringLiteral("ShowFrequent"), false}, {QStringLiteral("ShowFavorites"), false}});
    HomeModel m(&m_stats, &m_pins);
    m.setConfigName(m_config);
    QVERIFY(!m.showRecent() && !m.showFrequent() && !m.showFavorites());
    QCOMPARE(m.recentModel()->rowCount(), 0);
    QCOMPARE(m.favoritesModel()->rowCount(), 0);
    // pins stay, frequent folders go
    QCOMPARE(names(m.quickAccessModel()), (QStringList{QStringLiteral("Desktop"), QStringLiteral("Downloads")}));
}

void TestHomeModel::settingsApplyLive()
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        QSKIP("config change notices travel on the session bus, and there is none here");
    }
    HomeModel m(&m_stats, &m_pins);
    m.setConfigName(m_config);
    QSignalSpy spy(&m, &HomeModel::settingsChanged);
    // Folder options writes with Notify: an open Home follows at once.
    writeSettings({{QStringLiteral("ShowRecent"), false}});
    QTRY_VERIFY(!m.showRecent());
    QVERIFY(spy.count() >= 1);
    QCOMPARE(m.recentModel()->rowCount(), 0);
}

void TestHomeModel::clearHistoryKeepsFavorites()
{
    HomeModel m(&m_stats, &m_pins);
    m.setConfigName(m_config);
    m.clearHistory();
    QCOMPARE(m_stats.forgetCalls, 1);
    QCOMPARE(m.recentModel()->rowCount(), 0);
    QCOMPARE(m.continueModel()->rowCount(), 0);
    QCOMPARE(m.favoritesModel()->rowCount(), 1);
    // pins are not history
    QCOMPARE(names(m.quickAccessModel()), (QStringList{QStringLiteral("Desktop"), QStringLiteral("Downloads")}));
}

void TestHomeModel::openSignals()
{
    HomeModel m(&m_stats, &m_pins);
    m.setConfigName(m_config);
    QSignalSpy open(&m, &HomeModel::openRequested);
    QSignalSpy location(&m, &HomeModel::openLocationRequested);
    const QUrl file = QUrl::fromLocalFile(QDir::homePath() + QStringLiteral("/Documents/a b.txt"));
    m.open(file);
    QCOMPARE(open.count(), 1);
    QCOMPARE(open.at(0).at(0).toUrl(), file);
    m.openLocation(file);
    QCOMPARE(location.count(), 1);
    QCOMPARE(location.at(0).at(0).toUrl(), QUrl::fromLocalFile(QDir::homePath() + QStringLiteral("/Documents")));
    QCOMPARE(location.at(0).at(1).toUrl(), file);
}

void TestHomeModel::roles()
{
    m_stats.recent = {res(QDir::homePath() + QStringLiteral("/Pictures/Beach.jpg"), 5, QStringLiteral("image/jpeg")),
                      res(QDir::homePath() + QStringLiteral("/Documents/Work/Report.pdf"), 10, QStringLiteral("application/pdf"))};
    HomeModel m(&m_stats, &m_pins);
    m.setConfigName(m_config);
    const QModelIndex pic = m.recentModel()->index(0);
    QCOMPARE(pic.data(HomeListModel::ThumbnailRole).toUrl(), m_stats.recent.at(0).url);
    QCOMPARE(pic.data(HomeListModel::LocationRole).toString(), QStringLiteral("Home › Pictures"));
    QVERIFY(!pic.data(HomeListModel::DateRole).toString().isEmpty());
    const QModelIndex pdf = m.recentModel()->index(1);
    QVERIFY(pdf.data(HomeListModel::ThumbnailRole).toUrl().isEmpty());
    QCOMPARE(pdf.data(HomeListModel::IconRole).toString(), QStringLiteral("application-pdf"));
    QCOMPARE(pdf.data(HomeListModel::LocationRole).toString(), QStringLiteral("Home › Documents › Work"));
    // folders get the Raye folder icons
    QCOMPARE(m.quickAccessModel()->index(0).data(HomeListModel::IconRole).toString().startsWith(QLatin1String("folder"))
                 || m.quickAccessModel()->index(0).data(HomeListModel::IconRole).toString() == QLatin1String("user-desktop"),
             true);
    QVERIFY(!m.recentModel()->roleNames().isEmpty());
}

void TestHomeModel::locationTexts_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QString>("expected");
    const QString h = QDir::homePath();
    const QString s = QStringLiteral(" › ");
    QTest::newRow("home") << QUrl::fromLocalFile(h) << QStringLiteral("Home");
    QTest::newRow("home sub") << QUrl::fromLocalFile(h + QStringLiteral("/Documents/Work/")) << (QStringLiteral("Home") + s + QStringLiteral("Documents") + s + QStringLiteral("Work"));
    QTest::newRow("media") << QUrl::fromLocalFile(QStringLiteral("/media/ann/USB Drive")) << (QStringLiteral("This PC") + s + QStringLiteral("media") + s + QStringLiteral("ann") + s + QStringLiteral("USB Drive"));
    QTest::newRow("root") << QUrl::fromLocalFile(QStringLiteral("/")) << QStringLiteral("This PC");
    QTest::newRow("trash") << QUrl(QStringLiteral("trash:/")) << QStringLiteral("Recycle Bin");
    QTest::newRow("network") << QUrl(QStringLiteral("smb://nas/share/Photos")) << (QStringLiteral("nas") + s + QStringLiteral("share") + s + QStringLiteral("Photos"));
}

void TestHomeModel::locationTexts()
{
    QFETCH(QUrl, url);
    QFETCH(QString, expected);
    QCOMPARE(locationText(url), expected);
    // never the account's folder path
    QVERIFY(!locationText(url).contains(QDir::homePath()));
}

void TestHomeModel::placesPins()
{
    // a places list of the test's own
    const QString folder = QDir::homePath() + QStringLiteral("/Projects/Raye");
    QVERIFY(QDir().mkpath(folder));
    KFilePlacesModel places;
    PlacesPins pins(&places);
    QSignalSpy changed(&pins, &HomePins::changed);
    const int before = int(pins.pins().size());
    QVERIFY(!pins.isPinned(QUrl::fromLocalFile(folder)));
    QVERIFY(pins.pin(QUrl::fromLocalFile(folder)));
    QVERIFY(pins.isPinned(QUrl::fromLocalFile(folder + QLatin1Char('/'))));
    QCOMPARE(int(pins.pins().size()), before + 1);
    QCOMPARE(pins.pins().last().title, QStringLiteral("Raye"));
    QVERIFY(changed.count() >= 1);
    // twice is refused, files are refused
    QVERIFY(!pins.pin(QUrl::fromLocalFile(folder)));
    QFile file(QDir::homePath() + QStringLiteral("/note.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    QVERIFY(!pins.pin(QUrl::fromLocalFile(file.fileName())));
    // a second places model (another window) sees the pin
    KFilePlacesModel other;
    PlacesPins otherPins(&other);
    QTRY_VERIFY(otherPins.isPinned(QUrl::fromLocalFile(folder)));
    QVERIFY(pins.unpin(QUrl::fromLocalFile(folder)));
    QVERIFY(!pins.isPinned(QUrl::fromLocalFile(folder)));
    QCOMPARE(int(pins.pins().size()), before);
    // Home itself and the Recycle Bin are never Quick access cards
    for (const HomeResource &r : pins.pins()) {
        QVERIFY(r.url.isLocalFile());
        QVERIFY(QDir::cleanPath(r.url.toLocalFile()) != QDir::cleanPath(QDir::homePath()));
    }
}

void TestHomeModel::recentDocumentsSource()
{
    const QString dir = QDir::homePath() + QStringLiteral("/Documents/Letters");
    QVERIFY(QDir().mkpath(dir));
    QStringList files;
    for (int i = 0; i < 3; ++i) {
        const QString f = dir + QStringLiteral("/letter%1.txt").arg(i);
        QFile out(f);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("x");
        out.close();
        KRecentDocument::add(QUrl::fromLocalFile(f));
        files.append(f);
    }
    RecentDocumentsSource source;
    const QList<HomeResource> recent = source.recentFiles(10);
    QCOMPARE(recent.size(), 3);
    const QList<HomeResource> folders = source.frequentFolders(4);
    QCOMPARE(folders.size(), 1);
    QCOMPARE(folders.first().url, QUrl::fromLocalFile(dir));
    // Favorites survive clearing the history
    QSignalSpy changed(&source, &HomeStatsSource::changed);
    source.addFavorite(QUrl::fromLocalFile(files.first()));
    QVERIFY(source.isFavorite(QUrl::fromLocalFile(files.first())));
    source.forgetHistory();
    QCOMPARE(source.recentFiles(10).size(), 0);
    QCOMPARE(source.favorites().size(), 1);
    source.removeFavorite(QUrl::fromLocalFile(files.first()));
    QCOMPARE(source.favorites().size(), 0);
    QVERIFY(changed.count() >= 3);
}

void TestHomeModel::pageHost()
{
    QVERIFY(PageHost::isPageUrl(QUrl(QStringLiteral("raye:home"))));
    QVERIFY(PageHost::isPageUrl(QUrl(QStringLiteral("raye:/home"))));
    QVERIFY(PageHost::isPageUrl(PageHost::homeUrl()));
    QVERIFY(!PageHost::isPageUrl(QUrl(QStringLiteral("raye:nothing"))));
    QVERIFY(!PageHost::isPageUrl(QUrl::fromLocalFile(QStringLiteral("/home"))));
    QCOMPARE(PageHost::pageName(QUrl(QStringLiteral("raye:home"))), QStringLiteral("home"));
    QCOMPARE(PageHost::title(PageHost::homeUrl()), QStringLiteral("Home"));
    // another package's page
    PageHost::registerPage({QStringLiteral("testpage"), QStringLiteral("HomeEmptyState"), QStringLiteral("Test"), QStringLiteral("folder"), {}});
    QVERIFY(PageHost::isPageUrl(QUrl(QStringLiteral("raye:testpage"))));

    PageHost host;
    QVERIFY(!host.setUrl(QUrl::fromLocalFile(QStringLiteral("/tmp"))));
    QVERIFY(host.setUrl(PageHost::homeUrl()));
    QCOMPARE(host.url(), PageHost::homeUrl());
    QVERIFY(host.view());
    QVERIFY2(host.view()->errors().isEmpty(), qPrintable(host.view()->errors().isEmpty() ? QString() : host.view()->errors().first().toString()));
    QVERIFY(host.view()->rootObject());
    QVERIFY(qobject_cast<HomeModel *>(host.model()));
    // the model's requests come out of the host
    QSignalSpy open(&host, &PageHost::openUrlRequested);
    QSignalSpy location(&host, &PageHost::openLocationRequested);
    const QUrl file = QUrl::fromLocalFile(QDir::homePath() + QStringLiteral("/x.txt"));
    qobject_cast<HomeModel *>(host.model())->open(file);
    qobject_cast<HomeModel *>(host.model())->openLocation(file);
    QCOMPARE(open.count(), 1);
    QCOMPARE(location.count(), 1);
    // keyboard focus goes into the page
    QCOMPARE(host.focusProxy(), host.view());
}

void TestHomeModel::sharedServices()
{
    // The source Files uses answers, with or without activity statistics.
    HomeStatsSource *source = sharedStatsSource();
    QVERIFY(source);
    QCOMPARE(sharedStatsSource(), source);
    QVERIFY(source->recentFiles(5).size() <= 5);
    qInfo("Home reads %s", hasActivityStats() ? "the activity statistics" : "KDE's recent documents");

    // The menus' "Pin to Quick access" on the shared places list
    const QString folder = QDir::homePath() + QStringLiteral("/Shared/Pinned");
    QVERIFY(QDir().mkpath(folder));
    const QUrl url = QUrl::fromLocalFile(folder);
    QVERIFY(!isPinnedToQuickAccess(url));
    QVERIFY(pinToQuickAccess(url));
    QVERIFY(isPinnedToQuickAccess(url));
    QVERIFY(!pinToQuickAccess(url));
    QVERIFY(unpinFromQuickAccess(url));
    QVERIFY(!isPinnedToQuickAccess(url));

    // "Add to Favorites". With the activity statistics the link goes
    // through the activity manager, which keeps its database where a real
    // session does, not under the test's folders: checked in the VM only.
    if (!hasActivityStats()) {
        QFile f(QDir::homePath() + QStringLiteral("/fav.txt"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        const QUrl file = QUrl::fromLocalFile(f.fileName());
        QVERIFY(addToFavorites(file));
        QVERIFY(isFavorite(file));
        QVERIFY(!addToFavorites(file));
        QVERIFY(removeFromFavorites(file));
        QVERIFY(!isFavorite(file));
    }
}

QTEST_MAIN(TestHomeModel)

#include "test_homemodel.moc"
