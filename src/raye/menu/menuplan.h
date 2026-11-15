/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

// What the right-click menu of Files holds (explorer spec 1.11, 3.11), as
// lists of item ids. Pure: the menu, the This PC page and the tests read the
// same table. The ids are the object names of the menu's actions:
//
//   row      cut copy paste rename share delete
//   items    open open_with open_new_tab open_new_window open_file_location
//            map_drive disconnect_drive pin_quick_access favorite compress extract_all extract_here extract_to
//            set_background
//            rotate_right rotate_left copy_path create_shortcut
//            open_terminal open_admin eject format restore empty_bin view
//            sort group refresh undo new properties more_options
//   "-" is a separator.
namespace Raye::Menu
{

enum class ItemKind {
    File,
    Folder,
    Archive, // a ZIP, 7z, TAR... Files opens as a folder
    Picture,
    Executable, // a program: .exe, or a file with the executable bit
    Drive, // a drive or partition in This PC
};

enum class Place {
    Normal,
    RecycleBin,
    Search, // search results: items live in many folders
};

struct DOLPHIN_EXPORT Selection {
    Place place = Place::Normal;
    // Right click on empty space: the menu of the folder itself.
    bool background = false;
    QList<ItemKind> items;
    // The clipboard holds files that can be pasted here.
    bool clipboardHasFiles = false;
    // The folder (or for a background menu, this folder) can be changed.
    bool writable = true;
    // The items are on this computer (archives, shortcuts, the desktop
    // background and the terminal need a local file).
    bool local = true;
    // A drive that can be ejected or safely removed.
    bool driveRemovable = false;
    // One computer or shared folder seen on the network (Map network drive),
    // or the folder of a mapped network drive (Disconnect).
    bool networkShare = false;
    bool networkDrive = false;
};

struct DOLPHIN_EXPORT Plan {
    QStringList row;
    QStringList items;
};

// The compact menu for a selection.
DOLPHIN_EXPORT Plan plan(const Selection &selection);

// The kind of one item from its MIME type, whether it is a folder, and its
// name (".exe" counts as a program whatever its MIME type says).
DOLPHIN_EXPORT ItemKind kindOf(const QString &mimeType, bool isDir, const QString &fileName, bool executableBit);

// "Copy as path": each item quoted like Windows does, one per line;
// local files as paths, other places as their address (no password).
DOLPHIN_EXPORT QString copyAsPathText(const QList<QUrl> &urls);

// "Create shortcut": the name of the shortcut next to fileName, the way
// Windows names it ("report - Shortcut.pdf", "Work - Shortcut"), with
// " (2)", " (3)"... when a name in taken is already used.
DOLPHIN_EXPORT QString shortcutName(const QString &fileName, bool isDir, const QStringList &taken);

}
