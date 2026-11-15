/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.raye.files
import org.raye.files.kit

// The body of the delete confirmation (explorer spec 3.11): the question,
// then for one item its icon, name, type, size and date modified, in a
// Raye card. Yes / No are the card's buttons.
Item {
    id: page

    property string question: ""
    property int count: 1
    property string iconName: "unknown"
    property string thumbnail: ""
    property string name: ""
    property string typeText: ""
    property string sizeText: ""
    property string dateText: ""

    implicitWidth: 472
    implicitHeight: column.implicitHeight

    function focusField() {
    }

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 16

        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: page.question
            font: Kit.body
            color: Kit.colors.textPrimary
            wrapMode: Text.Wrap
        }

        Rectangle {
            visible: page.count === 1 && page.name.length > 0
            Layout.fillWidth: true
            implicitHeight: info.implicitHeight + 24
            radius: Kit.radiusControl
            color: Kit.colors.bgCard
            border.width: 1
            border.color: Kit.colors.strokeCard
            RowLayout {
                id: info
                anchors.fill: parent
                anchors.margins: 12
                spacing: 16
                Item {
                    Layout.alignment: Qt.AlignTop
                    implicitWidth: 48
                    implicitHeight: 48
                    Kirigami.Icon {
                        anchors.fill: parent
                        visible: thumb.status !== Image.Ready
                        source: page.iconName
                    }
                    Image {
                        id: thumb
                        anchors.fill: parent
                        source: page.thumbnail
                        sourceSize: Qt.size(96, 96)
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: page.name
                        font: Kit.bodyStrong
                        color: Kit.colors.textPrimary
                        elide: Text.ElideMiddle
                    }
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        visible: page.typeText.length > 0
                        text: i18nc("@label", "Type: %1", page.typeText)
                        font: Kit.caption
                        color: Kit.colors.textSecondary
                        elide: Text.ElideRight
                    }
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        visible: page.sizeText.length > 0
                        text: i18nc("@label", "Size: %1", page.sizeText)
                        font: Kit.caption
                        color: Kit.colors.textSecondary
                    }
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        visible: page.dateText.length > 0
                        text: i18nc("@label", "Date modified: %1", page.dateText)
                        font: Kit.caption
                        color: Kit.colors.textSecondary
                    }
                }
            }
        }
    }
}
