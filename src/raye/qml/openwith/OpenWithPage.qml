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

// The body of "Select an app to open this file" (explorer spec 3.15): the
// file, the recommended apps then every other app in one list that scrolls,
// "Look for an app on this PC" and "Find an app in the Store". Always, Just
// once and Cancel are the card's buttons. A double-click on an app is Just
// once.
//
// The list has one height whatever it holds, so the page has its full height
// the moment it loads: the card can open at its size and never resizes.
// Growing a window just after mapping it loses against the compositor on
// Wayland, and the card then stays too short for its buttons.
Item {
    id: page

    property var model: null
    property string fileName: ""
    property string fileIcon: "unknown"
    // false when choosing the default app for a type, with no file
    property bool forFile: true
    // the Store is installed
    property bool storeAvailable: true

    signal browseRequested()
    signal storeRequested()
    signal activated()

    implicitWidth: 472
    implicitHeight: column.implicitHeight

    function focusField() {
        list.forceActiveFocus()
    }

    readonly property int recommended: model ? model.recommendedCount : 0

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Kirigami.Icon {
                implicitWidth: 32
                implicitHeight: 32
                source: page.fileIcon
            }
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: page.fileName
                font: Kit.bodyStrong
                color: Kit.colors.textPrimary
                elide: Text.ElideMiddle
            }
        }

        Text {
            id: heading
            textFormat: Text.PlainText
            text: page.recommended > 0 ? i18nc("@title:group", "Recommended apps") : i18nc("@title:group", "Apps on this PC")
            font: Kit.caption
            color: Kit.colors.textSecondary
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: 240
            clip: true
            interactive: contentHeight > height
            model: page.model
            currentIndex: page.model ? page.model.currentIndex : -1
            keyNavigationEnabled: true
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: true
            Accessible.role: Accessible.List
            Accessible.name: heading.text

            T.ScrollBar.vertical: T.ScrollBar {
                id: scroller
                visible: list.count > 0 && list.contentHeight > list.height
                implicitWidth: 6
                padding: 1
                contentItem: Rectangle {
                    radius: 2
                    implicitWidth: 4
                    color: Kit.colors.textSecondary
                    opacity: scroller.active ? 0.8 : 0.4
                }
            }

            Text {
                textFormat: Text.PlainText
                visible: list.count === 0
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                text: i18nc("@info", "No app on this PC opens this kind of file.")
                font: Kit.body
                color: Kit.colors.textSecondary
                wrapMode: Text.Wrap
            }

            onCurrentIndexChanged: if (page.model && currentIndex !== page.model.currentIndex) page.model.currentIndex = currentIndex
            Keys.onReturnPressed: page.activated()

            section.property: "recommended"
            section.delegate: Item {
                required property string section
                width: ListView.view.width
                readonly property bool shown: section === "false" && page.recommended > 0
                height: shown ? 36 : 0
                visible: shown
                clip: true
                Text {
                    textFormat: Text.PlainText
                    visible: parent.shown
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 6
                    text: i18nc("@title:group", "More apps")
                    font: Kit.caption
                    color: Kit.colors.textSecondary
                }
            }

            delegate: T.ItemDelegate {
                id: row
                required property int index
                required property string name
                required property string iconName
                required property bool recommended
                required property bool isDefault
                readonly property bool selected: ListView.isCurrentItem

                width: ListView.view.width
                height: 40
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                Accessible.role: Accessible.ListItem
                Accessible.name: name
                Accessible.selected: selected

                onClicked: list.currentIndex = index
                onDoubleClicked: {
                    list.currentIndex = index
                    page.activated()
                }

                background: Rectangle {
                    radius: Kit.radiusControl
                    color: row.selected ? Kit.colors.fillSubtleSelected : row.hovered ? Kit.colors.fillSubtleHover : "transparent"
                    Rectangle {
                        visible: row.selected
                        width: 3
                        height: 16
                        radius: 1.5
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        color: Kit.colors.accent
                    }
                }
                contentItem: RowLayout {
                    spacing: 12
                    Item { implicitWidth: 4 }
                    Kirigami.Icon {
                        implicitWidth: 24
                        implicitHeight: 24
                        source: row.iconName
                    }
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: row.name
                        font: Kit.body
                        color: Kit.colors.textPrimary
                        elide: Text.ElideRight
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: row.isDefault
                        text: i18nc("@info the app now used for this type", "Current default")
                        font: Kit.caption
                        color: Kit.colors.textSecondary
                        rightPadding: 8
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            CommandButton {
                Layout.fillWidth: true
                alignment: Qt.AlignLeft
                text: i18nc("@action:button", "Look for an app on this PC")
                icon.name: "document-open-folder"
                onClicked: page.browseRequested()
            }
            CommandButton {
                visible: page.storeAvailable
                Layout.fillWidth: true
                alignment: Qt.AlignLeft
                text: i18nc("@action:button", "Find an app in the Store")
                icon.name: "plasmadiscover"
                onClicked: page.storeRequested()
            }
        }
    }
}
