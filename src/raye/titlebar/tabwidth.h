/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QList>

// Sizes of the title row of Files (explorer spec 1.1, 3.2) and the rule that
// gives each tab its width. Pure: the tab bar and the tests use it alike.
namespace Raye::Title
{
namespace Metrics
{
constexpr int rowHeight = 40;
constexpr int tabHeight = 32; // tabs sit on the bottom of the row
constexpr int leftMargin = 8;
constexpr int tabMaxWidth = 240;
constexpr int tabMinWidth = 112;
constexpr int tabFoot = 6; // each side of a tab: room for the curves joining the address row
constexpr int tabPadding = 12; // inside the tab body, left of the icon
constexpr int tabIconSize = 16;
constexpr int tabIconGap = 8;
constexpr int closeHit = 24;
constexpr int closeGlyph = 8; // the cross in the close button
constexpr int closeMargin = 4; // right of the close button
constexpr int tabTopRadius = 8;
constexpr int newTabSize = 32;
constexpr int newTabGap = 4;
constexpr int captionWidth = 46;
constexpr int captionGlyph = 10;
constexpr int minDragSpace = 46; // always left for moving the window
constexpr int dividerHeight = 16;
constexpr int dropLineHeight = 2;
constexpr int progressRing = 20;
constexpr int snapLayoutsDelayMs = 400;
}

// What a tab wants for a label this wide: the feet, padding, icon, gap, the
// label, the close button and its margin, kept between the minimum and
// maximum.
DOLPHIN_EXPORT int preferredTabWidth(int textWidth);

// The room left for the label inside a tab this wide.
DOLPHIN_EXPORT int textWidthFor(int tabWidth);

struct TabStripLayout {
    QList<int> widths;
    // The tabs do not fit even at their minimum: they keep it and the strip
    // scrolls.
    bool scrolls = false;
    int total() const
    {
        int sum = 0;
        for (int w : widths) {
            sum += w;
        }
        return sum;
    }
};

// Widths for tabs whose labels are this wide, in this much room. Tabs keep
// their preferred width while they fit; when they do not, the widest shrink
// first, evenly, down to the minimum (Windows does the same), then the strip
// scrolls. A negative room means "unbounded".
DOLPHIN_EXPORT TabStripLayout layoutTabs(const QList<int> &textWidths, int available);

}
