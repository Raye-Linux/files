/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QList>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QUrl>

class KJob;
class QAction;
class QMenu;
class QWidget;

#include <functional>

// What the items of the Files menu do when no window action does it: the
// pieces the context menu and the This PC page share.
namespace Raye::Menu
{

// "Copy as path" (Ctrl+Shift+C): the quoted paths on the clipboard.
DOLPHIN_EXPORT void copyAsPath(const QList<QUrl> &urls);

// "Create shortcut": a link to each local item, next to it, named the
// Windows way ("report - Shortcut.pdf"). Returns the jobs started (the
// caller may select the new items when they finish); errors show on
// window.
DOLPHIN_EXPORT QList<KJob *> createShortcuts(const QList<QUrl> &urls, QWidget *window);

// "Set as desktop background": the shell's own tool, the path as one
// argument, never through a shell. False when the file is not local.
DOLPHIN_EXPORT bool setAsDesktopBackground(const QUrl &picture);

// Rotate left / right: only when a service menu offers it (see
// Commands::findRotateActions); false otherwise.
DOLPHIN_EXPORT bool canRotate(bool right);
DOLPHIN_EXPORT bool rotate(const QUrl &picture, bool right);

// The Share flyout at a point (where the menu was).
DOLPHIN_EXPORT void shareAt(const QPoint &globalPos, const QList<QUrl> &urls, const QStringList &mimeTypes);

// "Open as administrator" is offered only when kio-admin is installed;
// this is the address that opens url that way.
DOLPHIN_EXPORT bool adminAvailable();
DOLPHIN_EXPORT QUrl adminUrl(const QUrl &url);

// Finds a window action by its name (KActionCollection::action).
using ActionLookup = std::function<QAction *(const QString &name)>;

// The submenus of the background menu, built on the window's own actions:
// View (the view modes), Sort by (the sort roles, Ascending, Descending)
// and Group by (None, then the roles: grouping follows the sort role). Made
// for parent (it owns them); nullptr when the window has no such actions.
DOLPHIN_EXPORT QMenu *viewMenu(QMenu *parent, const ActionLookup &lookup);
DOLPHIN_EXPORT QMenu *sortMenu(QMenu *parent, const ActionLookup &lookup);
DOLPHIN_EXPORT QMenu *groupMenu(QMenu *parent, const ActionLookup &lookup);

// "Open with ›": the first four apps for the type (Files itself left out),
// then "Choose another app", which calls chooseOther.
DOLPHIN_EXPORT QMenu *openWithMenu(QMenu *parent, const QList<QUrl> &urls, const QString &mimeType, const std::function<void()> &chooseOther);
constexpr int openWithApps = 4;

// KFileItemActions plugins the full menu leaves out: Files handles
// archives itself (Compress to, Extract all), and "Open as administrator"
// comes back under Raye's own words.
DOLPHIN_EXPORT QStringList excludedPlugins();

}
