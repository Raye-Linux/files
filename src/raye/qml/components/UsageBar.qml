/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import org.raye.files.kit

// How full a drive is: 4 px, radius 2, accent to glow; over 90 % used it
// turns to the caution colour. It grows from empty when it first shows.
Item {
    id: bar

    // used part, 0..1
    property real value: 0
    // false: show the value at once (lists that scroll, tests)
    property bool animateOnShow: true
    readonly property bool caution: value > 0.9

    property real _shown: animateOnShow ? 0 : value
    // the glow token without its alpha: the end of the gradient
    readonly property color _glow: Qt.rgba(Kit.colors.glow.r, Kit.colors.glow.g, Kit.colors.glow.b, 1)

    implicitWidth: 240
    implicitHeight: 4

    Accessible.role: Accessible.ProgressBar
    Accessible.name: Math.round(value * 100) + " %"

    Component.onCompleted: _shown = Qt.binding(() => Math.max(0, Math.min(1, bar.value)))

    Behavior on _shown {
        enabled: bar.animateOnShow
        NumberAnimation {
            duration: Math.round(400 * Kit.durationNormal / 250)
            easing.type: Easing.OutQuint
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 2
        color: Kit.alpha(Kit.colors.strokeStrong, 0.35)
    }
    Rectangle {
        width: Math.round(parent.width * bar._shown)
        height: parent.height
        radius: 2
        visible: width > 0
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: bar.caution ? Kit.colors.statusCaution : Kit.colors.accent }
            GradientStop { position: 1; color: bar.caution ? Kit.colors.statusCaution : bar._glow }
        }
    }
}
