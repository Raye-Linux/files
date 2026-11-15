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

// A section title of Home, "Quick access ⌄": Body strong 16 like Windows'
// section headers; the chevron folds the section.
T.AbstractButton {
    id: header

    property bool expanded: true
    property bool expandable: true
    signal foldToggled()

    Layout.bottomMargin: 8
    implicitWidth: row.implicitWidth + 16
    implicitHeight: 36
    hoverEnabled: expandable
    focusPolicy: expandable ? Qt.StrongFocus : Qt.NoFocus
    Accessible.role: expandable ? Accessible.Button : Accessible.Heading
    Accessible.name: text
    onClicked: if (expandable) foldToggled()

    background: Rectangle {
        radius: Kit.radiusControl
        color: header.expandable && header.down ? Kit.colors.fillSubtlePressed : header.expandable && header.hovered ? Kit.colors.fillSubtleHover : "transparent"
        x: -8
        width: parent.width
        FocusRing { visible: header.visualFocus }
    }
    contentItem: Item {
        RowLayout {
            id: row
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            Text {
                textFormat: Text.PlainText
                text: header.text
                font.family: Kit.bodyStrong.family
                font.weight: Font.DemiBold
                font.pixelSize: 16
                color: Kit.colors.textPrimary
            }
            Kirigami.Icon {
                visible: header.expandable
                implicitWidth: 12
                implicitHeight: 12
                source: "go-down-symbolic"
                color: Kit.colors.textSecondary
                isMask: true
                rotation: header.expanded ? 0 : -90
                Behavior on rotation { NumberAnimation { duration: Kit.durationFast; easing.type: Easing.OutCubic } }
            }
        }
    }
}
