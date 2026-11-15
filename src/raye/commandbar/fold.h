/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QList>

// Which buttons of the command bar stay when the window is narrow (explorer
// spec 3.4): the ones with the lowest priority go into "…" first, until the
// rest fits. A group whose buttons all went takes its separator with it.
namespace Raye::Commands
{

struct Slot {
    int width = 0;
    int priority = 0; // higher stays longer
    int group = 0; // a separator stands between two visible groups
};

// Visible flags, in the order of the items.
DOLPHIN_EXPORT QList<bool> fold(const QList<Slot> &items, int available, int separatorWidth);

// The width the visible items take.
DOLPHIN_EXPORT int usedWidth(const QList<Slot> &items, const QList<bool> &visible, int separatorWidth);

}
