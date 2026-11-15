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

// A Continue card (a Raye extra of explorer spec 3.9): a large preview of
// the file (its picture, or its icon on a soft accent field), its name and
// when it was last opened.
T.AbstractButton {
    id: card

    required property url url
    required property string name
    required property string iconName
    required property url thumbnail
    required property string date

    signal activated()

    implicitHeight: 176
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: name

    onClicked: activated()
    Keys.onReturnPressed: activated()

    background: Rectangle {
        radius: Kit.radiusOverlay
        color: card.hovered ? Kit.colors.bgCardHover : Kit.colors.bgCard
        border.width: 1
        border.color: Kit.colors.strokeCard
        scale: card.down ? 0.98 : 1
        Behavior on scale { NumberAnimation { duration: Kit.durationFaster } }
        Behavior on color { ColorAnimation { duration: Kit.durationFaster } }
        FocusRing { visible: card.visualFocus; controlRadius: Kit.radiusOverlay }
    }

    contentItem: ColumnLayout {
        spacing: 8
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 112
            Layout.margins: 4
            Layout.bottomMargin: 0
            radius: Kit.radiusControl
            clip: true
            gradient: Gradient {
                GradientStop { position: 0; color: Kit.alpha(Kit.colors.accent, 0.22) }
                GradientStop { position: 1; color: Kit.alpha(Kit.colors.glow, 0.08) }
            }
            Image {
                id: picture
                anchors.fill: parent
                source: card.thumbnail
                sourceSize: Qt.size(640, 224)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: status === Image.Ready
            }
            Kirigami.Icon {
                visible: !picture.visible
                anchors.centerIn: parent
                width: 56
                height: 56
                source: card.iconName
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 8
            spacing: 8
            Kirigami.Icon {
                implicitWidth: 16
                implicitHeight: 16
                source: card.iconName
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: card.name
                    font: Kit.body
                    color: Kit.colors.textPrimary
                    elide: Text.ElideMiddle
                }
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: card.date
                    font: Kit.caption
                    color: Kit.colors.textSecondary
                    elide: Text.ElideRight
                }
            }
        }
    }
}
