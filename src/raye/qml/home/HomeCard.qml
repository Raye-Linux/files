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

// A Quick access card (explorer spec 1.8, 3.9): 240×56, radius 4, folder
// icon 32, name in Body, where it is in Caption; a pin glyph on pinned
// folders. Hover fills it; the pin button shows on hover.
T.AbstractButton {
    id: card

    required property int index
    required property url url
    required property string name
    required property string iconName
    required property string location
    required property bool pinned

    signal activated()
    signal pinToggled()

    property var pageModel: null

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (point) => itemMenu.popupAt(card, point.position.x, point.position.y)
    }
    Keys.onMenuPressed: itemMenu.popupAt(card, 24, card.height)
    ItemMenu {
        id: itemMenu
        pageModel: card.pageModel
        url: card.url
        folder: true
        showLocation: false
        canPin: true
        pinned: card.pinned
        onPinToggled: card.pinToggled()
    }

    implicitHeight: 56
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: name
    Accessible.description: location

    onDoubleClicked: activated()
    Keys.onReturnPressed: activated()
    Keys.onEnterPressed: activated()

    background: Rectangle {
        radius: Kit.radiusControl
        color: card.down ? Kit.colors.fillSubtlePressed : card.hovered ? Kit.colors.fillSubtleHover : "transparent"
        Behavior on color { ColorAnimation { duration: Kit.durationFaster } }
        FocusRing { visible: card.visualFocus }
    }

    contentItem: RowLayout {
        spacing: 12
        Item { implicitWidth: 0 }
        Kirigami.Icon {
            implicitWidth: 32
            implicitHeight: 32
            source: card.iconName
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: card.name
                font: Kit.body
                color: Kit.colors.textPrimary
                elide: Text.ElideRight
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                Kirigami.Icon {
                    visible: card.pinned
                    implicitWidth: 12
                    implicitHeight: 12
                    source: "pin"
                    color: Kit.colors.textSecondary
                    isMask: true
                }
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: card.location
                    font: Kit.caption
                    color: Kit.colors.textSecondary
                    elide: Text.ElideMiddle
                }
            }
        }
        CommandButton {
            visible: card.hovered || activeFocus
            icon.name: card.pinned ? "window-unpin" : "window-pin"
            text: ""
            Accessible.name: card.pinned ? i18nc("@action:button", "Unpin from Quick access") : i18nc("@action:button", "Pin to Quick access")
            onClicked: card.pinToggled()
        }
    }
}
