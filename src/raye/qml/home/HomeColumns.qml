/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import org.raye.files.kit

// The three columns of Home's list (Name 50 %, Date 22 %, Location the
// rest), shared by the header and the rows so they line up.
Item {
    id: columns

    property bool header: false
    property string name: ""
    property string date: ""
    property string location: ""
    // room left at the start of the Name column (the row's icon)
    property int nameIndent: 0
    // room kept at the end for the hover buttons
    property int trailing: 72

    readonly property real nameWidth: (width - trailing) * 0.5
    readonly property real dateWidth: (width - trailing) * 0.22

    Text {
        textFormat: Text.PlainText
        x: 12 + columns.nameIndent
        width: columns.nameWidth - 12 - columns.nameIndent
        anchors.verticalCenter: parent.verticalCenter
        text: columns.name
        font: columns.header ? Kit.caption : Kit.body
        color: columns.header ? Kit.colors.textSecondary : Kit.colors.textPrimary
        elide: Text.ElideMiddle
    }
    Text {
        textFormat: Text.PlainText
        x: columns.nameWidth + 8
        width: columns.dateWidth - 8
        anchors.verticalCenter: parent.verticalCenter
        text: columns.date
        font: columns.header ? Kit.caption : Kit.body
        color: Kit.colors.textSecondary
        elide: Text.ElideRight
    }
    Text {
        textFormat: Text.PlainText
        x: columns.nameWidth + columns.dateWidth + 8
        width: columns.width - x - columns.trailing
        anchors.verticalCenter: parent.verticalCenter
        text: columns.location
        font: columns.header ? Kit.caption : Kit.body
        color: Kit.colors.textSecondary
        elide: Text.ElideMiddle
    }
}
