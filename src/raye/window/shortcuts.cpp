/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "shortcuts.h"

namespace Raye::Window
{
QList<Shortcut> windowShortcuts()
{
    QList<Shortcut> list = {
        {QStringLiteral("raye_focus_search"), {Qt::CTRL | Qt::Key_E, Qt::CTRL | Qt::Key_F, Qt::Key_F3}},
        {QStringLiteral("raye_edit_location"), {Qt::ALT | Qt::Key_D, Qt::CTRL | Qt::Key_L, Qt::Key_F4}},
        {QStringLiteral("raye_details_pane"), {Qt::ALT | Qt::SHIFT | Qt::Key_P}},
        {QStringLiteral("raye_preview_pane"), {Qt::ALT | Qt::Key_P}},
        {QStringLiteral("raye_copy_path"), {Qt::CTRL | Qt::SHIFT | Qt::Key_C}},
        {QStringLiteral("raye_full_screen"), {Qt::Key_F11}},
        {QStringLiteral("create_dir"), {Qt::CTRL | Qt::SHIFT | Qt::Key_N, Qt::Key_F10}},
    };
    // Windows has Ctrl+Shift+1..8 for the eight views; Ctrl+1..9 go to the tabs.
    const QStringList modes = viewModeActions();
    for (int i = 0; i < modes.size(); ++i) {
        list.append({modes.at(i), {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key(Qt::Key_1 + i))}});
    }
    for (int i = 0; i < 9; ++i) {
        list.append({QStringLiteral("activate_tab_%1").arg(i), {QKeySequence(Qt::CTRL | Qt::Key(Qt::Key_1 + i)), QKeySequence(Qt::ALT | Qt::Key(Qt::Key_1 + i))}});
    }
    return list;
}

QStringList viewModeActions()
{
    return {
        QStringLiteral("raye_mode_extralarge"),
        QStringLiteral("raye_mode_large"),
        QStringLiteral("raye_mode_medium"),
        QStringLiteral("raye_mode_small"),
        QStringLiteral("raye_mode_list"),
        QStringLiteral("raye_mode_details"),
        QStringLiteral("raye_mode_tiles"),
        QStringLiteral("raye_mode_content"),
    };
}

QStringList clearedShortcuts()
{
    return {
        // Dolphin's three layouts had Ctrl+1..3, which are the tabs now.
        QStringLiteral("icons"),
        QStringLiteral("compact"),
        QStringLiteral("details"),
        QStringLiteral("toggle_search"),
        QStringLiteral("replace_location"),
        QStringLiteral("editable_location"),
        QStringLiteral("split_view"),
        QStringLiteral("show_places_panel"),
        QStringLiteral("show_folders_panel"),
        QStringLiteral("show_terminal_panel"),
        QStringLiteral("show_information_panel"),
        QStringLiteral("focus_places_panel"),
        QStringLiteral("focus_terminal_panel"),
        QStringLiteral("options_show_menubar"),
        QStringLiteral("split_stash"),
        QStringLiteral("popout_split_view"),
    };
}
}
