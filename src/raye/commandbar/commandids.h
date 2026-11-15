/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

// The names of the actions the command bar is built from: Dolphin's own
// (KActionCollection names) and the ones Raye adds. A missing action leaves
// its button or menu item out, so the bar works with whatever the window has.
namespace Raye::Commands::Ids
{
// Dolphin
inline constexpr char newMenu[] = "new_menu";
inline constexpr char cut[] = "edit_cut";
inline constexpr char copy[] = "edit_copy";
inline constexpr char paste[] = "edit_paste";
inline constexpr char rename[] = "renamefile";
inline constexpr char moveToTrash[] = "movetotrash";
inline constexpr char deleteFile[] = "deletefile";
inline constexpr char sort[] = "sort";
inline constexpr char viewMode[] = "view_mode";
inline constexpr char undo[] = "edit_undo";
inline constexpr char redo[] = "edit_redo";
inline constexpr char selectAll[] = "edit_select_all";
inline constexpr char invertSelection[] = "invert_selection";
inline constexpr char properties[] = "properties";
inline constexpr char addToPlaces[] = "add_to_places";
inline constexpr char preferences[] = "options_configure";
inline constexpr char openTerminal[] = "open_terminal";
inline constexpr char ascending[] = "ascending";
inline constexpr char descending[] = "descending";
inline constexpr char sortByPrefix[] = "sort_by_";
inline constexpr char sortByName[] = "sort_by_text";
inline constexpr char sortByDate[] = "sort_by_modificationtime";
inline constexpr char sortByType[] = "sort_by_type";
inline constexpr char sortBySize[] = "sort_by_size";
inline constexpr char showInGroups[] = "show_in_groups";
inline constexpr char showHidden[] = "show_hidden_files";
inline constexpr char iconsMode[] = "icons";
inline constexpr char compactMode[] = "compact";
inline constexpr char detailsMode[] = "details";

// Raye: actions other packages add to the window's collection
inline constexpr char share[] = "raye_share"; // optional: the bar has its own
inline constexpr char detailsPane[] = "raye_details_pane"; // FX-12
inline constexpr char previewPane[] = "raye_preview_pane"; // FX-12
inline constexpr char navigationPane[] = "raye_navigation_pane"; // FX-8
inline constexpr char itemCheckBoxes[] = "raye_item_checkboxes"; // FX-10
inline constexpr char fileExtensions[] = "raye_show_extensions"; // FX-10
inline constexpr char compactView[] = "raye_compact_view"; // FX-10
inline constexpr char selectNone[] = "raye_select_none"; // FX-9
inline constexpr char folderOptions[] = "raye_folder_options"; // FX-20
inline constexpr char newShortcut[] = "raye_new_shortcut"; // FX-11
inline constexpr char newZip[] = "raye_new_zip"; // FX-16
inline constexpr char newText[] = "raye_new_text";

// Raye: the eight view modes (window/shortcuts.h viewModeActions())
inline constexpr char modeExtraLarge[] = "raye_mode_extralarge";
inline constexpr char modeLarge[] = "raye_mode_large";
inline constexpr char modeMedium[] = "raye_mode_medium";
inline constexpr char modeSmall[] = "raye_mode_small";
inline constexpr char modeList[] = "raye_mode_list";
inline constexpr char modeDetails[] = "raye_mode_details";
inline constexpr char modeTiles[] = "raye_mode_tiles";
inline constexpr char modeContent[] = "raye_mode_content";
// Raye: the default context sets of the bar (defaultsets.h)
inline constexpr char setAsBackground[] = "raye_set_background";
inline constexpr char rotateLeft[] = "raye_rotate_left";
inline constexpr char rotateRight[] = "raye_rotate_right";
inline constexpr char eject[] = "raye_eject";
inline constexpr char format[] = "raye_format";
}
