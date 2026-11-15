/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import org.raye.files.kit

// A row of views to pick from ("Recent | Favorites"): Body strong labels,
// the selected one in primary text over a 3 px accent underline that
// slides to it (167 ms). Left and Right move the selection.
FocusScope {
    id: bar

    // the labels, in order
    property var model: []
    property int currentIndex: 0
    signal activated(int index)

    implicitWidth: row.implicitWidth
    implicitHeight: 40
    activeFocusOnTab: true

    Accessible.role: Accessible.PageTabList

    Keys.onLeftPressed: select(Math.max(0, currentIndex - 1))
    Keys.onRightPressed: select(Math.min(model.length - 1, currentIndex + 1))
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Home) { select(0); event.accepted = true }
        else if (event.key === Qt.Key_End) { select(model.length - 1); event.accepted = true }
    }

    function select(index) {
        if (index < 0 || index >= model.length || index === currentIndex)
            return
        currentIndex = index
        activated(index)
    }

    Row {
        id: row
        height: parent.height
        spacing: 4
        Repeater {
            id: items
            model: bar.model
            delegate: Item {
                id: item
                required property int index
                required property string modelData
                readonly property bool current: index === bar.currentIndex
                width: text.implicitWidth + 24
                height: row.height

                Accessible.role: Accessible.PageTab
                Accessible.name: modelData
                Accessible.selected: current

                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: 4
                    anchors.bottomMargin: 4
                    radius: Kit.radiusControl
                    color: mouse.pressed ? Kit.colors.fillSubtlePressed : mouse.containsMouse ? Kit.colors.fillSubtleHover : "transparent"
                }
                Text {
                    id: text
                    anchors.centerIn: parent
                    text: item.modelData
                    font: Kit.bodyStrong
                    color: item.current || mouse.containsMouse ? Kit.colors.textPrimary : Kit.colors.textSecondary
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: bar.select(item.index)
                }
                FocusRing {
                    anchors.topMargin: 1
                    anchors.bottomMargin: 1
                    visible: bar.activeFocus && item.current && bar.focusReason !== Qt.MouseFocusReason
                }
            }
        }
    }

    Rectangle {
        id: pill
        readonly property Item target: items.count > bar.currentIndex ? items.itemAt(bar.currentIndex) : null
        visible: target !== null
        width: 16
        height: 3
        radius: 1.5
        y: bar.height - height - 4
        x: target ? target.x + (target.width - width) / 2 : 0
        color: Kit.colors.accent
        Behavior on x {
            NumberAnimation { duration: Kit.durationFast; easing.type: Easing.OutCubic }
        }
    }
}
