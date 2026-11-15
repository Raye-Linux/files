/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.raye.files
import org.raye.files.kit

// This PC (explorer spec 1.10, 3.10): "Devices and drives" with a usage
// bar per drive, then "Phones and cameras" and "Network locations" when
// there are any.
Rectangle {
    id: page

    property var model: null
    readonly property int sideMargin: 24
    readonly property int tileWidth: 272
    readonly property int tileSpacing: 8

    color: Kit.colors.bgLayer

    function focusFirst() {
        column.forceActiveFocus()
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight + 2 * page.sideMargin
        clip: true

        ColumnLayout {
            id: column
            x: page.sideMargin
            y: page.sideMargin - 8
            width: page.width - 2 * page.sideMargin
            spacing: 0

            Repeater {
                model: page.model ? page.model.sections : []
                ColumnLayout {
                    id: group
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.bottomMargin: 16
                    spacing: 0
                    HomeSectionHeader {
                        text: page.model.sectionTitle(group.modelData)
                        expandable: false
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: page.tileSpacing
                        Repeater {
                            model: page.model
                            DriveTile {
                                pageModel: page.model
                                visible: section === group.modelData
                                width: page.tileWidth
                                onActivated: connected ? page.model.open(url) : page.model.reconnect(url)
                                onPropertiesRequested: page.model.properties(url)
                            }
                        }
                    }
                }
            }

            Text {
                visible: page.model && page.model.count === 0
                textFormat: Text.PlainText
                text: i18nc("@info This PC with no drive", "No drives found.")
                font: Kit.body
                color: Kit.colors.textSecondary
            }
        }
    }
}
