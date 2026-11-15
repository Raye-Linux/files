/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "statustext.h"

#include <KFormat>
#include <KLocalizedString>

namespace Raye::Window
{
QString itemsText(int items)
{
    return i18ncp("@info:status", "%1 item", "%1 items", items);
}

QString selectionText(const StatusCounts &counts)
{
    if (counts.selected <= 0) {
        return QString();
    }
    const QString selected = i18ncp("@info:status", "%1 item selected", "%1 items selected", counts.selected);
    if (counts.selectedFiles <= 0) {
        return selected;
    }
    const QString size = counts.selectedBytes <= 0 ? i18nc("@info:status size of empty files", "0 bytes")
                                                    : KFormat().formatByteSize(double(counts.selectedBytes), 1, KFormat::JEDECBinaryDialect);
    return i18nc("@info:status %1 is '3 items selected', %2 a size", "%1  %2", selected, size);
}

QStringList statusParts(const StatusCounts &counts)
{
    QStringList parts{itemsText(counts.items)};
    const QString selection = selectionText(counts);
    if (!selection.isEmpty()) {
        parts << selection;
    }
    return parts;
}
}
