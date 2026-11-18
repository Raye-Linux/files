/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.raye.files
import org.raye.files.kit

// Gallery (explorer spec 1.9, 3.9): the pictures and videos of the gallery
// folders, newest first, a header per month, rows as wide as the page
// (about 180 tall, 4 px apart). More come in as the page scrolls.
Rectangle {
    id: page

    property var model: null
    readonly property int sideMargin: 24
    readonly property int rowHeight: 180
    readonly property int gap: 4

    color: Kit.colors.bgLayer

    function focusFirst() {
        flick.forceActiveFocus()
    }

    Flickable {
        id: flick
        WheelScroll { target: flick }
        anchors.fill: parent
        contentHeight: column.implicitHeight + 2 * page.sideMargin
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        // The next batch a screen before the end.
        onContentYChanged: {
            if (page.model && page.model.hasMore && contentY + height > contentHeight - 2 * height)
                page.model.loadMore()
        }

        ColumnLayout {
            id: column
            x: page.sideMargin
            y: page.sideMargin - 8
            width: page.width - 2 * page.sideMargin
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                visible: page.model && page.model.count > 0
                Item { Layout.fillWidth: true }
                CommandButton {
                    id: collectionButton
                    text: i18nc("@action:button folders shown in Gallery", "Collection")
                    icon.name: "folder-pictures"
                    showChevron: true
                    onClicked: collectionMenu.open()

                    Flyout {
                        id: collectionMenu
                        y: collectionButton.height + 4
                        x: collectionButton.width - width
                        contentItem: ColumnLayout {
                            spacing: 0
                            Repeater {
                                model: page.model ? page.model.collection : []
                                OptionCheckBox {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 8
                                    Layout.rightMargin: 12
                                    text: modelData.name
                                    checked: modelData.included
                                    onToggled: page.model.setFolderIncluded(modelData.path, checked)
                                }
                            }
                            Separator { vertical: false; Layout.fillWidth: true; Layout.topMargin: 4; Layout.bottomMargin: 4 }
                            CommandButton {
                                Layout.fillWidth: true
                                alignment: Qt.AlignLeft
                                text: i18nc("@action:inmenu add a folder to Gallery", "Add a folder…")
                                icon.name: "folder-add"
                                onClicked: { collectionMenu.close(); page.model.addFolder() }
                            }
                        }
                    }
                }
            }

            Repeater {
                model: page.model ? page.model.groups : []
                ColumnLayout {
                    id: month
                    required property var modelData
                    readonly property var aspects: modelData.items.map(i => i.aspect)
                    Layout.fillWidth: true
                    spacing: page.gap

                    HomeSectionHeader {
                        text: month.modelData.title
                        expandable: false
                    }
                    Repeater {
                        model: page.model ? page.model.rows(month.aspects, column.width, page.rowHeight, page.gap) : []
                        Row {
                            id: line
                            required property var modelData
                            spacing: page.gap
                            Repeater {
                                model: month.modelData.items.slice(line.modelData.start, line.modelData.start + line.modelData.count)
                                GalleryTile {
                                    pageModel: page.model
                                    height: line.modelData.height
                                    width: Math.floor(line.modelData.height * modelData.aspect)
                                    onActivated: page.model.open(url)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        visible: page.model && !page.model.loading && page.model.count === 0
        spacing: 12
        HomeEmptyState {
            Layout.alignment: Qt.AlignHCenter
            text: i18nc("@info empty Gallery", "Pictures and videos you save in Pictures will show here.")
        }
    }
}
