/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import QtQuick.Window
import org.raye.files
import org.raye.files.kit
import org.raye.files.archive

// The body of "Add to archive" (Compress to › Additional options): name,
// folder, format, level and password, then "Advanced options" with the
// rest. Create / Cancel are the Raye dialog's.
Item {
    id: page

    property CompressOptionsModel model: CompressOptionsModel {}
    property bool advanced: false
    // what the advanced options get before they scroll, so the dialog
    // still fits on a 1280x800 screen
    readonly property int advancedRoom: Math.max(160, (Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 1080) - 530)

    signal browseRequested()
    signal accepted()

    implicitWidth: 672
    implicitHeight: column.implicitHeight

    function focusField() {
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    component Label: Text {
        font: Kit.body
        color: enabled ? Kit.colors.textPrimary : Kit.colors.textDisabled
        Layout.alignment: Qt.AlignVCenter
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 12

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 16
            rowSpacing: 12

            Label { text: i18nc("@label archive file name", "Name") }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ArchiveTextField {
                    id: nameField
                    Layout.fillWidth: true
                    text: page.model.name
                    Accessible.name: i18nc("@label archive file name", "Name")
                    onTextEdited: page.model.name = text
                    onAccepted: page.accepted()
                }
                Text {
                    text: page.model.extension
                    font: Kit.body
                    color: Kit.colors.textSecondary
                }
            }

            Label { text: i18nc("@label the folder an archive is saved in", "Save in") }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ArchiveTextField {
                    Layout.fillWidth: true
                    text: page.model.folder
                    Accessible.name: i18nc("@label the folder an archive is saved in", "Save in")
                    onTextEdited: page.model.folder = text
                    onAccepted: page.accepted()
                }
                CommandButton {
                    kind: "standard"
                    text: i18nc("@action:button", "Browse…")
                    focusPolicy: Qt.StrongFocus
                    onClicked: page.browseRequested()
                }
            }

            Label { text: i18nc("@label archive format", "Format") }
            ColumnLayout {
                spacing: 4
                ArchiveChoice {
                    accessibleName: i18nc("@label archive format", "Format")
                    // the third one names the compression picked under Advanced options
                    options: ["ZIP", "7z", page.model.isTar ? page.model.extension.substring(1) : "tar.zst"]
                    currentIndex: page.model.format
                    onActivated: (index) => page.model.format = index
                }
                Text {
                    Layout.fillWidth: true
                    text: i18nc("@info", "RAR archives open in Files but can't be made: the format is closed. 7z packs as tightly.")
                    font: Kit.caption
                    color: Kit.colors.textSecondary
                    wrapMode: Text.Wrap
                }
            }

            Label {
                text: i18nc("@label compression level", "Level")
                enabled: page.model.levelEnabled
            }
            ArchiveChoice {
                enabled: page.model.levelEnabled
                accessibleName: i18nc("@label compression level", "Level")
                options: [i18nc("@item compression level", "Store"), i18nc("@item compression level", "Fastest"),
                          i18nc("@item compression level", "Fast"), i18nc("@item compression level", "Normal"),
                          i18nc("@item compression level", "Maximum"), i18nc("@item compression level", "Ultra")]
                currentIndex: page.model.levelIndex
                onActivated: (index) => page.model.levelIndex = index
            }

            Label {
                text: i18nc("@label", "Password")
                enabled: page.model.canEncrypt
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                enabled: page.model.canEncrypt
                ArchiveTextField {
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: page.model.canEncrypt ? i18nc("@info:placeholder", "No password") : i18nc("@info:placeholder", "tar can't be protected")
                    text: page.model.password
                    Accessible.name: i18nc("@label", "Password")
                    onTextEdited: page.model.password = text
                    onAccepted: page.accepted()
                }
                ArchiveTextField {
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    visible: page.model.password.length > 0
                    placeholderText: i18nc("@info:placeholder", "Type it again")
                    text: page.model.passwordAgain
                    Accessible.name: i18nc("@label", "Confirm password")
                    onTextEdited: page.model.passwordAgain = text
                    onAccepted: page.accepted()
                }
            }

            Item { implicitWidth: 1; implicitHeight: 1; visible: encryptNames.visible }
            ArchiveCheckBox {
                id: encryptNames
                visible: page.model.canEncryptNames && page.model.password.length > 0
                text: i18nc("@option:check", "Hide file names too (AES-256)")
                checked: page.model.encryptNames
                onToggled: page.model.encryptNames = checked
            }
        }

        CommandButton {
            kind: "subtle"
            text: page.advanced ? i18nc("@action:button", "Fewer options ⌃") : i18nc("@action:button", "Advanced options ⌄")
            focusPolicy: Qt.StrongFocus
            onClicked: page.advanced = !page.advanced
        }

        Flickable {
            id: more
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(advancedGrid.implicitHeight, page.advancedRoom)
            visible: page.advanced
            clip: true
            contentHeight: advancedGrid.implicitHeight
            interactive: contentHeight > height
            boundsBehavior: Flickable.StopAtBounds

            T.ScrollBar.vertical: T.ScrollBar {
                id: scroller
                visible: more.contentHeight > more.height
                implicitWidth: 6
                padding: 1
                contentItem: Rectangle {
                    radius: 2
                    implicitWidth: 4
                    color: Kit.colors.textSecondary
                    opacity: scroller.active ? 0.8 : 0.4
                }
            }

            GridLayout {
                id: advancedGrid
                width: more.width - (scroller.visible ? 12 : 0)
                columns: 4
                columnSpacing: 12
                rowSpacing: 10

                Label { text: page.model.isTar ? i18nc("@label compression of a tar", "Compression") : i18nc("@label compression method", "Method") }
                ArchiveDropDown {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    accessibleName: i18nc("@label compression method", "Method")
                    model: page.model.methods
                    currentIndex: page.model.methodIndex
                    onActivated: (index) => page.model.methodIndex = index
                }
                Label { text: i18nc("@label", "Dictionary"); enabled: page.model.dictionaryEnabled }
                ArchiveDropDown {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    enabled: page.model.dictionaryEnabled
                    accessibleName: i18nc("@label", "Dictionary")
                    model: page.model.dictionaries
                    currentIndex: page.model.dictionaryIndex
                    onActivated: (index) => page.model.dictionaryIndex = index
                }

                Label { text: i18nc("@label compression word size", "Word size"); enabled: page.model.dictionaryEnabled }
                ArchiveDropDown {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    enabled: page.model.dictionaryEnabled
                    accessibleName: i18nc("@label compression word size", "Word size")
                    model: page.model.wordSizes
                    currentIndex: page.model.wordIndex
                    onActivated: (index) => page.model.wordIndex = index
                }
                Label { text: i18nc("@label", "Solid blocks"); enabled: page.model.format === 1 }
                ArchiveDropDown {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    enabled: page.model.format === 1
                    accessibleName: i18nc("@label", "Solid blocks")
                    model: page.model.solidBlocks
                    currentIndex: page.model.solidIndex
                    onActivated: (index) => page.model.solidIndex = index
                }

                Label { text: i18nc("@label processor threads", "Threads") }
                ArchiveDropDown {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    accessibleName: i18nc("@label processor threads", "Threads")
                    model: page.model.threadChoices
                    currentIndex: page.model.threadIndex
                    onActivated: (index) => page.model.threadIndex = index
                }
                Label { text: i18nc("@label", "Split into parts") }
                ArchiveDropDown {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    accessibleName: i18nc("@label", "Split into parts")
                    model: [i18nc("@item no split", "Don't split"), "100 MB", i18nc("@item part size", "700 MB (CD)"),
                            i18nc("@item part size", "4 GB (FAT32 drive)"), i18nc("@item part size", "Other size…")]
                    currentIndex: page.model.volumeIndex
                    onActivated: (index) => page.model.volumeIndex = index
                }

                Label { text: i18nc("@label what to do with an existing archive", "If it exists") }
                ArchiveDropDown {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    accessibleName: i18nc("@label what to do with an existing archive", "If it exists")
                    model: [i18nc("@item", "Make a new archive"), i18nc("@item", "Add and replace files"),
                            i18nc("@item", "Add newer files only"), i18nc("@item", "Synchronize")]
                    currentIndex: page.model.updateMode
                    onActivated: (index) => page.model.updateMode = index
                }
                Label {
                    text: page.model.volumeIndex === 4 ? i18nc("@label size of each part", "Part size") : i18nc("@label zip encryption", "Encryption")
                    enabled: page.model.volumeIndex === 4 || (page.model.format === 0 && page.model.password.length > 0)
                }
                Item {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: Kit.controlHeight
                    ArchiveTextField {
                        anchors.fill: parent
                        visible: page.model.volumeIndex === 4
                        placeholderText: i18nc("@info:placeholder an example part size", "250 MB")
                        text: page.model.customVolume
                        Accessible.name: i18nc("@label size of each part", "Part size")
                        onTextEdited: page.model.customVolume = text
                    }
                    ArchiveDropDown {
                        anchors.fill: parent
                        visible: page.model.volumeIndex !== 4
                        enabled: page.model.format === 0 && page.model.password.length > 0
                        accessibleName: i18nc("@label zip encryption", "Encryption")
                        model: ["AES-256", i18nc("@item zip encryption", "ZipCrypto (opens in Windows)")]
                        currentIndex: page.model.zipEncryption
                        onActivated: (index) => page.model.zipEncryption = index
                    }
                }

                Label { text: i18nc("@label file name patterns to leave out", "Leave out") }
                ArchiveTextField {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.columnSpan: 3
                    placeholderText: "*.tmp; node_modules/**; .git/**"
                    text: page.model.excludes
                    Accessible.name: i18nc("@label file name patterns to leave out", "Leave out")
                    onTextEdited: page.model.excludes = text
                }

                Item { implicitWidth: 1; implicitHeight: 1 }
                GridLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.columnSpan: 3
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 8
                    ArchiveCheckBox {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        text: i18nc("@option:check", "Delete the files after (to the Recycle Bin)")
                        checked: page.model.deleteAfter
                        onToggled: page.model.deleteAfter = checked
                    }
                    ArchiveCheckBox {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        text: i18nc("@option:check", "Keep symbolic links")
                        checked: page.model.keepSymlinks
                        onToggled: page.model.keepSymlinks = checked
                    }
                    ArchiveCheckBox {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        text: i18nc("@option:check", "Store last opened time")
                        checked: page.model.storeAccessed
                        onToggled: page.model.storeAccessed = checked
                    }
                    ArchiveCheckBox {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        text: i18nc("@option:check", "Store creation time")
                        checked: page.model.storeCreated
                        onToggled: page.model.storeCreated = checked
                    }
                }

                Label { text: i18nc("@label saved settings", "Profile") }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.columnSpan: 3
                    spacing: 8
                    ArchiveDropDown {
                        id: profileBox
                        Layout.preferredWidth: 180
                        accessibleName: i18nc("@label saved settings", "Profile")
                        model: [i18nc("@item no saved profile picked", "—")].concat(page.model.profiles)
                        onActivated: (index) => { if (index > 0) page.model.loadProfile(currentText) }
                    }
                    ArchiveTextField {
                        id: profileName
                        Layout.fillWidth: true
                        placeholderText: i18nc("@info:placeholder", "Name for these settings")
                        Accessible.name: i18nc("@label", "Profile name")
                    }
                    CommandButton {
                        kind: "standard"
                        text: i18nc("@action:button", "Save")
                        enabled: profileName.text.trim().length > 0
                        onClicked: { page.model.saveProfile(profileName.text); profileName.text = "" }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: page.model.estimate
            font: Kit.caption
            color: Kit.colors.textSecondary
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            visible: page.model.errorText.length > 0
            text: page.model.errorText
            font: Kit.caption
            color: Kit.colors.statusCritical
            wrapMode: Text.Wrap
            Accessible.role: Accessible.AlertMessage
        }
    }
}
