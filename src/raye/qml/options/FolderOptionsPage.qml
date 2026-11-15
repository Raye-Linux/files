/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.raye.files
import org.raye.files.kit

// The body of Folder options (explorer spec 3.15): a pivot General | View |
// Search over the choices of that tab. Every choice changes the options
// object in memory; OK and Apply (the card's buttons) write them.
Item {
    id: page

    property var options: null
    property int tab: 0

    implicitWidth: 500
    // the tallest tab, so the card does not jump between tabs
    implicitHeight: pivot.height + 16 + Math.max(generalTab.implicitHeight, viewTab.implicitHeight, searchTab.implicitHeight)

    function focusField() {
        pivot.forceActiveFocus()
    }

    PivotBar {
        id: pivot
        anchors.left: parent.left
        anchors.leftMargin: -12
        model: [i18nc("@title:tab", "General"), i18nc("@title:tab", "View"), i18nc("@title:tab", "Search")]
        currentIndex: page.tab
        onActivated: index => page.tab = index
    }

    Item {
        anchors.top: pivot.bottom
        anchors.topMargin: 16
        anchors.left: parent.left
        anchors.right: parent.right

        // General
        ColumnLayout {
            id: generalTab
            visible: page.tab === 0
            width: parent.width
            spacing: 4

            SectionTitle { text: i18nc("@title:group", "Open Files to") }
            OptionRadio {
                text: i18nc("@option:radio", "Home")
                checked: page.options && page.options.startPage === "home"
                onClicked: page.options.startPage = "home"
            }
            OptionRadio {
                text: i18nc("@option:radio", "This PC")
                checked: page.options && page.options.startPage === "thispc"
                onClicked: page.options.startPage = "thispc"
            }

            SectionTitle { text: i18nc("@title:group", "Browse folders") }
            OptionRadio {
                text: i18nc("@option:radio", "Open each folder in the same window")
                checked: page.options && !page.options.openInOwnWindow
                onClicked: page.options.openInOwnWindow = false
            }
            OptionRadio {
                text: i18nc("@option:radio", "Open each folder in its own window")
                checked: page.options && page.options.openInOwnWindow
                onClicked: page.options.openInOwnWindow = true
            }

            SectionTitle { text: i18nc("@title:group", "Click items as follows") }
            OptionRadio {
                text: i18nc("@option:radio", "Single-click to open an item (point to select)")
                checked: page.options && page.options.singleClick
                onClicked: page.options.singleClick = true
            }
            OptionRadio {
                text: i18nc("@option:radio", "Double-click to open an item (single-click to select)")
                checked: page.options && !page.options.singleClick
                onClicked: page.options.singleClick = false
            }

            SectionTitle { text: i18nc("@title:group", "Privacy") }
            OptionCheckBox {
                Layout.fillWidth: true
                text: i18nc("@option:check", "Show recently used files")
                checked: page.options && page.options.showRecent
                onToggled: page.options.showRecent = checked
            }
            OptionCheckBox {
                Layout.fillWidth: true
                text: i18nc("@option:check", "Show frequently used folders")
                checked: page.options && page.options.showFrequent
                onToggled: page.options.showFrequent = checked
            }
            OptionCheckBox {
                Layout.fillWidth: true
                text: i18nc("@option:check", "Show files from Favorites")
                checked: page.options && page.options.showFavorites
                onToggled: page.options.showFavorites = checked
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 8
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: i18nc("@label", "Clear Files history")
                    font: Kit.body
                    color: Kit.colors.textPrimary
                }
                CommandButton {
                    id: clearButton
                    kind: "standard"
                    text: i18nc("@action:button", "Clear")
                    onClicked: {
                        page.options.clearHistory()
                        text = i18nc("@info the history is gone", "Cleared")
                        enabled = false
                    }
                }
            }

            CommandButton {
                Layout.alignment: Qt.AlignRight
                Layout.topMargin: 12
                kind: "standard"
                text: i18nc("@action:button", "Restore defaults")
                onClicked: page.options.restoreDefaults(0)
            }
        }

        // View
        ColumnLayout {
            id: viewTab
            visible: page.tab === 1
            width: parent.width
            spacing: 4

            SectionTitle { text: i18nc("@title:group", "Folder views") }
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: i18nc("@info", "Folders you changed keep their own view. Reset folders to give them all the default view again.")
                font: Kit.body
                color: Kit.colors.textSecondary
                wrapMode: Text.Wrap
            }
            CommandButton {
                id: resetButton
                Layout.topMargin: 4
                kind: "standard"
                text: i18nc("@action:button", "Reset folders")
                onClicked: {
                    if (page.options.resetFolders()) {
                        text = i18nc("@info the folders were reset", "Folders reset")
                        enabled = false
                    }
                }
            }

            SectionTitle { text: i18nc("@title:group", "Advanced settings") }
            OptionCheckBox {
                Layout.fillWidth: true
                text: i18nc("@option:check", "Always show icons, never thumbnails")
                checked: page.options && page.options.alwaysShowIcons
                onToggled: page.options.alwaysShowIcons = checked
            }
            OptionCheckBox {
                Layout.fillWidth: true
                text: i18nc("@option:check", "Show hidden files and folders")
                checked: page.options && page.options.showHidden
                onToggled: page.options.showHidden = checked
            }
            OptionCheckBox {
                Layout.fillWidth: true
                text: i18nc("@option:check", "Hide extensions for known file types")
                checked: page.options && page.options.hideExtensions
                onToggled: page.options.hideExtensions = checked
            }
            OptionCheckBox {
                Layout.fillWidth: true
                text: i18nc("@option:check", "Show status bar")
                checked: page.options && page.options.showStatusBar
                onToggled: page.options.showStatusBar = checked
            }
            OptionCheckBox {
                Layout.fillWidth: true
                text: i18nc("@option:check", "Decrease space between items (compact view)")
                checked: page.options && page.options.compact
                onToggled: page.options.compact = checked
            }
            OptionCheckBox {
                Layout.fillWidth: true
                text: i18nc("@option:check", "Use check boxes to select items")
                checked: page.options && page.options.itemCheckBoxes
                onToggled: page.options.itemCheckBoxes = checked
            }
            SectionTitle { text: i18nc("@title:group", "Navigation pane") }
            OptionCheckBox {
                Layout.fillWidth: true
                text: i18nc("@option:check", "Show all folders")
                checked: page.options && page.options.showAllFolders
                onToggled: page.options.showAllFolders = checked
            }
            OptionCheckBox {
                Layout.fillWidth: true
                text: i18nc("@option:check", "Expand to open folder")
                checked: page.options && page.options.expandToOpenFolder
                onToggled: page.options.expandToOpenFolder = checked
            }

            CommandButton {
                Layout.alignment: Qt.AlignRight
                Layout.topMargin: 12
                kind: "standard"
                text: i18nc("@action:button", "Restore defaults")
                onClicked: page.options.restoreDefaults(1)
            }
        }

        // Search
        ColumnLayout {
            id: searchTab
            visible: page.tab === 2
            width: parent.width
            spacing: 4

            SectionTitle { text: i18nc("@title:group", "When searching") }
            OptionCheckBox {
                Layout.fillWidth: true
                text: i18nc("@option:check", "Search file contents as well as names")
                checked: page.options && page.options.searchContents
                onToggled: page.options.searchContents = checked
            }
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                Layout.leftMargin: 28
                text: i18nc("@info", "Searches take longer to get ready when file contents are included.")
                font: Kit.caption
                color: Kit.colors.textSecondary
                wrapMode: Text.Wrap
            }
            CommandButton {
                Layout.topMargin: 12
                kind: "standard"
                text: i18nc("@action:button", "Search settings")
                icon.name: "settings-configure"
                onClicked: page.options.openSearchSettings()
            }
            CommandButton {
                Layout.alignment: Qt.AlignRight
                Layout.topMargin: 12
                kind: "standard"
                text: i18nc("@action:button", "Restore defaults")
                onClicked: page.options.restoreDefaults(2)
            }
        }
    }
}
