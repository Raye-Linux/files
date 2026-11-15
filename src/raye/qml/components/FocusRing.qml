/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import org.raye.files.kit

// The Raye keyboard focus ring: 2 px outer ring, 1 px inner ring, drawn
// 3 px outside the control, plus a soft glow at night. Put it in the
// control and bind visible to the control's visualFocus: never on a click.
Item {
    id: ring

    // corner radius of the control the ring goes around
    property real controlRadius: Kit.radiusControl

    anchors.fill: parent
    anchors.margins: -3
    z: 100

    Rectangle {
        // glow, outside the outer ring
        visible: Kit.night
        anchors.fill: parent
        anchors.margins: -2
        radius: ring.controlRadius + 5
        color: "transparent"
        border.width: 2
        border.color: Kit.colors.glow
    }
    Rectangle {
        anchors.fill: parent
        radius: ring.controlRadius + 3
        color: "transparent"
        border.width: 2
        border.color: Kit.colors.focusOuter
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: ring.controlRadius + 1
        color: "transparent"
        border.width: 1
        border.color: Kit.colors.focusInner
    }
}
