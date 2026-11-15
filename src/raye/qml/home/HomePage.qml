/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.raye.files
import org.raye.files.kit

// Home (explorer spec 3.9): the Continue strip (a Raye extra), "Quick
// access ⌄" with folder cards (pins first, then frequent folders; two rows
// then "Show more"), then the pivot Recent | Favorites over a Details list
// with Name, Date accessed and Location; row hover shows "Open file
// location" and the Favorites star. Empty lists show the Raye eye and one
// line. Everything follows the Folder options privacy choices.
Rectangle {
    id: page

    property var model: null
    property bool quickExpanded: true
    property bool quickShowAll: false
    property int pivot: 0

    color: Kit.colors.bgLayer

    function focusFirst() {
        if (quickRepeater.count > 0)
            quickRepeater.itemAt(0).forceActiveFocus()
        else
            pivotBar.forceActiveFocus()
    }

    readonly property int sideMargin: 24
    readonly property int cardWidth: 240
    readonly property int cardSpacing: 8
    readonly property int columnsFit: Math.max(1, Math.floor((width - 2 * sideMargin + cardSpacing) / (cardWidth + cardSpacing)))
    readonly property int quickCount: model ? model.quickAccess.count : 0
    readonly property bool quickOverflow: quickCount > columnsFit * 2
    readonly property var currentList: model ? (pivot === 0 || !model.showFavorites ? model.recent : model.favorites) : null

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 32
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: column
            x: page.sideMargin
            y: 16
            width: flick.width - 2 * page.sideMargin
            spacing: 0

            // Continue: the last three files, as big cards.
            HomeSectionHeader {
                visible: page.model && page.model.showContinue && page.model.continueItems.count > 0
                text: i18nc("@title:group files to go back to", "Continue")
                expandable: false
            }
            Flow {
                visible: page.model && page.model.showContinue && page.model.continueItems.count > 0
                Layout.fillWidth: true
                Layout.bottomMargin: 20
                spacing: 12
                Repeater {
                    model: page.model ? page.model.continueItems : null
                    delegate: HomeContinueCard {
                        width: Math.min(320, (column.width - 24) / 3)
                        onActivated: page.model.open(url)
                    }
                }
            }

            // Quick access
            HomeSectionHeader {
                id: quickHeader
                text: i18nc("@title:group", "Quick access")
                expanded: page.quickExpanded
                onFoldToggled: page.quickExpanded = !page.quickExpanded
            }
            Flow {
                id: quickFlow
                visible: page.quickExpanded
                Layout.fillWidth: true
                spacing: page.cardSpacing
                Repeater {
                    id: quickRepeater
                    model: page.model ? page.model.quickAccess : null
                    delegate: HomeCard {
                        pageModel: page.model
                        visible: page.quickShowAll || index < page.columnsFit * 2
                        width: page.cardWidth
                        onActivated: page.model.open(url)
                        onPinToggled: page.model.togglePin(url)
                    }
                }
            }
            CommandButton {
                visible: page.quickExpanded && page.quickOverflow
                Layout.topMargin: 4
                text: page.quickShowAll ? i18nc("@action:button", "Show less") : i18nc("@action:button", "Show more")
                icon.name: page.quickShowAll ? "go-up" : "go-down"
                onClicked: page.quickShowAll = !page.quickShowAll
            }
            Text {
                textFormat: Text.PlainText
                visible: page.quickExpanded && page.quickCount === 0
                Layout.fillWidth: true
                text: i18nc("@info", "Pin folders here to get to them quickly.")
                font: Kit.body
                color: Kit.colors.textSecondary
            }

            // Recent | Favorites
            PivotBar {
                id: pivotBar
                Layout.topMargin: 24
                Layout.leftMargin: -12
                visible: page.model && (page.model.showRecent || page.model.showFavorites)
                model: {
                    const tabs = []
                    if (page.model && page.model.showRecent) tabs.push(i18nc("@title:tab", "Recent"))
                    if (page.model && page.model.showFavorites) tabs.push(i18nc("@title:tab", "Favorites"))
                    return tabs
                }
                currentIndex: page.model && !page.model.showRecent ? 0 : page.pivot
                onActivated: index => page.pivot = index
            }

            // The column header of the Details list.
            Item {
                visible: listRepeater.count > 0 && pivotBar.visible
                Layout.fillWidth: true
                Layout.topMargin: 8
                implicitHeight: 32
                HomeColumns {
                    anchors.fill: parent
                    header: true
                    name: i18nc("@title:column", "Name")
                    date: page.pivot === 0 || !page.model.showFavorites ? i18nc("@title:column", "Date accessed") : i18nc("@title:column", "Date modified")
                    location: i18nc("@title:column", "Location")
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Kit.colors.strokeDivider
                }
            }
            Column {
                visible: pivotBar.visible
                Layout.fillWidth: true
                Repeater {
                    id: listRepeater
                    model: page.currentList
                    delegate: HomeFileRow {
                        pageModel: page.model
                        width: column.width
                        onActivated: page.model.open(url)
                        onLocationRequested: page.model.openLocation(url)
                        onFavoriteToggled: page.model.toggleFavorite(url)
                    }
                }
            }
            CommandButton {
                visible: pivotBar.visible && (page.pivot === 0 || !page.model.showFavorites) && page.model.canShowMoreRecent
                Layout.topMargin: 4
                text: i18nc("@action:button", "Show more")
                icon.name: "go-down"
                onClicked: page.model.showMoreRecent()
            }
            HomeEmptyState {
                visible: pivotBar.visible && listRepeater.count === 0
                Layout.fillWidth: true
                Layout.topMargin: 24
                text: page.pivot === 0 && page.model && page.model.showRecent
                      ? i18nc("@info", "Files you open will show here.")
                      : i18nc("@info", "Files you add to Favorites will show here.")
            }
        }
    }
}
