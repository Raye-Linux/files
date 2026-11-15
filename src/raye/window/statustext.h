/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QString>
#include <QStringList>

namespace Raye::Window
{
// What the status bar says (explorer spec 1.6, 3.7): "12 items" on the left,
// then "1 item selected  2.4 MB" when something is selected. The size only
// shows when the selection holds files: Windows does not add up folders.
struct StatusCounts {
    int items = 0;
    int selected = 0;
    int selectedFiles = 0;
    qint64 selectedBytes = 0;
};

DOLPHIN_EXPORT QString itemsText(int items);
DOLPHIN_EXPORT QString selectionText(const StatusCounts &counts);

// The parts in order, empty ones left out. The bar draws a thin divider
// between them.
DOLPHIN_EXPORT QStringList statusParts(const StatusCounts &counts);
}
