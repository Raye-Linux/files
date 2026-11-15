/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "folderoptions.h"

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

namespace Raye::Options
{

namespace
{
const QString raye = QStringLiteral("Raye");
const QString general = QStringLiteral("General");

std::function<void()> &clearHook()
{
    static std::function<void()> hook;
    return hook;
}

// Dolphin's status bar setting: 0 small, 1 across the window, 2 off.
constexpr int statusBarFullWidth = 1;
constexpr int statusBarDisabled = 2;
}

FolderOptions::FolderOptions(QObject *parent)
    : FolderOptions(Files(), parent)
{
}

FolderOptions::FolderOptions(const Files &files, QObject *parent)
    : QObject(parent)
    , m_files(files)
{
    if (m_files.viewPropertiesDir.isEmpty()) {
        m_files.viewPropertiesDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/dolphin/view_properties");
    }
    load();
}

FolderOptions::Values FolderOptions::read(bool defaultsOnly) const
{
    Values v;
    auto open = [defaultsOnly](const QString &name) {
        auto c = std::make_unique<KConfig>(name, KConfig::NoGlobals);
        if (defaultsOnly) {
            // Read from the system copies only: the same name looked up
            // in the configuration folders after the account's own.
            QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
            if (!dirs.isEmpty()) {
                dirs.removeFirst();
            }
            QStringList found;
            for (const QString &d : std::as_const(dirs)) {
                const QString f = d + QLatin1Char('/') + name;
                if (QFile::exists(f)) {
                    found.prepend(f);
                }
            }
            c = std::make_unique<KConfig>(QString(), KConfig::SimpleConfig);
            if (!found.isEmpty()) {
                c = std::make_unique<KConfig>(found.last(), KConfig::SimpleConfig);
                c->addConfigSources(found.mid(0, found.size() - 1));
                c->reparseConfiguration();
            }
        }
        return c;
    };
    const auto dolphin = open(m_files.dolphinrc);
    const KConfigGroup r = dolphin->group(raye);
    const KConfigGroup g = dolphin->group(general);
    v.startPage = r.readEntry("StartPage", v.startPage) == QLatin1String("thispc") ? QStringLiteral("thispc") : QStringLiteral("home");
    v.ownWindow = r.readEntry("OpenFoldersInOwnWindow", v.ownWindow);
    v.recent = r.readEntry("ShowRecent", v.recent);
    v.frequent = r.readEntry("ShowFrequent", v.frequent);
    v.favorites = r.readEntry("ShowFavorites", v.favorites);
    v.alwaysIcons = r.readEntry("AlwaysShowIcons", v.alwaysIcons);
    v.hidden = r.readEntry("ShowHidden", v.hidden);
    v.hideExtensions = !r.readEntry("ShowExtensions", !v.hideExtensions);
    v.statusBar = g.readEntry("ShowStatusBar", statusBarFullWidth) != statusBarDisabled;
    v.compact = r.readEntry("Compact", v.compact);
    v.allFolders = r.readEntry("ShowAllFolders", v.allFolders);
    v.expandToOpen = r.readEntry("ExpandToOpenFolder", v.expandToOpen);
    v.checkBoxes = r.readEntry("ItemCheckBoxes", v.checkBoxes);

    const auto globals = open(m_files.kdeglobals);
    v.singleClick = globals->group(QStringLiteral("KDE")).readEntry("SingleClick", false);

    const auto baloo = open(m_files.baloofilerc);
    v.contents = !baloo->group(general).readEntry("only basic indexing", false);
    return v;
}

void FolderOptions::load()
{
    m_v = read(false);
    m_saved = m_v;
    Q_EMIT changed();
}

bool FolderOptions::dirty() const
{
    return !(m_v == m_saved);
}

void FolderOptions::set(bool Values::*field, bool on)
{
    if (m_v.*field == on) {
        return;
    }
    m_v.*field = on;
    Q_EMIT changed();
}

void FolderOptions::setStartPage(const QString &page)
{
    const QString p = page == QLatin1String("thispc") ? page : QStringLiteral("home");
    if (p != m_v.startPage) {
        m_v.startPage = p;
        Q_EMIT changed();
    }
}

void FolderOptions::setOpenInOwnWindow(bool on)
{
    set(&Values::ownWindow, on);
}
void FolderOptions::setSingleClick(bool on)
{
    set(&Values::singleClick, on);
}
void FolderOptions::setShowRecent(bool on)
{
    set(&Values::recent, on);
}
void FolderOptions::setShowFrequent(bool on)
{
    set(&Values::frequent, on);
}
void FolderOptions::setShowFavorites(bool on)
{
    set(&Values::favorites, on);
}
void FolderOptions::setAlwaysShowIcons(bool on)
{
    set(&Values::alwaysIcons, on);
}
void FolderOptions::setShowHidden(bool on)
{
    set(&Values::hidden, on);
}
void FolderOptions::setHideExtensions(bool on)
{
    set(&Values::hideExtensions, on);
}
void FolderOptions::setShowStatusBar(bool on)
{
    set(&Values::statusBar, on);
}
void FolderOptions::setCompact(bool on)
{
    set(&Values::compact, on);
}
void FolderOptions::setShowAllFolders(bool on)
{
    set(&Values::allFolders, on);
}
void FolderOptions::setExpandToOpenFolder(bool on)
{
    set(&Values::expandToOpen, on);
}
void FolderOptions::setItemCheckBoxes(bool on)
{
    set(&Values::checkBoxes, on);
}
void FolderOptions::setSearchContents(bool on)
{
    set(&Values::contents, on);
}

bool FolderOptions::apply()
{
    const Values &v = m_v;
    const Values &old = m_saved;
    bool ok = true;

    KSharedConfig::Ptr dolphin = KSharedConfig::openConfig(m_files.dolphinrc, KConfig::NoGlobals);
    KConfigGroup r = dolphin->group(raye);
    KConfigGroup g = dolphin->group(general);
    // Notify: open windows watch dolphinrc and change at once.
    const auto flags = KConfig::Normal | KConfig::Notify;
    auto put = [&r, flags](const char *key, const auto &now, const auto &before) {
        if (!(now == before)) {
            r.writeEntry(key, now, flags);
        }
    };
    put("StartPage", v.startPage, old.startPage);
    put("OpenFoldersInOwnWindow", v.ownWindow, old.ownWindow);
    put("ShowRecent", v.recent, old.recent);
    put("ShowFrequent", v.frequent, old.frequent);
    put("ShowFavorites", v.favorites, old.favorites);
    put("AlwaysShowIcons", v.alwaysIcons, old.alwaysIcons);
    put("ShowHidden", v.hidden, old.hidden);
    put("ShowExtensions", !v.hideExtensions, !old.hideExtensions);
    put("Compact", v.compact, old.compact);
    put("ShowAllFolders", v.allFolders, old.allFolders);
    put("ExpandToOpenFolder", v.expandToOpen, old.expandToOpen);
    put("ItemCheckBoxes", v.checkBoxes, old.checkBoxes);
    if (v.statusBar != old.statusBar) {
        g.writeEntry("ShowStatusBar", v.statusBar ? statusBarFullWidth : statusBarDisabled, flags);
    }
    ok = dolphin->sync() && ok;

    if (v.singleClick != old.singleClick) {
        KSharedConfig::Ptr globals = KSharedConfig::openConfig(m_files.kdeglobals, KConfig::NoGlobals);
        globals->group(QStringLiteral("KDE")).writeEntry("SingleClick", v.singleClick, flags);
        ok = globals->sync() && ok;
    }
    if (v.contents != old.contents) {
        KSharedConfig::Ptr baloo = KSharedConfig::openConfig(m_files.baloofilerc, KConfig::NoGlobals);
        baloo->group(general).writeEntry("only basic indexing", !v.contents, flags);
        ok = baloo->sync() && ok;
    }
    if (ok) {
        m_saved = m_v;
        Q_EMIT changed();
        Q_EMIT applied();
    }
    return ok;
}

void FolderOptions::restoreDefaults(int tab)
{
    const Values d = read(true);
    switch (tab) {
    case General:
        m_v.startPage = d.startPage;
        m_v.ownWindow = d.ownWindow;
        m_v.singleClick = d.singleClick;
        m_v.recent = d.recent;
        m_v.frequent = d.frequent;
        m_v.favorites = d.favorites;
        break;
    case View:
        m_v.alwaysIcons = d.alwaysIcons;
        m_v.hidden = d.hidden;
        m_v.hideExtensions = d.hideExtensions;
        m_v.statusBar = d.statusBar;
        m_v.compact = d.compact;
        m_v.allFolders = d.allFolders;
        m_v.expandToOpen = d.expandToOpen;
        m_v.checkBoxes = d.checkBoxes;
        break;
    case Search:
        m_v.contents = d.contents;
        break;
    }
    Q_EMIT changed();
}

bool FolderOptions::resetFolders()
{
    // Dolphin ignores a folder's saved view older than this time stamp.
    KSharedConfig::Ptr dolphin = KSharedConfig::openConfig(m_files.dolphinrc, KConfig::NoGlobals);
    dolphin->group(general).writeEntry("ViewPropsTimestamp", QDateTime::currentDateTime(), KConfig::Normal | KConfig::Notify);
    bool ok = dolphin->sync();
    QDir dir(m_files.viewPropertiesDir);
    if (dir.exists()) {
        ok = dir.removeRecursively() && ok;
    }
    return ok;
}

void FolderOptions::clearHistory()
{
    if (clearHook()) {
        clearHook()();
    }
}

void FolderOptions::setClearHistoryHook(const std::function<void()> &hook)
{
    clearHook() = hook;
}

void FolderOptions::openSearchSettings()
{
    QProcess::startDetached(QStringLiteral("raye-settings"), {QStringLiteral("--page"), QStringLiteral("privacy/file-search")});
}

}
