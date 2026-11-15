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

// A drive of This PC: 272×72, icon 48, name, a 4 px usage bar and
// "123 GB free of 476 GB". Opens with a double click or Enter.
T.AbstractButton {
    id: tile

    required property url url
    required property string name
    required property string iconName
    required property real usage
    required property string spaceText

    // Filled by the page; empty when the tile shows only its own data.
    required property string section
    required property bool removable
    required property bool mapped
    required property bool connected
    property var pageModel: null

    signal activated()
    signal propertiesRequested()

    readonly property bool isLocal: url.toString().startsWith("file:")
    readonly property bool isComputer: section === "computers"

    implicitHeight: 72
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: name
    Accessible.description: spaceText

    onDoubleClicked: activated()

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: menu.popup()
    }
    Keys.onMenuPressed: menu.popup()

    component MenuEntry: CommandButton {
        Layout.fillWidth: true
        alignment: Qt.AlignLeft
    }

    Flyout {
        id: menu
        function popup() { x = tile.width / 2; y = tile.height - 8; open() }
        function run(f) { menu.close(); f() }
        contentItem: ColumnLayout {
            spacing: 0
            MenuEntry {
                text: i18nc("@action:inmenu", "Open")
                icon.name: "document-open"
                onClicked: menu.run(() => tile.activated())
            }
            MenuEntry {
                text: i18nc("@action:inmenu", "Open in new tab")
                icon.name: "tab-new"
                visible: tile.pageModel !== null && tile.connected
                onClicked: menu.run(() => tile.pageModel.openInNewTab(tile.url))
            }
            MenuEntry {
                text: i18nc("@action:inmenu", "Open in new window")
                icon.name: "window-new"
                visible: tile.pageModel !== null && tile.connected
                onClicked: menu.run(() => tile.pageModel.openInNewWindow(tile.url))
            }
            MenuEntry {
                text: tile.pageModel && tile.pageModel.isPinned(tile.url) ? i18nc("@action:inmenu", "Unpin from Quick access")
                                                                          : i18nc("@action:inmenu", "Pin to Quick access")
                icon.name: "window-pin"
                visible: tile.pageModel !== null && tile.connected && !tile.isComputer
                onClicked: menu.run(() => tile.pageModel.pinToQuickAccess(tile.url))
            }
            Separator { vertical: false; Layout.fillWidth: true; Layout.topMargin: 4; Layout.bottomMargin: 4; visible: tile.pageModel !== null }
            MenuEntry {
                text: i18nc("@action:inmenu", "Map network drive…")
                icon.name: "folder-network"
                visible: tile.pageModel !== null && tile.isComputer
                onClicked: menu.run(() => tile.pageModel.mapNetworkDrive(tile.url))
            }
            MenuEntry {
                text: i18nc("@action:inmenu remove a network drive", "Disconnect")
                icon.name: "network-disconnect"
                visible: tile.pageModel !== null && tile.mapped
                onClicked: menu.run(() => tile.pageModel.disconnectDrive(tile.url))
            }
            MenuEntry {
                text: i18nc("@action:inmenu eject a drive or disc", "Eject")
                icon.name: "media-eject"
                visible: tile.pageModel !== null && tile.section === "drives" && tile.removable
                onClicked: menu.run(() => tile.pageModel.eject(tile.url))
            }
            MenuEntry {
                text: i18nc("@action:inmenu erase a drive and prepare it", "Format…")
                icon.name: "drive-harddisk"
                visible: tile.pageModel !== null && tile.section === "drives" && tile.pageModel.canFormat()
                onClicked: menu.run(() => tile.pageModel.format())
            }
            MenuEntry {
                text: i18nc("@action:inmenu", "Copy as path")
                icon.name: "edit-copy-path"
                visible: tile.pageModel !== null && !tile.isComputer
                onClicked: menu.run(() => tile.pageModel.copyAsPath(tile.url))
            }
            MenuEntry {
                text: i18nc("@action:inmenu", "Properties")
                icon.name: "document-properties"
                onClicked: menu.run(() => tile.propertiesRequested())
            }
        }
    }
    Keys.onReturnPressed: activated()
    Keys.onEnterPressed: activated()

    background: Rectangle {
        radius: Kit.radiusControl
        color: tile.down ? Kit.colors.fillSubtlePressed : tile.hovered ? Kit.colors.fillSubtleHover : "transparent"
        Behavior on color { ColorAnimation { duration: Kit.durationFaster } }
        FocusRing { visible: tile.visualFocus }
    }

    contentItem: RowLayout {
        spacing: 12
        Item { implicitWidth: 0 }
        Kirigami.Icon {
            implicitWidth: 48
            implicitHeight: 48
            source: tile.iconName
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.rightMargin: 12
            spacing: 4
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: tile.name
                font: Kit.body
                color: Kit.colors.textPrimary
                elide: Text.ElideRight
            }
            UsageBar {
                Layout.fillWidth: true
                visible: tile.spaceText.length > 0 && tile.connected
                value: tile.usage
            }
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                visible: text.length > 0
                text: tile.spaceText
                font: Kit.caption
                color: Kit.colors.textSecondary
                elide: Text.ElideRight
            }
        }
    }
}
