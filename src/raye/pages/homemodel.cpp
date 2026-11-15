/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "homemodel.h"

#include "menu/menuplan.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

#include "options/folderoptions.h"

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QDir>
#include <QLocale>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStandardPaths>

#include <algorithm>

namespace Raye::Pages
{

QString locationText(const QUrl &folder)
{
    const QString sep = QStringLiteral(" › ");
    if (folder.scheme() == QLatin1String("trash")) {
        return i18nc("@label the trash", "Recycle Bin");
    }
    if (!folder.isLocalFile()) {
        // smb://nas/share/x → nas › share › x
        QStringList parts{folder.host()};
        parts.append(folder.path().split(QLatin1Char('/'), Qt::SkipEmptyParts));
        parts.removeAll(QString());
        return parts.join(sep);
    }
    const QString path = QDir::cleanPath(folder.toLocalFile());
    const QString home = QDir::cleanPath(QDir::homePath());
    QStringList parts;
    QString rest;
    if (path == home || path.startsWith(home + QLatin1Char('/'))) {
        parts.append(i18nc("@label the home folder", "Home"));
        rest = path.mid(home.size());
    } else {
        parts.append(i18nc("@label the whole computer", "This PC"));
        rest = path;
    }
    parts.append(rest.split(QLatin1Char('/'), Qt::SkipEmptyParts));
    return parts.join(sep);
}

QString dateText(const QDateTime &when)
{
    if (!when.isValid()) {
        return QString();
    }
    return QLocale().toString(when.toLocalTime(), QLocale::ShortFormat);
}

// --- list ----------------------------------------------------------------

HomeListModel::HomeListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int HomeListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QHash<int, QByteArray> HomeListModel::roleNames() const
{
    return {
        {UrlRole, "url"},
        {NameRole, "name"},
        {IconRole, "iconName"},
        {ThumbnailRole, "thumbnail"},
        {DateRole, "date"},
        {LocationRole, "location"},
        {FavoriteRole, "favorite"},
        {PinnedRole, "pinned"},
        {FolderRole, "folder"},
    };
}

namespace
{
QMimeType mimeOf(const HomeResource &r)
{
    QMimeDatabase db;
    if (!r.mimeType.isEmpty()) {
        const QMimeType m = db.mimeTypeForName(r.mimeType);
        if (m.isValid()) {
            return m;
        }
    }
    if (r.url.isLocalFile() && QFileInfo(r.url.toLocalFile()).isDir()) {
        return db.mimeTypeForName(QStringLiteral("inode/directory"));
    }
    return db.mimeTypeForFile(r.url.fileName(), QMimeDatabase::MatchExtension);
}

bool isFolder(const HomeResource &r)
{
    return mimeOf(r).name() == QLatin1String("inode/directory");
}

// The Raye icon of a standard folder, like the navigation pane.
QString folderIcon(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return QStringLiteral("folder-network");
    }
    const QString path = QDir::cleanPath(url.toLocalFile());
    const struct {
        QStandardPaths::StandardLocation location;
        const char *icon;
    } known[] = {
        {QStandardPaths::DesktopLocation, "user-desktop"},
        {QStandardPaths::DocumentsLocation, "folder-documents"},
        {QStandardPaths::DownloadLocation, "folder-download"},
        {QStandardPaths::MusicLocation, "folder-music"},
        {QStandardPaths::PicturesLocation, "folder-pictures"},
        {QStandardPaths::MoviesLocation, "folder-videos"},
    };
    for (const auto &k : known) {
        if (QDir::cleanPath(QStandardPaths::writableLocation(k.location)) == path) {
            return QString::fromLatin1(k.icon);
        }
    }
    return QStringLiteral("folder");
}
}

QVariant HomeListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= int(m_items.size())) {
        return {};
    }
    const Item &item = m_items.at(index.row());
    const HomeResource &r = item.resource;
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: {
        if (!r.title.isEmpty()) {
            return r.title;
        }
        const QString name = r.url.adjusted(QUrl::StripTrailingSlash).fileName();
        return name.isEmpty() ? r.url.toDisplayString(QUrl::PreferLocalFile) : name;
    }
    case UrlRole:
        return r.url;
    case IconRole:
        return isFolder(r) ? folderIcon(r.url) : mimeOf(r).iconName();
    case ThumbnailRole: {
        // Pictures show themselves on the Continue cards and in the list.
        const QString mime = mimeOf(r).name();
        if (r.url.isLocalFile() && mime.startsWith(QLatin1String("image/")) && mime != QLatin1String("image/svg+xml")) {
            return r.url;
        }
        return QUrl();
    }
    case DateRole:
        return dateText(r.used);
    case LocationRole:
        return locationText(r.url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash));
    case FavoriteRole:
        return item.favorite;
    case PinnedRole:
        return item.pinned;
    case FolderRole:
        return isFolder(r);
    }
    return {};
}

void HomeListModel::setItems(const QList<Item> &items)
{
    const int before = int(m_items.size());
    beginResetModel();
    m_items = items;
    endResetModel();
    if (before != int(m_items.size())) {
        Q_EMIT countChanged();
    }
}

QList<QUrl> HomeListModel::urls() const
{
    QList<QUrl> out;
    for (const Item &i : m_items) {
        out.append(i.resource.url);
    }
    return out;
}

// --- Home ----------------------------------------------------------------

HomeModel::HomeModel(HomeStatsSource *stats, HomePins *pins, QObject *parent)
    : QObject(parent)
    , m_stats(stats ? stats : sharedStatsSource())
    , m_pins(pins ? pins : sharedPins())
    , m_quick(new HomeListModel(this))
    , m_continue(new HomeListModel(this))
    , m_recent(new HomeListModel(this))
    , m_favorites(new HomeListModel(this))
{
    if (m_stats) {
        connect(m_stats, &HomeStatsSource::changed, this, &HomeModel::reload);
    }
    if (m_pins) {
        connect(m_pins, &HomePins::changed, this, &HomeModel::reload);
    }
    setConfigName(m_configName);
}

void HomeModel::setConfigName(const QString &name)
{
    m_configName = name;
    if (m_watcher) {
        disconnect(m_watcher.data(), nullptr, this, nullptr);
    }
    // A change in Folder options or Raye Settings shows at once.
    m_watcher = KConfigWatcher::create(KSharedConfig::openConfig(m_configName, KConfig::NoGlobals));
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == QLatin1String("Raye")) {
            reload();
        }
    });
    reload();
}

void HomeModel::readSettings()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(m_configName, KConfig::NoGlobals);
    config->reparseConfiguration();
    const KConfigGroup g = config->group(QStringLiteral("Raye"));
    const bool recent = g.readEntry("ShowRecent", true);
    const bool frequent = g.readEntry("ShowFrequent", true);
    const bool favorites = g.readEntry("ShowFavorites", true);
    const bool cont = g.readEntry("ShowContinue", true);
    if (recent != m_showRecent || frequent != m_showFrequent || favorites != m_showFavorites || cont != m_showContinue) {
        m_showRecent = recent;
        m_showFrequent = frequent;
        m_showFavorites = favorites;
        m_showContinue = cont;
        Q_EMIT settingsChanged();
    }
}

void HomeModel::reload()
{
    readSettings();
    const QList<HomeResource> favs = m_stats ? m_stats->favorites() : QList<HomeResource>();
    auto isFav = [&favs](const QUrl &url) {
        return std::any_of(favs.cbegin(), favs.cend(), [&url](const HomeResource &r) {
            return r.url == url;
        });
    };

    // Quick access: pins, then the most used folders not pinned.
    QList<HomeListModel::Item> quick;
    const QList<HomeResource> pins = m_pins ? m_pins->pins() : QList<HomeResource>();
    for (const HomeResource &p : pins) {
        HomeResource r = p;
        r.mimeType = QStringLiteral("inode/directory");
        quick.append({r, true, false});
    }
    if (m_showFrequent && m_stats) {
        int added = 0;
        for (const HomeResource &f : m_stats->frequentFolders(frequentCount + int(pins.size()))) {
            if (added >= frequentCount) {
                break;
            }
            if (m_pins && m_pins->isPinned(f.url)) {
                continue;
            }
            HomeResource r = f;
            r.mimeType = QStringLiteral("inode/directory");
            quick.append({r, false, false});
            ++added;
        }
    }
    m_quick->setItems(quick);

    // Recent, and the Continue strip from its first files.
    QList<HomeListModel::Item> recent;
    if (m_showRecent && m_stats) {
        const int limit = m_more ? recentMoreCount : recentCount;
        // One more than shown tells whether "Show more" has anything.
        const QList<HomeResource> found = m_stats->recentFiles(limit + 1);
        m_recentFull = found.size() > limit;
        for (const HomeResource &r : found.mid(0, limit)) {
            recent.append({r, false, isFav(r.url)});
        }
    } else {
        m_recentFull = false;
    }
    m_recent->setItems(recent);
    m_continue->setItems(showContinue() ? recent.mid(0, continueCount) : QList<HomeListModel::Item>());

    QList<HomeListModel::Item> favItems;
    if (m_showFavorites) {
        for (const HomeResource &r : favs) {
            favItems.append({r, false, true});
        }
    }
    m_favorites->setItems(favItems);
    Q_EMIT changed();
}

bool HomeModel::canShowMoreRecent() const
{
    return !m_more && m_recentFull;
}

void HomeModel::open(const QUrl &url)
{
    Q_EMIT openRequested(url);
}

void HomeModel::openLocation(const QUrl &url)
{
    Q_EMIT openLocationRequested(url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash), url);
}

void HomeModel::toggleFavorite(const QUrl &url)
{
    if (!m_stats) {
        return;
    }
    if (m_stats->isFavorite(url)) {
        m_stats->removeFavorite(url);
    } else {
        m_stats->addFavorite(url);
    }
    reload();
}

void HomeModel::togglePin(const QUrl &url)
{
    if (!m_pins) {
        return;
    }
    if (m_pins->isPinned(url)) {
        m_pins->unpin(url);
    } else {
        m_pins->pin(url);
    }
    reload();
}

void HomeModel::showMoreRecent()
{
    if (!m_more) {
        m_more = true;
        reload();
    }
}

void HomeModel::clearHistory()
{
    if (m_stats) {
        m_stats->forgetHistory();
    }
    reload();
}

// --- shared services -----------------------------------------------------

HomeStatsSource *sharedStatsSource()
{
    static QPointer<HomeStatsSource> source;
    if (!source && QCoreApplication::instance()) {
        source = createStatsSource(QCoreApplication::instance());
    }
    return source;
}

HomePins *sharedPins()
{
    static QPointer<HomePins> pins;
    if (!pins && QCoreApplication::instance()) {
        pins = new PlacesPins(nullptr, QCoreApplication::instance());
    }
    return pins;
}

bool pinToQuickAccess(const QUrl &folder)
{
    HomePins *p = sharedPins();
    return p && p->pin(folder);
}

bool unpinFromQuickAccess(const QUrl &folder)
{
    HomePins *p = sharedPins();
    return p && p->unpin(folder);
}

bool isPinnedToQuickAccess(const QUrl &folder)
{
    HomePins *p = sharedPins();
    return p && p->isPinned(folder);
}

bool addToFavorites(const QUrl &file)
{
    HomeStatsSource *s = sharedStatsSource();
    if (!s || s->isFavorite(file)) {
        return false;
    }
    s->addFavorite(file);
    return true;
}

bool removeFromFavorites(const QUrl &file)
{
    HomeStatsSource *s = sharedStatsSource();
    if (!s || !s->isFavorite(file)) {
        return false;
    }
    s->removeFavorite(file);
    return true;
}

bool isFavorite(const QUrl &file)
{
    HomeStatsSource *s = sharedStatsSource();
    return s && s->isFavorite(file);
}

void clearFilesHistory()
{
    if (HomeStatsSource *s = sharedStatsSource()) {
        s->forgetHistory();
    }
}

}

// Folder options' "Clear" reaches the history Home reads.
static void rayeHomeRegisterClearHistory()
{
    Raye::Options::FolderOptions::setClearHistoryHook(&Raye::Pages::clearFilesHistory);
}
Q_COREAPP_STARTUP_FUNCTION(rayeHomeRegisterClearHistory)

void Raye::Pages::HomeModel::openInNewTab(const QUrl &url)
{
    Q_EMIT openInNewTabRequested(url);
}

void Raye::Pages::HomeModel::openInNewWindow(const QUrl &url)
{
    Q_EMIT openInNewWindowRequested(url);
}

void Raye::Pages::HomeModel::properties(const QUrl &url)
{
    Q_EMIT propertiesRequested(url);
}

void Raye::Pages::HomeModel::copyAsPath(const QUrl &url)
{
    copyPathToClipboard(url);
}

void Raye::Pages::copyPathToClipboard(const QUrl &url)
{
    auto *mime = new QMimeData;
    mime->setText(Raye::Menu::copyAsPathText({url}));
    QGuiApplication::clipboard()->setMimeData(mime);
}
