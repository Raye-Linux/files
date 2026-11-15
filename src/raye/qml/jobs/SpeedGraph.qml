/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import org.raye.files.kit

// The speed graph of a copy (explorer spec 3.13): the throughput as an
// area filled with an accent-to-glow gradient at 35 %, a 2 px accent line
// on top, and a dashed line at the current speed with its label. New
// samples arrive four times a second; the graph slides between them.
Item {
    id: graph

    // bytes per second, oldest first, one point per 250 ms
    property var history: []
    property real speed: 0
    property real peak: 0
    property string label: ""
    // how many points fill the width
    property int span: 120

    implicitHeight: 96

    Rectangle {
        anchors.fill: parent
        radius: Kit.radiusControl
        color: Kit.colors.fillControl
        border.width: 1
        border.color: Kit.colors.strokeCard
    }

    readonly property real _top: Math.max(peak, speed, 1) * 1.15

    Canvas {
        id: canvas
        anchors.fill: parent
        anchors.margins: 1
        property color accent: Kit.colors.accent
        property color glow: Kit.colors.glow
        property color ink: Kit.colors.textSecondary
        onAccentChanged: requestPaint()
        Connections {
            target: graph
            function onHistoryChanged() { canvas.requestPaint() }
            function onSpeedChanged() { canvas.requestPaint() }
            function onWidthChanged() { canvas.requestPaint() }
        }
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const h = graph.history || []
            const n = h.length
            if (n < 2) return
            const step = width / Math.max(graph.span - 1, 1)
            const x0 = Math.max(0, width - (n - 1) * step)
            const first = Math.max(0, n - graph.span)
            const yOf = v => height - (v / graph._top) * (height - 6)

            ctx.beginPath()
            ctx.moveTo(x0, height)
            for (let i = first; i < n; ++i)
                ctx.lineTo(x0 + (i - first) * step, yOf(h[i]))
            ctx.lineTo(x0 + (n - 1 - first) * step, height)
            ctx.closePath()
            const g = ctx.createLinearGradient(0, 0, 0, height)
            g.addColorStop(0, Qt.rgba(accent.r, accent.g, accent.b, 0.35))
            g.addColorStop(1, Qt.rgba(glow.r, glow.g, glow.b, 0.35 * 0.4))
            ctx.fillStyle = g
            ctx.fill()

            ctx.beginPath()
            for (let i = first; i < n; ++i) {
                const x = x0 + (i - first) * step
                if (i === first) ctx.moveTo(x, yOf(h[i]))
                else ctx.lineTo(x, yOf(h[i]))
            }
            ctx.strokeStyle = accent
            ctx.lineWidth = 2
            ctx.lineJoin = "round"
            ctx.stroke()

            if (graph.speed > 0) {
                const y = Math.round(yOf(graph.speed)) + 0.5
                ctx.beginPath()
                ctx.setLineDash([4, 3])
                ctx.moveTo(0, y)
                ctx.lineTo(width, y)
                ctx.strokeStyle = Qt.rgba(ink.r, ink.g, ink.b, 0.8)
                ctx.lineWidth = 1
                ctx.stroke()
            }
        }
    }

    Text {
        textFormat: Text.PlainText
        visible: graph.label.length > 0
        anchors.right: parent.right
        anchors.rightMargin: 8
        y: {
            const line = graph.height - (graph.speed / graph._top) * (graph.height - 6)
            return Math.max(4, Math.min(graph.height - height - 4, line - height - 2))
        }
        text: graph.label
        font: Kit.caption
        color: Kit.colors.textPrimary
    }
}
