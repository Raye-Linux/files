/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Effects
import QtQuick.Templates as T
import org.raye.files.kit

// A Raye popup inside a page (src/raye/ui/flyout.cpp for windows of their
// own): radius 8, 1 px stroke.surface, the flyout shadow, 4 px of padding.
// It comes in with the Raye popup motion, a 6 px slide from its anchor's
// side and a fade (167 ms), and leaves in 83 ms. Esc and a click outside
// close it.
T.Popup {
    id: flyout

    // "bottom": it opened under its anchor and slides down into place,
    // "top": above it, sliding up.
    property string openFrom: "bottom"
    property real _slide: 0

    padding: 4
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Kit.durationFast; easing.type: Easing.OutCubic }
            NumberAnimation { property: "_slide"; from: flyout.openFrom === "top" ? 6 : -6; to: 0; duration: Kit.durationFast; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Kit.durationFaster }
    }

    contentItem.transform: Translate { y: flyout._slide }

    background: Item {
        implicitWidth: 2 * Kit.radiusOverlay
        implicitHeight: 2 * Kit.radiusOverlay
        transform: Translate { y: flyout._slide }

        RectangularShadow {
            anchors.fill: card
            radius: Kit.radiusOverlay
            blur: Kit.night ? 28 : 20
            offset.y: Kit.night ? 10 : 8
            color: Kit.alpha(Kit.colors.shadow, Kit.night ? 0.4 : 0.16)
        }
        Rectangle {
            id: card
            anchors.fill: parent
            radius: Kit.radiusOverlay
            // Inside a window there is nothing to blur: the solid tint.
            color: Kit.colors.acrylicTintSolid
            border.width: 1
            border.color: Kit.colors.strokeSurface
        }
    }
}
