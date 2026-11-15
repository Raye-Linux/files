/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QObject>
#include <QString>

#include <functional>

namespace Raye::Options
{

// Folder options of Files (explorer spec 3.15, keys of 3.18). The values
// are read from the account's configuration, changed in memory by the
// dialog, and written by apply(): dolphinrc [Raye] and [General],
// kdeglobals [KDE] SingleClick (shared with the desktop) and Baloo's
// content indexing. "Restore defaults" removes the account's own values,
// so the system defaults in /etc/xdg apply again.
class DOLPHIN_EXPORT FolderOptions : public QObject
{
    Q_OBJECT
    // General
    Q_PROPERTY(QString startPage READ startPage WRITE setStartPage NOTIFY changed)
    Q_PROPERTY(bool openInOwnWindow READ openInOwnWindow WRITE setOpenInOwnWindow NOTIFY changed)
    Q_PROPERTY(bool singleClick READ singleClick WRITE setSingleClick NOTIFY changed)
    Q_PROPERTY(bool showRecent READ showRecent WRITE setShowRecent NOTIFY changed)
    Q_PROPERTY(bool showFrequent READ showFrequent WRITE setShowFrequent NOTIFY changed)
    Q_PROPERTY(bool showFavorites READ showFavorites WRITE setShowFavorites NOTIFY changed)
    // View
    Q_PROPERTY(bool alwaysShowIcons READ alwaysShowIcons WRITE setAlwaysShowIcons NOTIFY changed)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY changed)
    Q_PROPERTY(bool hideExtensions READ hideExtensions WRITE setHideExtensions NOTIFY changed)
    Q_PROPERTY(bool showStatusBar READ showStatusBar WRITE setShowStatusBar NOTIFY changed)
    Q_PROPERTY(bool compact READ compact WRITE setCompact NOTIFY changed)
    Q_PROPERTY(bool showAllFolders READ showAllFolders WRITE setShowAllFolders NOTIFY changed)
    Q_PROPERTY(bool expandToOpenFolder READ expandToOpenFolder WRITE setExpandToOpenFolder NOTIFY changed)
    Q_PROPERTY(bool itemCheckBoxes READ itemCheckBoxes WRITE setItemCheckBoxes NOTIFY changed)
    // Search
    Q_PROPERTY(bool searchContents READ searchContents WRITE setSearchContents NOTIFY changed)

    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)

public:
    enum Tab {
        General,
        View,
        Search,
    };
    Q_ENUM(Tab)

    struct Files {
        QString dolphinrc = QStringLiteral("dolphinrc");
        QString kdeglobals = QStringLiteral("kdeglobals");
        QString baloofilerc = QStringLiteral("baloofilerc");
        // Dolphin's per-folder view settings (Reset folders empties it).
        QString viewPropertiesDir;
    };

    explicit FolderOptions(QObject *parent = nullptr);
    FolderOptions(const Files &files, QObject *parent = nullptr);

    QString startPage() const
    {
        return m_v.startPage;
    }
    void setStartPage(const QString &page);
    bool openInOwnWindow() const
    {
        return m_v.ownWindow;
    }
    void setOpenInOwnWindow(bool on);
    bool singleClick() const
    {
        return m_v.singleClick;
    }
    void setSingleClick(bool on);
    bool showRecent() const
    {
        return m_v.recent;
    }
    void setShowRecent(bool on);
    bool showFrequent() const
    {
        return m_v.frequent;
    }
    void setShowFrequent(bool on);
    bool showFavorites() const
    {
        return m_v.favorites;
    }
    void setShowFavorites(bool on);
    bool alwaysShowIcons() const
    {
        return m_v.alwaysIcons;
    }
    void setAlwaysShowIcons(bool on);
    bool showHidden() const
    {
        return m_v.hidden;
    }
    void setShowHidden(bool on);
    bool hideExtensions() const
    {
        return m_v.hideExtensions;
    }
    void setHideExtensions(bool on);
    bool showStatusBar() const
    {
        return m_v.statusBar;
    }
    void setShowStatusBar(bool on);
    bool compact() const
    {
        return m_v.compact;
    }
    void setCompact(bool on);
    bool showAllFolders() const
    {
        return m_v.allFolders;
    }
    void setShowAllFolders(bool on);
    bool expandToOpenFolder() const
    {
        return m_v.expandToOpen;
    }
    void setExpandToOpenFolder(bool on);
    bool itemCheckBoxes() const
    {
        return m_v.checkBoxes;
    }
    void setItemCheckBoxes(bool on);
    bool searchContents() const
    {
        return m_v.contents;
    }
    void setSearchContents(bool on);

    // Something changed since load() or apply().
    bool dirty() const;

    // Reads the configuration again (drops what was not applied).
    Q_INVOKABLE void load();
    // Writes what changed; false when a file could not be written.
    Q_INVOKABLE bool apply();
    // The values of the system defaults for the tab's keys, not written
    // until apply().
    Q_INVOKABLE void restoreDefaults(int tab);
    // "Reset folders": every folder forgets its own view, now.
    Q_INVOKABLE bool resetFolders();
    // "Clear": the history behind Home's Recent list (the hook is set by
    // the Home page code; nothing happens without it).
    Q_INVOKABLE void clearHistory();
    static void setClearHistoryHook(const std::function<void()> &hook);
    // Opens the Raye Settings page for file search.
    Q_INVOKABLE void openSearchSettings();

Q_SIGNALS:
    void changed();
    void applied();

private:
    struct Values {
        QString startPage = QStringLiteral("home");
        bool ownWindow = false;
        bool singleClick = false;
        bool recent = true;
        bool frequent = true;
        bool favorites = true;
        bool alwaysIcons = false;
        bool hidden = false;
        bool hideExtensions = true;
        bool statusBar = true;
        bool compact = false;
        bool allFolders = false;
        bool expandToOpen = false;
        bool checkBoxes = false;
        bool contents = true;
        bool operator==(const Values &) const = default;
    };
    void set(bool Values::*field, bool on);
    Values read(bool defaultsOnly) const;

    Files m_files;
    Values m_v;
    Values m_saved;
};

}
