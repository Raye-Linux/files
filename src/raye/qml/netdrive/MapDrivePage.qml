/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.raye.files
import org.raye.files.kit

// Map network drive: the shared folder, a name, the folder it goes in,
// Reconnect at sign-in and, when the share wants them, a user name and
// password (kept in the wallet, never in a file).
Item {
    id: page

    property var model: null
    readonly property int labelWidth: 140

    implicitWidth: 480
    implicitHeight: column.implicitHeight

    function focusField() {
        folderField.forceActiveFocus()
    }

    component Label: Text {
        Layout.preferredWidth: page.labelWidth
        Layout.alignment: Qt.AlignVCenter
        textFormat: Text.PlainText
        font: Kit.body
        color: Kit.colors.textSecondary
        elide: Text.ElideRight
    }

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 12

        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            font: Kit.body
            color: Kit.colors.textPrimary
            text: i18nc("@info", "Pick the shared folder and where it shows up. You'll find it in This PC and in the navigation pane.")
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            Label { text: i18nc("@label shared folder on another computer", "Folder:") }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                ArchiveTextField {
                    id: folderField
                    Layout.fillWidth: true
                    placeholderText: i18nc("@info:placeholder a shared folder", "\\\\server\\share")
                    text: page.model ? page.model.folder : ""
                    onTextEdited: page.model.folder = text
                    onAccepted: page.model.finish()
                }
                Text {
                    textFormat: Text.PlainText
                    font: Kit.caption
                    color: Kit.colors.textTertiary
                    text: i18nc("@info example of a shared folder", "Example: \\\\server\\share")
                }
            }

            Label { text: i18nc("@label name of the drive", "Name:") }
            ArchiveTextField {
                Layout.fillWidth: true
                text: page.model ? page.model.name : ""
                onTextEdited: page.model.name = text
            }

            Label { text: i18nc("@label where the drive shows up", "Location:") }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ArchiveTextField {
                    Layout.fillWidth: true
                    text: page.model ? page.model.location : ""
                    onTextEdited: page.model.location = text
                }
                CommandButton {
                    kind: "standard"
                    text: i18nc("@action:button pick another folder", "Browse…")
                    onClicked: page.model.chooseLocation()
                }
            }
        }

        ArchiveCheckBox {
            text: i18nc("@option:check", "Reconnect at sign-in")
            checked: page.model ? page.model.reconnect : true
            onToggled: page.model.reconnect = checked
        }
        ArchiveCheckBox {
            text: i18nc("@option:check", "Connect using different credentials")
            checked: page.model ? page.model.otherCredentials : false
            onToggled: page.model.otherCredentials = checked
        }

        GridLayout {
            Layout.fillWidth: true
            visible: page.model && page.model.otherCredentials
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            Label { text: i18nc("@label", "User name:") }
            ArchiveTextField {
                Layout.fillWidth: true
                placeholderText: i18nc("@info:placeholder", "name or DOMAIN\\name")
                text: page.model ? page.model.user : ""
                onTextEdited: page.model.user = text
            }
            Label { text: i18nc("@label", "Password:") }
            ArchiveTextField {
                Layout.fillWidth: true
                echoMode: TextInput.Password
                text: page.model ? page.model.password : ""
                onTextEdited: page.model.password = text
                onAccepted: page.model.finish()
            }
            Item { implicitWidth: 1 }
            ArchiveCheckBox {
                text: i18nc("@option:check keep the password in the wallet", "Remember my credentials")
                checked: page.model ? page.model.remember : true
                onToggled: page.model.remember = checked
            }
        }

        Text {
            Layout.fillWidth: true
            visible: text.length > 0
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            font: Kit.body
            color: Kit.colors.statusCritical
            Accessible.role: Accessible.AlertMessage
            text: page.model ? page.model.error : ""
        }
    }
}
