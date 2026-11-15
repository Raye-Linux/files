/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.raye.files
import org.raye.files.kit

// The right-click menu of an item shown on a page (Home, Gallery): what the
// item can do, sent to the page's model. Entries the model has no method for
// are left out.
Flyout {
    id: menu

    property var pageModel: null
    property url url
    property bool folder: false
    property bool showLocation: true
    property bool pinned: false
    property bool favorite: false
    property bool canFavorite: false
    property bool canPin: false
    property bool picture: false

    signal pinToggled()
    signal favoriteToggled()

    function popupAt(item, px, py) {
        x = px; y = py; open()
    }
    function has(name) { return pageModel && typeof pageModel[name] === "function" }
    function run(f) { menu.close(); f() }

    component Entry: CommandButton {
        Layout.fillWidth: true
        alignment: Qt.AlignLeft
    }

    contentItem: ColumnLayout {
        spacing: 0
        Entry {
            text: i18nc("@action:inmenu", "Open")
            icon.name: "document-open"
            onClicked: menu.run(() => menu.pageModel.open(menu.url))
        }
        Entry {
            text: i18nc("@action:inmenu", "Open in new tab")
            icon.name: "tab-new"
            visible: menu.folder && menu.has("openInNewTab")
            onClicked: menu.run(() => menu.pageModel.openInNewTab(menu.url))
        }
        Entry {
            text: i18nc("@action:inmenu", "Open in new window")
            icon.name: "window-new"
            visible: menu.folder && menu.has("openInNewWindow")
            onClicked: menu.run(() => menu.pageModel.openInNewWindow(menu.url))
        }
        Entry {
            text: i18nc("@action:inmenu", "Open file location")
            icon.name: "document-open-folder"
            visible: menu.showLocation && menu.has("openLocation")
            onClicked: menu.run(() => menu.pageModel.openLocation(menu.url))
        }
        Separator { vertical: false; Layout.fillWidth: true; Layout.topMargin: 4; Layout.bottomMargin: 4 }
        Entry {
            text: i18nc("@action:inmenu", "Set as desktop background")
            icon.name: "preferences-desktop-wallpaper"
            visible: menu.picture && menu.has("setAsBackground")
            onClicked: menu.run(() => menu.pageModel.setAsBackground(menu.url))
        }
        Entry {
            text: menu.pinned ? i18nc("@action:inmenu", "Unpin from Quick access") : i18nc("@action:inmenu", "Pin to Quick access")
            icon.name: "window-pin"
            visible: menu.canPin
            onClicked: menu.run(() => menu.pinToggled())
        }
        Entry {
            text: menu.favorite ? i18nc("@action:inmenu", "Remove from Favorites") : i18nc("@action:inmenu", "Add to Favorites")
            icon.name: "emblem-favorite"
            visible: menu.canFavorite
            onClicked: menu.run(() => menu.favoriteToggled())
        }
        Entry {
            text: i18nc("@action:inmenu", "Copy as path")
            icon.name: "edit-copy-path"
            visible: menu.has("copyAsPath")
            onClicked: menu.run(() => menu.pageModel.copyAsPath(menu.url))
        }
        Entry {
            text: i18nc("@action:inmenu", "Properties")
            icon.name: "document-properties"
            visible: menu.has("properties")
            onClicked: menu.run(() => menu.pageModel.properties(menu.url))
        }
    }
}
