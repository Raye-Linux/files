/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rules.h"

namespace Raye::Commands
{

bool allows(Command command, const State &state)
{
    const bool selection = state.selected > 0;
    switch (command) {
    case Command::New:
        return state.writable;
    case Command::Cut:
    case Command::Rename:
    case Command::Delete:
        return selection && state.writable;
    case Command::Copy:
    case Command::Share:
        return selection;
    case Command::Paste:
        return state.clipboardHasFiles && state.writable;
    case Command::Other:
        break;
    }
    return true;
}

}
