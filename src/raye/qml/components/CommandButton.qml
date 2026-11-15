/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.raye.files.kit

// The Raye command button for Files' pages, same look as the widget one
// (src/raye/ui/commandbutton.cpp): 32 tall; 40 wide with only an icon;
// otherwise 12 px, icon 16, 8 px, label, (8 px, chevron 12,) 12 px.
// kind: "subtle" (command bars, transparent at rest), "standard" (filled,
// with a border), "accent" (the one main action).
T.AbstractButton {
    id: button

    property string kind: "subtle"
    // a menu opens from it: draws the chevron after the label
    property bool showChevron: false
    // "Ctrl+C": shown after the name in the tooltip and read by Orca
    property string shortcut: ""

    readonly property bool iconOnly: text.length === 0 || display === T.AbstractButton.IconOnly
    readonly property bool hasIcon: icon.name.length > 0 || icon.source.toString().length > 0

    implicitHeight: Kit.controlHeight
    implicitWidth: iconOnly && !showChevron ? Kit.iconButtonWidth
                 : 12 + (hasIcon ? 16 : 0) + (hasIcon && !iconOnly ? 8 : 0)
                   + (iconOnly ? 0 : Math.ceil(label.implicitWidth))
                   + (showChevron ? (iconOnly && !hasIcon ? 12 : 20) : 0) + 12

    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    font: Kit.body

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.description: shortcut

    readonly property color _fill: {
        if (kind === "accent") {
            if (!enabled) return Kit.colors.accentDisabled
            return down ? Kit.colors.accentPressed : hovered ? Kit.colors.accentHover : Kit.colors.accent
        }
        if (kind === "standard") {
            if (!enabled) return Kit.colors.fillControlDisabled
            if (checked) return down ? Kit.colors.accentPressed : hovered ? Kit.colors.accentHover : Kit.colors.accent
            return down ? Kit.colors.fillControlPressed : hovered ? Kit.colors.fillControlHover : Kit.colors.fillControl
        }
        if (!enabled) return checked ? Kit.colors.fillSubtlePressed : "transparent"
        if (down) return Kit.colors.fillSubtlePressed
        if (checked) return Kit.colors.fillSubtleSelected
        return hovered ? Kit.colors.fillSubtleHover : "transparent"
    }
    readonly property color _ink: {
        if (!enabled) return kind === "accent" ? Kit.colors.textOnAccent : Kit.colors.textDisabled
        if (kind === "accent" || (kind === "standard" && checked)) return Kit.colors.textOnAccent
        return down && kind === "standard" ? Kit.colors.textSecondary : Kit.colors.textPrimary
    }

    // "right": the right edge meets another button and is cut square,
    // "left": the left one (SplitButton). The left part draws the shared edge.
    property string join: "none"

    background: Item {
        implicitWidth: button.implicitWidth
        implicitHeight: Kit.controlHeight
        clip: button.join !== "none"

        Rectangle {
            id: plate
            x: button.join === "left" ? -Kit.radiusControl : 0
            width: parent.width + (button.join === "none" ? 0 : Kit.radiusControl)
            height: parent.height
            radius: Kit.radiusControl
            color: button._fill
            border.width: button.kind === "subtle" ? 0 : 1
            border.color: button.kind === "accent" ? Kit.colors.edgeLit : Kit.colors.strokeControl
            Behavior on color {
                ColorAnimation { duration: Kit.durationFaster }
            }
        }
        // the shared edge of a split button
        Rectangle {
            visible: button.join === "right" && button.kind !== "subtle"
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            width: 1
            color: button.kind === "accent" ? Kit.alpha(Kit.colors.textOnAccent, 0.3) : Kit.colors.strokeControl
        }
        // the bottom pixel row of a filled button is a shade stronger
        Rectangle {
            visible: button.kind === "standard" && !button.down && button.enabled
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom
                      leftMargin: button.join === "left" ? 0 : Kit.radiusControl
                      rightMargin: button.join === "right" ? 0 : Kit.radiusControl }
            height: 1
            color: Kit.colors.strokeControlBottom
        }
    }

    // false when a parent draws one ring around several parts (SplitButton)
    property bool drawsFocusRing: true

    // outside the background, which clips when joined
    FocusRing {
        visible: button.drawsFocusRing && button.visualFocus
    }

    // Qt.AlignHCenter (buttons) or Qt.AlignLeft (rows of a list or a flyout)
    property int alignment: Qt.AlignHCenter
    // Command glyphs take the ink colour; false keeps a coloured icon as drawn.
    property bool tintIcon: true

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.verticalCenter: parent.verticalCenter
            x: button.alignment & Qt.AlignLeft
               ? (button.mirrored ? parent.width - width - 12 : 12)
               : Math.round((parent.width - width) / 2)
            layoutDirection: button.mirrored ? Qt.RightToLeft : Qt.LeftToRight
            spacing: 8
            Kirigami.Icon {
                visible: button.hasIcon
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                source: button.icon.name.length > 0 ? button.icon.name : button.icon.source
                color: button._ink
                isMask: button.tintIcon
                opacity: button.enabled || button.kind === "accent" ? 1 : 0.45
            }
            Text {
                id: label
                visible: !button.iconOnly
                anchors.verticalCenter: parent.verticalCenter
                text: button.text
                font: button.font
                color: button._ink
            }
            // The same 8 x 4 chevron the widgets draw (paintChevron).
            Canvas {
                id: chevron
                visible: button.showChevron
                anchors.verticalCenter: parent.verticalCenter
                width: 12
                height: 12
                property color ink: button._ink
                onInkChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = ink
                    ctx.lineWidth = 1.2
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(2, 4)
                    ctx.lineTo(6, 8)
                    ctx.lineTo(10, 4)
                    ctx.stroke()
                }
            }
        }
    }
}
