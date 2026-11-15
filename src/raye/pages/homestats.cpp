/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Home's lists from the Plasma activity statistics: the same database the
// Start menu's "Recommended" reads. Built when Files links
// Plasma::ActivitiesStats (see the CMake lines of FX-13); without it Home
// falls back to KDE's recent documents.

#include "homesources.h"

#if __has_include(<PlasmaActivities/Stats/ResultSet>)
#define RAYE_HOME_ACTIVITIES 1
#include <PlasmaActivities/Stats/Cleaning>
#include <PlasmaActivities/Stats/Query>
#include <PlasmaActivities/Stats/ResultModel>
#include <PlasmaActivities/Stats/ResultSet>
#include <PlasmaActivities/Stats/ResultWatcher>
#include <PlasmaActivities/Stats/Terms>
#endif

#include <QTimer>

namespace Raye::Pages
{

#ifdef RAYE_HOME_ACTIVITIES

namespace Stats = KActivities::Stats;
// The query operators live in Stats, the terms in Stats::Terms.
using namespace KActivities::Stats;
using namespace KActivities::Stats::Terms;

namespace
{
QList<HomeResource> run(const Stats::Query &query)
{
    QList<HomeResource> out;
    for (const Stats::ResultSet::Result &result : Stats::ResultSet(query)) {
        HomeResource r;
        r.url = result.url();
        if (!r.url.isValid() || r.url.isEmpty()) {
            continue;
        }
        if (result.title() != result.resource()) {
            r.title = result.title();
        }
        if (result.mimetype() != result.resource()) {
            r.mimeType = result.mimetype();
        }
        r.used = QDateTime::fromSecsSinceEpoch(result.lastUpdate());
        out.append(r);
    }
    return out;
}

Stats::Query recentQuery(int limit)
{
    return UsedResources | Agent::any() | Type::files() | Activity::any() | Url::file() | RecentlyUsedFirst | Limit(limit);
}

Stats::Query foldersQuery(int limit)
{
    return UsedResources | Agent::any() | Type::directories() | Activity::any() | Url::file() | HighScoredFirst | Limit(limit);
}

Stats::Query favoritesQuery()
{
    return LinkedResources | Agent(favoritesAgent()) | Type::any() | Activity::any() | RecentlyCreatedFirst;
}

class ActivitiesStatsSource : public HomeStatsSource
{
public:
    explicit ActivitiesStatsSource(QObject *parent)
        : HomeStatsSource(parent)
        , m_favorites(new Stats::ResultModel(favoritesQuery(), this))
    {
        // Many changes arrive at once (a folder copied in, a session
        // starting): one refresh for all of them.
        m_coalesce.setSingleShot(true);
        m_coalesce.setInterval(250);
        connect(&m_coalesce, &QTimer::timeout, this, &HomeStatsSource::changed);
        for (const Stats::Query &q : {recentQuery(50), foldersQuery(20), favoritesQuery()}) {
            auto *watcher = new Stats::ResultWatcher(q, this);
            connect(watcher, &Stats::ResultWatcher::resultScoreUpdated, &m_coalesce, qOverload<>(&QTimer::start));
            connect(watcher, &Stats::ResultWatcher::resultRemoved, &m_coalesce, qOverload<>(&QTimer::start));
            connect(watcher, &Stats::ResultWatcher::resultLinked, &m_coalesce, qOverload<>(&QTimer::start));
            connect(watcher, &Stats::ResultWatcher::resultUnlinked, &m_coalesce, qOverload<>(&QTimer::start));
            connect(watcher, &Stats::ResultWatcher::resultsInvalidated, &m_coalesce, qOverload<>(&QTimer::start));
        }
    }

    QList<HomeResource> recentFiles(int limit) const override
    {
        return run(recentQuery(limit));
    }
    QList<HomeResource> frequentFolders(int limit) const override
    {
        return run(foldersQuery(limit));
    }
    QList<HomeResource> favorites() const override
    {
        return run(favoritesQuery());
    }
    void addFavorite(const QUrl &url) override
    {
        m_favorites->linkToActivity(url, Activity::global(), Agent(favoritesAgent()));
        m_coalesce.start();
    }
    void removeFavorite(const QUrl &url) override
    {
        m_favorites->unlinkFromActivity(url, Activity::global(), Agent(favoritesAgent()));
        m_coalesce.start();
    }
    void forgetHistory() override
    {
        // Every recent file and folder, of every app: Windows clears the
        // whole history too. Linked Favorites are not usage and stay.
        Stats::forgetResources(UsedResources | Agent::any() | Type::any() | Activity::any() | Url::file());
        m_coalesce.start();
    }

private:
    Stats::ResultModel *m_favorites;
    QTimer m_coalesce;
};
}

HomeStatsSource *createStatsSource(QObject *parent)
{
    return new ActivitiesStatsSource(parent);
}

bool hasActivityStats()
{
    return true;
}

#else

HomeStatsSource *createStatsSource(QObject *parent)
{
    return new RecentDocumentsSource(parent);
}

bool hasActivityStats()
{
    return false;
}

#endif

}
