/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QString>
#include <QStringList>

namespace Raye::Commands
{

class CommandBar;

// A [Desktop Action] of a service menu file.
struct ServiceAction {
    QString file;
    QString action;
    bool isValid() const
    {
        return !file.isEmpty() && !action.isEmpty();
    }
};

struct RotateActions {
    ServiceAction left;
    ServiceAction right;
};

// Where service menus live (kio/servicemenus in the data folders).
DOLPHIN_EXPORT QStringList serviceMenuDirs();

// Service menu actions that rotate pictures: a file whose MimeType covers
// pictures, with an action marked X-Raye-Rotate=left|right, or named like
// "rotateLeft" / "rotate-right" / "rotate_cw". Raye ships none: without
// one, Rotate left and Rotate right are not offered.
DOLPHIN_EXPORT RotateActions findRotateActions(const QStringList &dirs);

// The context sets every command bar has (explorer spec 3.4):
// - one picture selected on this computer: Set as background (through
//   plasma-apply-wallpaperimage), and Rotate left / right when a service
//   menu offers them;
// - a drive selected: Eject for removable ones, Format (the disk manager)
//   for all.
DOLPHIN_EXPORT void addDefaultSets(CommandBar *bar);

}
