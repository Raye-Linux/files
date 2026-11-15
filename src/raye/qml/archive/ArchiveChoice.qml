/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Templates as T
import org.raye.files
import org.raye.files.kit

// A row of choices where one is picked (format, level, compression): one
// plate, radius 4, the picked part filled with the accent. Left and Right
// move between the parts like a group of radio buttons.
T.Control {
    id: choice

    // the labels, in order
    property var options: []
    property int currentIndex: 0
    // read by Orca: "Format", "Level"...
    property string accessibleName: ""

    signal activated(int index)


    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: Kit.controlHeight
    opacity: enabled ? 1 : 0.45
    focusPolicy: Qt.TabFocus
    hoverEnabled: true

    Accessible.role: Accessible.Grouping
    Accessible.name: accessibleName

    function pick(i) {
        if (i < 0 || i >= options.length || i === currentIndex) return
        currentIndex = i
        activated(i)
    }

    Keys.onLeftPressed: pick(currentIndex - 1)
    Keys.onRightPressed: pick(currentIndex + 1)

    padding: 2
    background: Rectangle {
        radius: Kit.radiusControl
        color: Kit.colors.fillControl
        border.width: 1
        border.color: Kit.colors.strokeControl
    }

    contentItem: Row {
        id: row
        spacing: 2
        Repeater {
            model: choice.options
            T.AbstractButton {
                id: part
                required property int index
                required property string modelData
                readonly property bool picked: index === choice.currentIndex
                height: Kit.controlHeight - 4
                width: Math.max(56, label.implicitWidth + 24)
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData
                Accessible.checked: picked
                onClicked: choice.pick(index)
                background: Rectangle {
                    radius: Kit.radiusControl - 1
                    color: part.picked ? (part.down ? Kit.colors.accentPressed : part.hovered ? Kit.colors.accentHover : Kit.colors.accent)
                                       : (part.down ? Kit.colors.fillSubtlePressed : part.hovered ? Kit.colors.fillSubtleHover : "transparent")
                    Behavior on color { ColorAnimation { duration: Kit.durationFaster } }
                }
                contentItem: Text {
                    id: label
                    text: part.modelData
                    font: part.picked ? Kit.bodyStrong : Kit.body
                    color: part.picked ? Kit.colors.textOnAccent : Kit.colors.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // keyboard focus only (Tab), never after a click
    FocusRing {
        visible: choice.visualFocus
    }
}
