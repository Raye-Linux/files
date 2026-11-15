/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fold.h"

#include <algorithm>
#include <numeric>

namespace Raye::Commands
{

int usedWidth(const QList<Slot> &items, const QList<bool> &visible, int separatorWidth)
{
    int width = 0;
    bool any = false;
    int lastGroup = 0;
    for (int i = 0; i < items.size(); ++i) {
        if (!visible.at(i)) {
            continue;
        }
        if (any && items.at(i).group != lastGroup) {
            width += separatorWidth;
        }
        width += items.at(i).width;
        lastGroup = items.at(i).group;
        any = true;
    }
    return width;
}

QList<bool> fold(const QList<Slot> &items, int available, int separatorWidth)
{
    QList<bool> visible(items.size(), true);
    // Lowest priority first; on a tie the one further right.
    QList<int> order(items.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&items](int a, int b) {
        if (items.at(a).priority != items.at(b).priority) {
            return items.at(a).priority < items.at(b).priority;
        }
        return a > b;
    });
    for (int index : std::as_const(order)) {
        if (usedWidth(items, visible, separatorWidth) <= available) {
            break;
        }
        visible[index] = false;
    }
    return visible;
}

}
