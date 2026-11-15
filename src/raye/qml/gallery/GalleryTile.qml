/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Templates as T
import org.raye.files
import org.raye.files.kit

// One picture or video of Gallery, cut to its place in the row with round
// corners; a video shows its length. The thumbnail fades in once loaded.
T.AbstractButton {
    id: tile

    required property var modelData
    readonly property url url: modelData.url
    readonly property bool video: modelData.video
    // what the corners are cut out of
    property color pageColor: Qt.tint(Kit.colors.bgLayerSolid, Kit.colors.bgLayer)

    property var pageModel: null

    signal activated()

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (point) => itemMenu.popupAt(tile, point.position.x, point.position.y)
    }
    Keys.onMenuPressed: itemMenu.popupAt(tile, tile.width / 2, tile.height / 2)
    ItemMenu {
        id: itemMenu
        pageModel: tile.pageModel
        url: tile.url
        picture: !tile.video
    }

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: modelData.name

    onDoubleClicked: activated()
    Keys.onReturnPressed: activated()
    Keys.onEnterPressed: activated()

    background: Item {
        Rectangle {
            anchors.fill: parent
            color: Kit.colors.fillControl
        }
        Image {
            id: thumb
            anchors.fill: parent
            source: tile.video ? "image://rayethumb/" + tile.url : tile.url
            sourceSize.height: Math.ceil(tile.height)
            asynchronous: true
            cache: false
            fillMode: Image.PreserveAspectCrop
            opacity: status === Image.Ready ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Kit.durationFast } }
        }
        // The corners: a frame of the page colour outside a round hole.
        // Clipping to a radius needs a GPU effect, this works everywhere.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: Kit.radiusOverlay + 4
            color: "transparent"
            border.width: 4
            border.color: tile.pageColor
        }
        Rectangle {
            visible: tile.video
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 6
            width: length.implicitWidth + 12
            height: 20
            radius: 4
            color: Qt.alpha(Kit.colors.shadow, 0.6)
            Text {
                id: length
                anchors.centerIn: parent
                textFormat: Text.PlainText
                text: "▶ " + (tile.modelData.duration.length > 0 ? tile.modelData.duration : "")
                font: Kit.caption
                color: Kit.colors.textOnAccent
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: Kit.radiusOverlay
            color: "transparent"
            border.width: tile.hovered || tile.visualFocus ? 2 : 0
            border.color: tile.visualFocus ? Kit.colors.textPrimary : Kit.colors.accent
        }
    }
}
