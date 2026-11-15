/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.raye.files
import org.raye.files.kit

// The body of a copy error asked outside the progress window: the
// message, and "Do this for all current items" when there are more.
// Try again / Skip / Cancel are the card's buttons.
Item {
    id: page

    property string message: ""
    property bool multiple: false
    readonly property alias applyToAll: forAll.checked

    implicitWidth: 472
    implicitHeight: column.implicitHeight

    function focusField() {
    }

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 12
        Text {
            Layout.fillWidth: true
            text: page.message
            textFormat: Text.PlainText
            font: Kit.body
            color: Kit.colors.textPrimary
            wrapMode: Text.Wrap
            Accessible.role: Accessible.AlertMessage
        }
        OptionCheckBox {
            id: forAll
            visible: page.multiple
            Layout.fillWidth: true
            text: i18nc("@option:check", "Do this for all current items")
        }
    }
}
