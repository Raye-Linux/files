/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Templates as T
import org.raye.files
import org.raye.files.kit

// A radio button of Folder options: a 20 px ring, filled with the accent
// and a dot when chosen; the label after it in Body.
T.RadioButton {
    id: radio

    implicitWidth: implicitIndicatorWidth + spacing + implicitContentWidth
    implicitHeight: Kit.controlHeight
    spacing: 8
    font: Kit.body
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    // Each group binds checked to its own setting: no automatic exclusion
    // across the groups of one column.
    autoExclusive: false

    Accessible.role: Accessible.RadioButton
    Accessible.name: text

    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        x: radio.leftPadding
        anchors.verticalCenter: parent.verticalCenter
        radius: 10
        color: radio.checked ? (radio.down ? Kit.colors.accentPressed : radio.hovered ? Kit.colors.accentHover : Kit.colors.accent)
                             : (radio.down ? Kit.colors.fillControlPressed : radio.hovered ? Kit.colors.fillControlHover : Kit.colors.fillControl)
        border.width: radio.checked ? 0 : 1
        border.color: Kit.colors.strokeStrong
        Rectangle {
            visible: radio.checked
            anchors.centerIn: parent
            width: radio.down ? 8 : radio.hovered ? 12 : 10
            height: width
            radius: width / 2
            color: Kit.colors.textOnAccent
            Behavior on width { NumberAnimation { duration: Kit.durationFaster } }
        }
        FocusRing {
            visible: radio.visualFocus
            controlRadius: 10
        }
    }

    contentItem: Text {
        textFormat: Text.PlainText
        leftPadding: radio.indicator.width + radio.spacing
        verticalAlignment: Text.AlignVCenter
        text: radio.text
        font: radio.font
        color: radio.enabled ? Kit.colors.textPrimary : Kit.colors.textDisabled
        wrapMode: Text.Wrap
    }
}
