/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tabwidth.h"

#include <QtGlobal>

#include <algorithm>

namespace Raye::Title
{
namespace
{
constexpr int chrome = 2 * Metrics::tabFoot + Metrics::tabPadding + Metrics::tabIconSize + Metrics::tabIconGap + Metrics::tabIconGap + Metrics::closeHit + Metrics::closeMargin;
}

int preferredTabWidth(int textWidth)
{
    return std::clamp(chrome + qMax(0, textWidth), Metrics::tabMinWidth, Metrics::tabMaxWidth);
}

int textWidthFor(int tabWidth)
{
    return qMax(0, tabWidth - chrome);
}

TabStripLayout layoutTabs(const QList<int> &textWidths, int available)
{
    TabStripLayout layout;
    layout.widths.reserve(textWidths.size());
    for (int w : textWidths) {
        layout.widths.append(preferredTabWidth(w));
    }
    if (available < 0 || layout.total() <= available) {
        return layout;
    }

    const qsizetype count = layout.widths.size();
    if (count * Metrics::tabMinWidth >= available) {
        // Not even the minimum fits: every tab at its minimum, then scroll
        // (equal at exactly the minimum is not a scroll).
        std::fill(layout.widths.begin(), layout.widths.end(), Metrics::tabMinWidth);
        layout.scrolls = count * Metrics::tabMinWidth > available;
        return layout;
    }

    // Water filling from the top: find the largest cap so that every tab at
    // min(preferred, cap) fits, the cap staying at or above the minimum.
    QList<int> sorted = layout.widths;
    std::sort(sorted.begin(), sorted.end());
    int cap = Metrics::tabMinWidth;
    int below = 0; // sum of the tabs narrower than the cap tried
    for (qsizetype i = 0; i < count; ++i) {
        const qsizetype rest = count - i;
        const int even = (available - below) / int(rest);
        if (even < sorted.at(i)) {
            cap = qMax(even, Metrics::tabMinWidth);
            break;
        }
        below += sorted.at(i);
        cap = sorted.at(i);
    }

    int used = 0;
    for (int &w : layout.widths) {
        w = qMin(w, cap);
        used += w;
    }
    // Hand the pixels lost to rounding to the capped tabs, left first, so the
    // strip ends exactly where the room does.
    int spare = available - used;
    for (qsizetype i = 0; i < count && spare > 0; ++i) {
        if (layout.widths.at(i) == cap && cap < preferredTabWidth(textWidths.at(i))) {
            ++layout.widths[i];
            --spare;
        }
    }
    return layout;
}

}
