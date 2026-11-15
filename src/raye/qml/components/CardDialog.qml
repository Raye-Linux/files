/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Effects
import QtQuick.Templates as T
import org.raye.files.kit

// The Raye dialog inside a page (src/raye/ui/carddialog.cpp for a window of
// its own): radius 8, padding 24, the title in Subtitle, the content, then
// an 80 px button row on the base colour with up to three buttons of equal
// width, 8 px apart. The page behind it dims. Enter presses the default
// button, Esc is Close.
T.Dialog {
    id: dialog

    property string primaryText: ""
    property string secondaryText: ""
    property string closeText: ""
    // "primary", "secondary" or "close" (no accent button)
    property string defaultButton: "primary"

    signal primaryClicked()
    signal secondaryClicked()
    signal closeClicked()

    readonly property int _buttons: (primaryText ? 1 : 0) + (secondaryText ? 1 : 0) + (closeText ? 1 : 0)

    modal: true
    focus: true
    anchors.centerIn: T.Overlay.overlay
    closePolicy: T.Popup.CloseOnEscape
    padding: 24
    topPadding: _hasHeader ? 12 : 24
    readonly property bool _hasHeader: title.length > 0
    readonly property bool _hasFooter: _buttons > 0
    implicitWidth: Math.min(548, Math.max(320, implicitContentWidth + leftPadding + rightPadding))
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
                    + (_hasHeader ? implicitHeaderHeight : 0) + (_hasFooter ? implicitFooterHeight : 0)

    T.Overlay.modal: Rectangle {
        color: Kit.colors.bgSmoke
        Behavior on opacity { NumberAnimation { duration: Kit.durationFast } }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Kit.durationNormal; easing.type: Easing.OutQuint }
            NumberAnimation { property: "scale"; from: 1.05; to: 1; duration: Kit.durationNormal; easing.type: Easing.OutQuint }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Kit.durationFaster }
    }

    onRejected: closeClicked()

    function press(which) {
        if (which === "primary") { primaryClicked(); accept() }
        else if (which === "secondary") { secondaryClicked(); done(2) }
        else { reject() }
    }

    header: Text {
        visible: dialog._hasHeader
        text: dialog.title
        font: Kit.subtitle
        color: Kit.colors.textPrimary
        wrapMode: Text.Wrap
        topPadding: 24
        leftPadding: 24
        rightPadding: 24
        Accessible.role: Accessible.Heading
    }

    footer: Item {
        visible: dialog._hasFooter
        implicitHeight: 80
        // inside the card's 1 px outline; round at the bottom, square on top
        Rectangle {
            id: plate
            anchors { fill: parent; leftMargin: 1; rightMargin: 1; bottomMargin: 1 }
            radius: Kit.radiusOverlay - 1
            color: Kit.colors.bgBase
        }
        Rectangle {
            anchors { left: plate.left; right: plate.right; top: plate.top }
            height: Kit.radiusOverlay
            color: plate.color
        }
        Rectangle {
            anchors { left: plate.left; right: plate.right; top: plate.top }
            height: 1
            color: Kit.colors.strokeCardSolid
        }
        Row {
            id: buttons
            readonly property real cell: (width - (dialog._buttons - 1) * spacing) / Math.max(1, dialog._buttons)
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 24; rightMargin: 24 }
            spacing: 8
            CommandButton {
                visible: dialog.primaryText.length > 0
                width: buttons.cell
                text: dialog.primaryText
                kind: dialog.defaultButton === "primary" ? "accent" : "standard"
                focusPolicy: Qt.StrongFocus
                onClicked: dialog.press("primary")
            }
            CommandButton {
                visible: dialog.secondaryText.length > 0
                width: buttons.cell
                text: dialog.secondaryText
                kind: dialog.defaultButton === "secondary" ? "accent" : "standard"
                focusPolicy: Qt.StrongFocus
                onClicked: dialog.press("secondary")
            }
            CommandButton {
                visible: dialog.closeText.length > 0
                width: buttons.cell
                text: dialog.closeText
                kind: "standard"
                focusPolicy: Qt.StrongFocus
                onClicked: dialog.press("close")
            }
        }
    }

    background: Item {
        RectangularShadow {
            anchors.fill: card
            radius: Kit.radiusOverlay
            blur: Kit.night ? 56 : 40
            offset.y: Kit.night ? 18 : 16
            color: Kit.alpha(Kit.colors.shadow, Kit.night ? 0.5 : 0.22)
        }
        Rectangle {
            id: card
            anchors.fill: parent
            radius: Kit.radiusOverlay
            color: Kit.colors.bgCardSolid
            border.width: 1
            border.color: Kit.colors.strokeSurfaceSolid
        }
    }

    // Enter presses the default button (a Popup has no Keys of its own).
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: dialog.opened && dialog.defaultButton !== "close"
                 && (dialog.defaultButton === "primary" ? dialog.primaryText : dialog.secondaryText).length > 0
        onActivated: dialog.press(dialog.defaultButton)
    }
}
