/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QKeySequence>
#include <QList>
#include <QString>
#include <QStringList>

namespace Raye::Window
{
// The keys of File Explorer (explorer spec 1.18) on the window's actions,
// by action name. Dolphin's own keys that would clash are taken off the
// actions in clearedShortcuts() (their panels are gone from the window).
struct Shortcut {
    QString action;
    QList<QKeySequence> keys;
};

DOLPHIN_EXPORT QList<Shortcut> windowShortcuts();
DOLPHIN_EXPORT QStringList clearedShortcuts();
// The eight view modes, in the order of the View menu and Ctrl+Shift+1..8:
// "raye_mode_" and the id of Raye::ViewDefaults::modeId().
DOLPHIN_EXPORT QStringList viewModeActions();
}
