/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.raye.files
import org.raye.files.kit
import org.raye.files.archive

// The body of "Extract compressed (zipped) folders" (explorer spec 1.12,
// 3.12): the folder the files go to, Browse…, and "Show extracted files
// when complete". The card, its title and the Extract / Cancel buttons are
// the Raye dialog around it (archivewizards.cpp).
Item {
    id: page

    property ExtractAllModel model: ExtractAllModel {}

    // Browse… was pressed: the dialog opens the folder picker.
    signal browseRequested()
    // Enter in the folder box.
    signal accepted()

    implicitWidth: 472
    implicitHeight: column.implicitHeight

    function focusField() {
        field.forceActiveFocus()
        field.selectAll()
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: i18nc("@label", "Files will be extracted to this folder:")
            font: Kit.body
            color: Kit.colors.textPrimary
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ArchiveTextField {
                id: field
                Layout.fillWidth: true
                text: page.model.destination
                placeholderText: i18nc("@info:placeholder", "Folder")
                Accessible.name: i18nc("@label", "Files will be extracted to this folder:")
                onTextEdited: page.model.destination = text
                onAccepted: page.accepted()
            }
            CommandButton {
                kind: "standard"
                text: i18nc("@action:button", "Browse…")
                focusPolicy: Qt.StrongFocus
                onClicked: page.browseRequested()
            }
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

        ArchiveCheckBox {
            Layout.topMargin: 8
            Layout.fillWidth: true
            text: i18nc("@option:check", "Show extracted files when complete")
            checked: page.model.showWhenDone
            onToggled: page.model.showWhenDone = checked
        }
    }
}
