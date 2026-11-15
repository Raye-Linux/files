/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "homesources.h"

#include <QAbstractListModel>
#include <QPointer>

#include <KConfigWatcher>

namespace Raye::Pages
{

// One list of Home: Quick access cards, the Continue strip, Recent or
// Favorites.
class DOLPHIN_EXPORT HomeListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles {
        UrlRole = Qt::UserRole + 1,
        NameRole,
        IconRole,
        ThumbnailRole,
        DateRole,
        LocationRole,
        FavoriteRole,
        PinnedRole,
        FolderRole,
    };
    struct Item {
        HomeResource resource;
        bool pinned = false;
        bool favorite = false;
    };

    explicit HomeListModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(const QList<Item> &items);
    QList<Item> items() const
    {
        return m_items;
    }
    QList<QUrl> urls() const;

Q_SIGNALS:
    void countChanged();

private:
    QList<Item> m_items;
};

// Home (explorer spec 3.9): Quick access (pins, then the most used folders
// that are not pinned), the Continue strip (the last three files), and the
// Recent | Favorites lists, following the Folder options privacy choices
// (dolphinrc [Raye] ShowRecent, ShowFrequent, ShowFavorites, ShowContinue).
class DOLPHIN_EXPORT HomeModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *quickAccess READ quickAccess CONSTANT)
    Q_PROPERTY(QObject *continueItems READ continueItems CONSTANT)
    Q_PROPERTY(QObject *recent READ recent CONSTANT)
    Q_PROPERTY(QObject *favorites READ favorites CONSTANT)
    Q_PROPERTY(bool showRecent READ showRecent NOTIFY settingsChanged)
    Q_PROPERTY(bool showFrequent READ showFrequent NOTIFY settingsChanged)
    Q_PROPERTY(bool showFavorites READ showFavorites NOTIFY settingsChanged)
    Q_PROPERTY(bool showContinue READ showContinue NOTIFY settingsChanged)
    Q_PROPERTY(bool moreRecent READ moreRecent NOTIFY changed)
    Q_PROPERTY(bool canShowMoreRecent READ canShowMoreRecent NOTIFY changed)

public:
    static constexpr int frequentCount = 4; // folders after the pins
    static constexpr int continueCount = 3;
    static constexpr int recentCount = 20;
    static constexpr int recentMoreCount = 50;

    // Neither is owned. Null: the sources Files uses (activity statistics
    // or recent documents, and the places list).
    explicit HomeModel(HomeStatsSource *stats = nullptr, HomePins *pins = nullptr, QObject *parent = nullptr);

    QObject *quickAccess() const
    {
        return m_quick;
    }
    QObject *continueItems() const
    {
        return m_continue;
    }
    QObject *recent() const
    {
        return m_recent;
    }
    QObject *favorites() const
    {
        return m_favorites;
    }
    HomeListModel *quickAccessModel() const
    {
        return m_quick;
    }
    HomeListModel *continueModel() const
    {
        return m_continue;
    }
    HomeListModel *recentModel() const
    {
        return m_recent;
    }
    HomeListModel *favoritesModel() const
    {
        return m_favorites;
    }

    bool showRecent() const
    {
        return m_showRecent;
    }
    bool showFrequent() const
    {
        return m_showFrequent;
    }
    bool showFavorites() const
    {
        return m_showFavorites;
    }
    bool showContinue() const
    {
        return m_showContinue && m_showRecent;
    }
    bool moreRecent() const
    {
        return m_more;
    }
    bool canShowMoreRecent() const;

    // Where the settings come from (tests use a file of their own).
    void setConfigName(const QString &name);

    Q_INVOKABLE void open(const QUrl &url);
    Q_INVOKABLE void openLocation(const QUrl &url);
    Q_INVOKABLE void toggleFavorite(const QUrl &url);
    Q_INVOKABLE void togglePin(const QUrl &url);
    Q_INVOKABLE void openInNewTab(const QUrl &url);
    Q_INVOKABLE void openInNewWindow(const QUrl &url);
    Q_INVOKABLE void properties(const QUrl &url);
    Q_INVOKABLE void copyAsPath(const QUrl &url);
    Q_INVOKABLE void showMoreRecent();
    Q_INVOKABLE void clearHistory();
    // Reads the sources and the settings again.
    Q_INVOKABLE void reload();

Q_SIGNALS:
    // The page host opens these (in the tab, like a double-click).
    void openRequested(const QUrl &url);
    void openLocationRequested(const QUrl &folder, const QUrl &select);
    void openInNewTabRequested(const QUrl &url);
    void openInNewWindowRequested(const QUrl &url);
    void propertiesRequested(const QUrl &url);
    void changed();
    void settingsChanged();

private:
    void readSettings();

    QPointer<HomeStatsSource> m_stats;
    QPointer<HomePins> m_pins;
    HomeListModel *m_quick;
    HomeListModel *m_continue;
    HomeListModel *m_recent;
    HomeListModel *m_favorites;
    QString m_configName = QStringLiteral("dolphinrc");
    KConfigWatcher::Ptr m_watcher;
    bool m_showRecent = true;
    bool m_showFrequent = true;
    bool m_showFavorites = true;
    bool m_showContinue = true;
    bool m_more = false;
    bool m_recentFull = false;
};

// "Home › Documents › Work", "This PC › media › USB Drive", "Recycle Bin":
// where a folder is, in Raye words, never /home/<user>.
DOLPHIN_EXPORT QString locationText(const QUrl &folder);
// "10/30/2026 4:12 PM" in the user's language.
DOLPHIN_EXPORT QString dateText(const QDateTime &when);

// The services menus use (FX-11's "Pin to Quick access", "Add to
// Favorites"), on the sources Files uses. Each returns false when nothing
// changed.
DOLPHIN_EXPORT bool pinToQuickAccess(const QUrl &folder);
DOLPHIN_EXPORT bool unpinFromQuickAccess(const QUrl &folder);
DOLPHIN_EXPORT bool isPinnedToQuickAccess(const QUrl &folder);
DOLPHIN_EXPORT bool addToFavorites(const QUrl &file);
DOLPHIN_EXPORT bool removeFromFavorites(const QUrl &file);
DOLPHIN_EXPORT bool isFavorite(const QUrl &file);
// "Copy as path": the item's path on the clipboard, quoted like Windows.
DOLPHIN_EXPORT void copyPathToClipboard(const QUrl &url);
// "Clear File Explorer history" (Folder options).
DOLPHIN_EXPORT void clearFilesHistory();
// The shared sources (made on first use).
DOLPHIN_EXPORT HomeStatsSource *sharedStatsSource();
DOLPHIN_EXPORT HomePins *sharedPins();

}
