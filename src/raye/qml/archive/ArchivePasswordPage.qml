/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.raye.files
import org.raye.files.kit

// The body of the password prompt of a protected archive (explorer spec
// 3.12): the sentence, the archive's name and the password box. OK and
// Cancel are the Raye dialog's.
Item {
    id: page

    property string archiveName: ""
    // the last password did not open it
    property bool wrongPassword: false
    readonly property alias password: field.text

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
            text: i18nc("@info", "This archive is protected. Enter the password.")
            font: Kit.body
            color: Kit.colors.textPrimary
            wrapMode: Text.Wrap
        }
        Text {
            Layout.fillWidth: true
            visible: page.archiveName.length > 0
            text: page.archiveName
            font: Kit.caption
            color: Kit.colors.textSecondary
            elide: Text.ElideMiddle
        }
        ArchiveTextField {
            id: field
            Layout.topMargin: 4
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: i18nc("@info:placeholder", "Password")
            onAccepted: page.accepted()
        }
        Text {
            Layout.fillWidth: true
            visible: page.wrongPassword
            text: i18nc("@info", "The password is incorrect. Try again.")
            font: Kit.caption
            color: Kit.colors.statusCritical
            wrapMode: Text.Wrap
            Accessible.role: Accessible.AlertMessage
        }
    }
}
