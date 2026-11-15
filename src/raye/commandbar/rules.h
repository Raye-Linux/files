/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

// When a command bar button can act (explorer spec 1.3): Windows disables,
// never hides. Cut, Copy, Rename, Share and Delete need a selection, Paste
// files on the clipboard; what writes needs a folder that can be written.
namespace Raye::Commands
{

enum class Command {
    New,
    Cut,
    Copy,
    Paste,
    Rename,
    Share,
    Delete,
    Other,
};

struct State {
    int selected = 0;
    bool clipboardHasFiles = false;
    bool writable = true;
};

DOLPHIN_EXPORT bool allows(Command command, const State &state);

}
