/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Templates as T
import org.raye.files.kit

// A button with a narrow second part that opens a menu (src/raye/ui/
// splitbutton.cpp): the main part acts at once, the 28 px chevron part
// offers the other choices. One Tab stop; Alt+Down or F4 opens the menu.
FocusScope {
    id: split

    property alias text: main.text
    property alias icon: main.icon
    property string kind: "subtle"
    property string shortcut: ""
    // shown under the chevron part; anything with open() or popup()
    property var menu: null

    signal clicked()
    signal menuRequested()

    implicitWidth: main.implicitWidth + more.implicitWidth
    implicitHeight: Kit.controlHeight
    activeFocusOnTab: true

    function openMenu() {
        menuRequested()
        if (!menu)
            return
        if (menu.popup !== undefined)
            menu.popup(split, 0, split.height + 4)
        else if (menu.open !== undefined)
            menu.open()
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_F4 || (event.key === Qt.Key_Down && (event.modifiers & Qt.AltModifier))) {
            openMenu()
            event.accepted = true
        }
    }

    CommandButton {
        id: main
        focus: true
        kind: split.kind
        shortcut: split.shortcut
        join: "right"
        drawsFocusRing: false
        onClicked: split.clicked()
    }
    CommandButton {
        id: more
        x: main.width
        kind: split.kind
        join: "left"
        showChevron: true
        focusPolicy: Qt.NoFocus
        activeFocusOnTab: false
        implicitWidth: 28
        Accessible.name: i18nc("@action:button the menu part of a split button", "More options")
        onClicked: split.openMenu()
    }

    FocusRing {
        visible: main.visualFocus
    }
}
