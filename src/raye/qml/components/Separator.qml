/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import org.raye.files.kit

// A 1 px divider: 24 tall between command groups, or a full-width line.
Rectangle {
    property bool vertical: true

    implicitWidth: vertical ? 1 : 24
    implicitHeight: vertical ? 24 : 1
    color: Kit.colors.strokeDivider
    Accessible.role: Accessible.Separator
}
