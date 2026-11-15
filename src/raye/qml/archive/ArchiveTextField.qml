/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Templates as T
import org.raye.files
import org.raye.files.kit

// The text box of the archive wizards: 32 tall, radius 4, the control fill
// with a stronger bottom edge, and a 2 px accent line under it while typing
// (like every Raye text box).
T.TextField {
    id: field

    implicitWidth: 240
    implicitHeight: Kit.controlHeight
    leftPadding: 11
    rightPadding: 11
    verticalAlignment: TextInput.AlignVCenter
    font: Kit.body
    color: enabled ? Kit.colors.textPrimary : Kit.colors.textDisabled
    placeholderTextColor: Kit.colors.textTertiary
    selectionColor: Kit.colors.accent
    selectedTextColor: Kit.colors.textOnAccent
    selectByMouse: true
    hoverEnabled: true

    Accessible.role: Accessible.EditableText
    Accessible.name: placeholderText

    // the template draws no placeholder of its own
    Text {
        x: field.leftPadding
        width: field.width - field.leftPadding - field.rightPadding
        height: field.height
        verticalAlignment: Text.AlignVCenter
        visible: field.length === 0 && field.preeditText.length === 0
        text: field.placeholderText
        font: field.font
        color: field.placeholderTextColor
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitHeight: Kit.controlHeight
        radius: Kit.radiusControl
        color: field.activeFocus ? Kit.colors.fillInputActive
             : field.hovered ? Kit.colors.fillControlHover : Kit.colors.fillControl
        border.width: 1
        border.color: Kit.colors.strokeControl
        Behavior on color { ColorAnimation { duration: Kit.durationFaster } }

        // the bottom edge, round with the corners
        Item {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: field.activeFocus ? 2 : 1
            clip: true
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: Kit.radiusControl * 2
                radius: Kit.radiusControl
                color: "transparent"
                border.width: field.activeFocus ? 2 : 1
                border.color: field.activeFocus ? Kit.colors.accent : Kit.colors.strokeControlBottom
            }
        }
    }
}
