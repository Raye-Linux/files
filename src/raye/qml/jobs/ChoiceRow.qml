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

// A large choice of a Files dialog: 48 px or more, an icon, the action in
// Body strong and a secondary line under it; hover fill, radius 4.
T.AbstractButton {
    id: row

    property string iconName: ""
    property string title: ""
    property string line: ""

    implicitHeight: Math.max(48, content.implicitHeight + 12)
    implicitWidth: content.implicitWidth + 24
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: line

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    background: Rectangle {
        radius: Kit.radiusControl
        color: row.down ? Kit.colors.fillSubtlePressed : row.hovered ? Kit.colors.fillSubtleHover : "transparent"
        Behavior on color { ColorAnimation { duration: Kit.durationFaster } }
        FocusRing {
            visible: row.visualFocus
        }
    }

    contentItem: Item {
        RowLayout {
            id: content
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12
            Kirigami.Icon {
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 24
                implicitHeight: 24
                source: row.iconName
                color: Kit.colors.accentText
                isMask: true
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 0
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: row.title
                    font: Kit.bodyStrong
                    color: Kit.colors.textPrimary
                    elide: Text.ElideRight
                }
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: row.line.length > 0
                    text: row.line
                    font: Kit.caption
                    color: Kit.colors.textSecondary
                    elide: Text.ElideMiddle
                }
            }
        }
    }
}
