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

// The body of "Replace or skip files" (explorer spec 3.13): the operation,
// "The destination already has a file named "a.txt"", then three large
// choices; the third opens the compare view, two columns with a check box
// each (both ticked keeps both files). For a folder onto a folder it asks
// to merge them. The card around it carries the title and the buttons.
Item {
    id: page

    // "file", "merge" or "same" (a file copied onto itself)
    property string mode: "file"
    property string headline: ""
    property string fileName: ""
    property string sourceFolder: ""
    property string destinationFolder: ""
    property bool canReplace: true
    property bool canSkip: true
    property bool canKeepBoth: true
    property bool multiple: false
    // the two sides: name, icon, thumbnail (url or empty), size and date
    // texts, and whether that side is the newer / larger one
    property var source: ({})
    property var destination: ({})

    property bool comparing: false
    readonly property alias applyToAll: forAll.checked
    readonly property bool keepSource: sourceBoxItem ? sourceBoxItem.checked : true
    readonly property bool keepDestination: destinationBoxItem ? destinationBoxItem.checked : true
    // the two check boxes of the compare view
    property Item sourceBoxItem: null
    property Item destinationBoxItem: null

    // 0 replace (or merge), 1 skip, 2 keep both
    signal chosen(int choice, bool applyToAll)

    implicitWidth: 472
    implicitHeight: column.implicitHeight

    function focusField() {
        if (choices.count > 0)
            choices.itemAt(0).forceActiveFocus()
    }

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 12

        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            visible: page.headline.length > 0
            text: page.headline
            font: Kit.body
            color: Kit.colors.textSecondary
            elide: Text.ElideMiddle
        }

        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: page.mode === "merge"
                  ? i18nc("@info", "The destination already has a folder named \"%1\".", page.fileName)
                  : page.mode === "same"
                    ? i18nc("@info", "The source and destination file names are the same.")
                    : i18nc("@info", "The destination already has a file named \"%1\".", page.fileName)
            font: Kit.bodyStrong
            color: Kit.colors.textPrimary
            wrapMode: Text.Wrap
        }

        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            visible: page.mode === "merge"
            text: i18nc("@info", "If files in it have the same names, you'll be asked about each of them. Merge the folders?")
            font: Kit.body
            color: Kit.colors.textSecondary
            wrapMode: Text.Wrap
        }

        ColumnLayout {
            visible: !page.comparing
            Layout.fillWidth: true
            spacing: 4
            Repeater {
                id: choices
                model: {
                    const list = []
                    if (page.mode === "merge") {
                        list.push({ id: 0, icon: "folder-open", title: i18nc("@action:button", "Merge the folders"), line: i18nc("@info", "Files go into the folder that is already there") })
                        if (page.canSkip)
                            list.push({ id: 1, icon: "media-skip-forward", title: i18nc("@action:button", "Skip this folder"), line: i18nc("@info", "Leave both folders as they are") })
                        return list
                    }
                    if (page.canReplace)
                        list.push({ id: 0, icon: "document-replace", title: i18nc("@action:button", "Replace the file in the destination"), line: page.destinationFolder })
                    if (page.canSkip)
                        list.push({ id: 1, icon: "media-skip-forward", title: i18nc("@action:button", "Skip this file"), line: i18nc("@info", "Leave the file in the destination as it is") })
                    if (page.canKeepBoth && page.mode !== "same")
                        list.push({ id: 3, icon: "view-split-left-right", title: i18nc("@action:button", "Compare info for both files"), line: i18nc("@info", "Choose which files to keep") })
                    return list
                }
                delegate: ChoiceRow {
                    required property var modelData
                    Layout.fillWidth: true
                    iconName: modelData.icon
                    title: modelData.title
                    line: modelData.line
                    onClicked: {
                        if (modelData.id === 3)
                            page.comparing = true
                        else
                            page.chosen(modelData.id, forAll.checked)
                    }
                }
            }
        }

        // The compare view.
        GridLayout {
            visible: page.comparing
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 16
            rowSpacing: 8
            Repeater {
                model: [
                    { side: page.source, header: page.sourceFolder.length ? i18nc("@title:column", "Files from %1", page.sourceFolder) : i18nc("@title:column", "Files to copy") },
                    { side: page.destination, header: page.destinationFolder.length ? i18nc("@title:column", "Files already in %1", page.destinationFolder) : i18nc("@title:column", "Files already there") }
                ]
                delegate: ColumnLayout {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    spacing: 8
                    OptionCheckBox {
                        id: box
                        Layout.fillWidth: true
                        text: modelData.header
                        checked: true
                        Component.onCompleted: {
                            if (index === 0) page.sourceBoxItem = box
                            else page.destinationBoxItem = box
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 96
                        radius: Kit.radiusControl
                        color: Kit.colors.bgCard
                        border.width: 1
                        border.color: Kit.colors.strokeCard
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 12
                            Item {
                                implicitWidth: 64
                                implicitHeight: 64
                                Kirigami.Icon {
                                    anchors.fill: parent
                                    visible: thumb.status !== Image.Ready
                                    source: modelData.side.icon || "unknown"
                                }
                                Image {
                                    id: thumb
                                    anchors.fill: parent
                                    source: modelData.side.thumbnail || ""
                                    sourceSize: Qt.size(128, 128)
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
                                    text: modelData.side.date || ""
                                    font: modelData.side.newer ? Kit.bodyStrong : Kit.body
                                    color: Kit.colors.textPrimary
                                    elide: Text.ElideRight
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                    text: modelData.side.newer ? i18nc("@info", "Newer") : ""
                                    visible: text.length > 0
                                    font: Kit.caption
                                    color: Kit.colors.accentText
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                    text: modelData.side.size || ""
                                    font: modelData.side.larger ? Kit.bodyStrong : Kit.body
                                    color: Kit.colors.textSecondary
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }

        OptionCheckBox {
            id: forAll
            visible: page.multiple && page.mode !== "same"
            Layout.fillWidth: true
            text: i18nc("@option:check", "Do this for all current items")
        }
    }
}
