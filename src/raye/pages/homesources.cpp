/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "homesources.h"

#include <KConfigGroup>
#include <KFilePlacesModel>
#include <KRecentDocument>
#include <KSharedConfig>

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>

#include <algorithm>

namespace Raye::Pages
{

QString favoritesAgent()
{
    return QStringLiteral("org.raye.files.favorites");
}

bool HomeStatsSource::isFavorite(const QUrl &url) const
{
    const QList<HomeResource> list = favorites();
    return std::any_of(list.cbegin(), list.cend(), [&url](const HomeResource &r) {
        return r.url == url;
    });
}

QList<HomeResource> StaticStatsSource::recentFiles(int limit) const
{
    return recent.mid(0, limit);
}

QList<HomeResource> StaticStatsSource::frequentFolders(int limit) const
{
    return folders.mid(0, limit);
}

QList<HomeResource> StaticStatsSource::favorites() const
{
    return favs;
}

void StaticStatsSource::addFavorite(const QUrl &url)
{
    if (!isFavorite(url)) {
        HomeResource r;
        r.url = url;
        r.used = QDateTime::currentDateTime();
        for (const HomeResource &known : std::as_const(recent)) {
            if (known.url == url) {
                r = known;
            }
        }
        favs.prepend(r);
        Q_EMIT changed();
    }
}

void StaticStatsSource::removeFavorite(const QUrl &url)
{
    if (favs.removeAll(HomeResource{url, {}, {}, {}}) > 0) {
        Q_EMIT changed();
    }
}

void StaticStatsSource::forgetHistory()
{
    ++forgetCalls;
    recent.clear();
    folders.clear();
    Q_EMIT changed();
}

// --- KDE's recent documents --------------------------------------------

namespace
{
KConfigGroup favoritesGroup()
{
    return KSharedConfig::openStateConfig()->group(QStringLiteral("Raye Favorites"));
}

QList<HomeResource> existingRecent()
{
    QList<HomeResource> out;
    const QList<QUrl> urls = KRecentDocument::recentUrls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            HomeResource r;
            r.url = url;
            out.append(r);
            continue;
        }
        const QFileInfo fi(url.toLocalFile());
        if (!fi.exists() || fi.isDir()) {
            continue;
        }
        HomeResource r;
        r.url = url;
        r.used = fi.lastRead().isValid() ? fi.lastRead() : fi.lastModified();
        out.append(r);
    }
    std::stable_sort(out.begin(), out.end(), [](const HomeResource &a, const HomeResource &b) {
        return a.used > b.used;
    });
    return out;
}
}

QList<HomeResource> RecentDocumentsSource::recentFiles(int limit) const
{
    return existingRecent().mid(0, limit);
}

QList<HomeResource> RecentDocumentsSource::frequentFolders(int limit) const
{
    // The folders holding most of the recent files, Home left out.
    QHash<QString, int> count;
    QHash<QString, QDateTime> last;
    for (const HomeResource &r : existingRecent()) {
        if (!r.url.isLocalFile()) {
            continue;
        }
        const QString dir = QFileInfo(r.url.toLocalFile()).absolutePath();
        if (dir == QDir::homePath()) {
            continue;
        }
        ++count[dir];
        last[dir] = std::max(last.value(dir), r.used);
    }
    QStringList dirs = count.keys();
    std::sort(dirs.begin(), dirs.end(), [&](const QString &a, const QString &b) {
        return count.value(a) != count.value(b) ? count.value(a) > count.value(b) : last.value(a) > last.value(b);
    });
    QList<HomeResource> out;
    for (const QString &d : std::as_const(dirs)) {
        if (out.size() >= limit) {
            break;
        }
        HomeResource r;
        r.url = QUrl::fromLocalFile(d);
        r.mimeType = QStringLiteral("inode/directory");
        r.used = last.value(d);
        out.append(r);
    }
    return out;
}

QList<HomeResource> RecentDocumentsSource::favorites() const
{
    QList<HomeResource> out;
    const QStringList urls = favoritesGroup().readEntry("Urls", QStringList());
    for (const QString &s : urls) {
        HomeResource r;
        r.url = QUrl(s);
        if (r.url.isLocalFile()) {
            const QFileInfo fi(r.url.toLocalFile());
            if (!fi.exists()) {
                continue;
            }
            r.used = fi.lastModified();
        }
        out.append(r);
    }
    return out;
}

void RecentDocumentsSource::addFavorite(const QUrl &url)
{
    KConfigGroup g = favoritesGroup();
    QStringList urls = g.readEntry("Urls", QStringList());
    if (urls.contains(url.toString())) {
        return;
    }
    urls.prepend(url.toString());
    g.writeEntry("Urls", urls);
    g.sync();
    Q_EMIT changed();
}

void RecentDocumentsSource::removeFavorite(const QUrl &url)
{
    KConfigGroup g = favoritesGroup();
    QStringList urls = g.readEntry("Urls", QStringList());
    if (urls.removeAll(url.toString()) > 0) {
        g.writeEntry("Urls", urls);
        g.sync();
        Q_EMIT changed();
    }
}

void RecentDocumentsSource::forgetHistory()
{
    KRecentDocument::clear();
    Q_EMIT changed();
}

// --- pins ----------------------------------------------------------------

namespace
{
bool isStandardFolder(const QString &path)
{
    for (auto location : {QStandardPaths::DesktopLocation,
                          QStandardPaths::DocumentsLocation,
                          QStandardPaths::DownloadLocation,
                          QStandardPaths::MusicLocation,
                          QStandardPaths::PicturesLocation,
                          QStandardPaths::MoviesLocation}) {
        if (QDir::cleanPath(QStandardPaths::writableLocation(location)) == path) {
            return true;
        }
    }
    return false;
}
}

bool HomePins::isPinned(const QUrl &url) const
{
    const QList<HomeResource> list = pins();
    const QUrl clean = url.adjusted(QUrl::StripTrailingSlash);
    return std::any_of(list.cbegin(), list.cend(), [&clean](const HomeResource &r) {
        return r.url.adjusted(QUrl::StripTrailingSlash) == clean;
    });
}

bool StaticPins::pin(const QUrl &url)
{
    if (isPinned(url)) {
        return false;
    }
    HomeResource r;
    r.url = url;
    r.mimeType = QStringLiteral("inode/directory");
    list.append(r);
    Q_EMIT changed();
    return true;
}

bool StaticPins::unpin(const QUrl &url)
{
    const QUrl clean = url.adjusted(QUrl::StripTrailingSlash);
    const auto before = list.size();
    list.erase(std::remove_if(list.begin(),
                              list.end(),
                              [&clean](const HomeResource &r) {
                                  return r.url.adjusted(QUrl::StripTrailingSlash) == clean;
                              }),
               list.end());
    if (list.size() == before) {
        return false;
    }
    Q_EMIT changed();
    return true;
}

PlacesPins::PlacesPins(KFilePlacesModel *places, QObject *parent)
    : HomePins(parent)
    , m_places(places ? places : new KFilePlacesModel(this))
{
    auto notify = [this] {
        Q_EMIT changed();
    };
    connect(m_places, &QAbstractItemModel::rowsInserted, this, notify);
    connect(m_places, &QAbstractItemModel::rowsRemoved, this, notify);
    connect(m_places, &QAbstractItemModel::dataChanged, this, notify);
    connect(m_places, &QAbstractItemModel::modelReset, this, notify);
}

QList<HomeResource> PlacesPins::pins() const
{
    QList<HomeResource> out;
    if (!m_places) {
        return out;
    }
    const QString home = QDir::cleanPath(QDir::homePath());
    for (int i = 0; i < m_places->rowCount(); ++i) {
        const QModelIndex idx = m_places->index(i, 0);
        if (m_places->isHidden(idx) || m_places->isDevice(idx) || m_places->groupType(idx) != KFilePlacesModel::PlacesType) {
            continue;
        }
        const QUrl url = m_places->url(idx);
        if (!url.isLocalFile()) {
            continue;
        }
        const QString path = QDir::cleanPath(url.toLocalFile());
        if (path == home || path == QLatin1String("/") || !QFileInfo(path).isDir()) {
            continue;
        }
        HomeResource r;
        r.url = QUrl::fromLocalFile(path);
        r.title = m_places->text(idx);
        r.mimeType = QStringLiteral("inode/directory");
        out.append(r);
    }
    return out;
}

bool PlacesPins::pin(const QUrl &url)
{
    if (!m_places || !url.isLocalFile() || !QFileInfo(url.toLocalFile()).isDir() || isPinned(url)) {
        return false;
    }
    const QString path = QDir::cleanPath(url.toLocalFile());
    // A pinned folder that was hidden comes back rather than twice.
    for (int i = 0; i < m_places->rowCount(); ++i) {
        const QModelIndex idx = m_places->index(i, 0);
        const QUrl u = m_places->url(idx);
        if (u.isLocalFile() && QDir::cleanPath(u.toLocalFile()) == path && m_places->isHidden(idx)) {
            m_places->setPlaceHidden(idx, false);
            return true;
        }
    }
    m_places->addPlace(QFileInfo(path).fileName(), QUrl::fromLocalFile(path), QStringLiteral("folder"));
    return true;
}

bool PlacesPins::unpin(const QUrl &url)
{
    if (!m_places || !url.isLocalFile()) {
        return false;
    }
    const QString path = QDir::cleanPath(url.toLocalFile());
    for (int i = 0; i < m_places->rowCount(); ++i) {
        const QModelIndex idx = m_places->index(i, 0);
        const QUrl u = m_places->url(idx);
        if (u.isLocalFile() && QDir::cleanPath(u.toLocalFile()) == path && !m_places->isHidden(idx) && !m_places->isDevice(idx)) {
            // The standard folders KDE puts there are hidden, not removed,
            // so the navigation pane can show them again.
            if (isStandardFolder(path)) {
                m_places->setPlaceHidden(idx, true);
            } else {
                m_places->removePlace(idx);
            }
            return true;
        }
    }
    return false;
}

}
