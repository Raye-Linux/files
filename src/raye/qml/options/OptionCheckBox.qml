/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Templates as T
import org.raye.files
import org.raye.files.kit

// The check box of Files' dialogs (progress, conflicts, Folder options):
// a 20 px box, radius 4, filled with the accent and a check mark when
// ticked; the label after it in Body. A label that wraps makes the box
// taller, with the box beside its first line.
T.CheckBox {
    id: box

    implicitWidth: implicitIndicatorWidth + spacing + implicitContentWidth
    implicitHeight: Math.max(Kit.controlHeight, implicitContentHeight + topPadding + bottomPadding)
    spacing: 8
    font: Kit.body
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.CheckBox
    Accessible.name: text

    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        x: box.leftPadding
        y: box.topPadding + Math.round((Kit.controlHeight - box.topPadding - box.bottomPadding - height) / 2)
        radius: Kit.radiusControl
        color: box.checked ? (box.down ? Kit.colors.accentPressed : box.hovered ? Kit.colors.accentHover : Kit.colors.accent)
                           : (box.down ? Kit.colors.fillControlPressed : box.hovered ? Kit.colors.fillControlHover : Kit.colors.fillControl)
        border.width: box.checked ? 0 : 1
        border.color: Kit.colors.strokeStrong
        opacity: box.enabled ? 1 : 0.45
        Behavior on color { ColorAnimation { duration: Kit.durationFaster } }

        Canvas {
            anchors.fill: parent
            visible: box.checked
            property color ink: Kit.colors.textOnAccent
            onInkChanged: requestPaint()
            onVisibleChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = ink
                ctx.lineWidth = 1.6
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(5.5, 10.5)
                ctx.lineTo(8.5, 13.5)
                ctx.lineTo(14.5, 7)
                ctx.stroke()
            }
        }
        FocusRing {
            visible: box.visualFocus
        }
    }

    contentItem: Text {
        textFormat: Text.PlainText
        leftPadding: box.indicator.width + box.spacing
        verticalAlignment: Text.AlignVCenter
        text: box.text
        font: box.font
        color: box.enabled ? Kit.colors.textPrimary : Kit.colors.textDisabled
        wrapMode: Text.Wrap
    }
}
