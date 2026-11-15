/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.raye.files
import org.raye.files.kit

// A row of Home's Recent or Favorites list: 32 tall like the Details view,
// icon 16 (or a tiny picture), Name, Date, Location; on hover "Open file
// location" and the Favorites star (filled when the file is a favorite).
T.AbstractButton {
    id: row

    required property url url
    required property string name
    required property string iconName
    required property url thumbnail
    required property string date
    required property string location
    required property bool favorite

    property var pageModel: null

    signal activated()
    signal locationRequested()
    signal favoriteToggled()

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (point) => itemMenu.popupAt(row, point.position.x, point.position.y)
    }
    Keys.onMenuPressed: itemMenu.popupAt(row, 24, row.height)
    ItemMenu {
        id: itemMenu
        pageModel: row.pageModel
        url: row.url
        favorite: row.favorite
        canFavorite: true
        onFavoriteToggled: row.favoriteToggled()
    }

    implicitHeight: 32
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.ListItem
    Accessible.name: name
    Accessible.description: location

    onDoubleClicked: activated()
    Keys.onReturnPressed: activated()

    background: Rectangle {
        radius: Kit.radiusControl
        color: row.down ? Kit.colors.fillSubtlePressed : row.hovered ? Kit.colors.fillSubtleHover : "transparent"
        FocusRing { visible: row.visualFocus }
    }

    contentItem: Item {
        Item {
            x: 12
            width: 16
            height: 16
            anchors.verticalCenter: parent.verticalCenter
            Kirigami.Icon {
                anchors.fill: parent
                visible: tiny.status !== Image.Ready
                source: row.iconName
            }
            Image {
                id: tiny
                anchors.fill: parent
                source: row.thumbnail
                sourceSize: Qt.size(32, 32)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
        }
        HomeColumns {
            anchors.fill: parent
            nameIndent: 24
            name: row.name
            date: row.date
            location: row.location
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0
            visible: row.hovered || row.activeFocus || row.favorite
            CommandButton {
                visible: row.hovered || row.activeFocus
                implicitHeight: 28
                icon.name: "document-open-folder"
                text: ""
                Accessible.name: i18nc("@action:button", "Open file location")
                onClicked: row.locationRequested()
            }
            CommandButton {
                implicitHeight: 28
                icon.name: row.favorite ? "starred-symbolic" : "non-starred-symbolic"
                text: ""
                Accessible.name: row.favorite ? i18nc("@action:button", "Remove from Favorites") : i18nc("@action:button", "Add to Favorites")
                onClicked: row.favoriteToggled()
            }
        }
    }
}
