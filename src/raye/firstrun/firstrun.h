/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <KSharedConfig>

#include <QString>
#include <QStringList>

class KConfig;
class KFilePlacesModel;

// What Files does once for each account (explorer spec 3.5, 3.14 and 3.18).
// The system defaults live in /etc/xdg/dolphinrc; this is the part that can
// only be done in the user's own files, and only the first time.
namespace Raye::FirstRun
{

// Version of the per-user defaults below (trash size, hidden Templates and
// Public). Raise it when a step is added: an account only runs the steps
// newer than the number it has in [Raye] DefaultsVersion.
inline constexpr int DefaultsVersion = 1;

// The places list every KDE dialog shares (user-places.xbel), made to look
// like Windows' Quick access: the KDE defaults Windows has not (Home, Trash,
// the root folder, the Network, Recent and Search For groups) are hidden, and
// Desktop, Downloads, Documents, Pictures, Music, Videos come first, in that
// order. Places the user added are kept, after those six. Nothing is ever
// removed. Returns whether anything changed; a second call changes nothing.
DOLPHIN_EXPORT bool migratePlaces(KFilePlacesModel *model);

// Recycle Bin defaults for the home trash, Windows' way: about 5 % of the
// drive, and the oldest items go when it is full (KIO refuses the delete
// instead). Only written when the user has no setting for that folder yet.
DOLPHIN_EXPORT bool applyTrashDefaults(KConfig &ktrashrc, const QString &homeTrashPath);

// Templates and Public folders sitting right in the home folder, read from
// the text of user-dirs.dirs: their names, for the home's .hidden list.
// Folders elsewhere, missing ones or ones set to the home itself are left out.
DOLPHIN_EXPORT QStringList hiddenUserDirs(const QString &home, const QString &userDirsText);

// Adds the names missing from a .hidden file, keeping what is there.
// Returns whether the file changed.
DOLPHIN_EXPORT bool addToHiddenFile(const QString &hiddenFile, const QStringList &names);

// Runs what is left to run for this account, guarded by [Raye]
// PlacesMigrated and [Raye] DefaultsVersion in dolphinrc. Cheap once done:
// call it at start, before the navigation pane reads the places.
DOLPHIN_EXPORT void runOnce(const KSharedConfig::Ptr &dolphinrc);
DOLPHIN_EXPORT void runOnce();

}
