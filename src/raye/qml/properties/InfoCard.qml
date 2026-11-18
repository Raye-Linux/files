/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.raye.files.kit

// A card of Properties: rows of "Label:  value" on the card colour.
Rectangle {
    id: card

    default property alias rows: grid.data
    property alias columns: grid.columns

    Layout.fillWidth: true
    implicitHeight: grid.implicitHeight + 20
    radius: Kit.radiusOverlay
    color: Kit.colors.bgCard
    border.width: 1
    border.color: Kit.colors.strokeCard

    GridLayout {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        columns: 2
        columnSpacing: 12
        rowSpacing: 6
    }
}
