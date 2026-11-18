/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Templates as T
import org.raye.files
import org.raye.files.kit

// A drop-down list (dictionary size, threads, part size...): the control
// plate with the picked text and a chevron, the list in a flyout under it.
T.ComboBox {
    id: box

    property string accessibleName: ""

    implicitWidth: 200
    implicitHeight: Kit.controlHeight
    opacity: enabled ? 1 : 0.45
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font: Kit.body

    Accessible.name: accessibleName

    leftPadding: 11
    rightPadding: 30

    background: Rectangle {
        radius: Kit.radiusControl
        color: box.down ? Kit.colors.fillControlPressed : box.hovered ? Kit.colors.fillControlHover : Kit.colors.fillControl
        border.width: 1
        border.color: box.visualFocus ? Kit.colors.accent : Kit.colors.strokeControl
        Behavior on color { ColorAnimation { duration: Kit.durationFaster } }
    }

    contentItem: Text {
        text: box.displayText
        font: Kit.body
        color: Kit.colors.textPrimary
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: box.width - width - 11
        y: (box.height - height) / 2
        text: "⌄"
        font.pixelSize: 12
        color: Kit.colors.textSecondary
    }

    delegate: T.ItemDelegate {
        id: row
        required property int index
        required property string modelData
        width: ListView.view ? ListView.view.width : box.width
        height: Kit.controlHeight
        highlighted: box.highlightedIndex === index
        hoverEnabled: true
        leftPadding: 12
        contentItem: Text {
            text: row.modelData
            font: row.index === box.currentIndex ? Kit.bodyStrong : Kit.body
            color: Kit.colors.textPrimary
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Kit.radiusControl
            color: row.highlighted || row.hovered ? Kit.colors.fillSubtleHover : "transparent"
            Rectangle {
                visible: row.index === box.currentIndex
                width: 3; height: 16; radius: 1.5
                anchors.verticalCenter: parent.verticalCenter
                color: Kit.colors.accent
            }
        }
    }

    popup: T.Popup {
        y: box.height + 2
        width: box.width
        implicitHeight: Math.min(list.contentHeight + 8, 300)
        padding: 4
        contentItem: ListView {
            id: list
            WheelScroll { target: list }
            clip: true
            model: box.popup.visible ? box.delegateModel : null
            currentIndex: box.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
        }
        background: Rectangle {
            radius: 8
            color: Kit.colors.bgCard
            border.width: 1
            border.color: Kit.colors.strokeCard
        }
    }
}
