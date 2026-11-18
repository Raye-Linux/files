/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.raye.files.kit

// A group of Folder options on its own card: an icon and a title, an
// optional line under it, then the choices, lined up under the title.
Rectangle {
    id: group

    property string title: ""
    property string description: ""
    property string iconName: ""
    default property alias content: rows.data

    Layout.fillWidth: true
    implicitHeight: inner.implicitHeight + 24
    radius: Kit.radiusOverlay
    color: Kit.colors.bgCard
    border.width: 1
    border.color: Kit.colors.strokeCard

    Accessible.role: Accessible.Grouping
    Accessible.name: title

    ColumnLayout {
        id: inner
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Kirigami.Icon {
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 1
                implicitWidth: 20
                implicitHeight: 20
                visible: group.iconName.length > 0
                source: group.iconName
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: group.title
                    font: Kit.bodyStrong
                    color: Kit.colors.textPrimary
                    elide: Text.ElideRight
                    Accessible.role: Accessible.Heading
                }
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: group.description.length > 0
                    text: group.description
                    font: Kit.caption
                    color: Kit.colors.textSecondary
                    wrapMode: Text.Wrap
                }
            }
        }

        ColumnLayout {
            id: rows
            Layout.fillWidth: true
            Layout.leftMargin: group.iconName.length > 0 ? 32 : 0
            spacing: 0
        }
    }
}
