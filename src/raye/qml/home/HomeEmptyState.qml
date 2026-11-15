/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.raye.files.kit

// An empty list of Home: the Raye eye, faint, and one line under it.
ColumnLayout {
    id: empty

    property string text: ""

    spacing: 12

    Kirigami.Icon {
        Layout.alignment: Qt.AlignHCenter
        implicitWidth: 96
        implicitHeight: 96
        source: "raye-eye"
        fallback: "folder-open"
        opacity: 0.55
    }
    Text {
        textFormat: Text.PlainText
        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: empty.text
        font: Kit.body
        color: Kit.colors.textSecondary
        wrapMode: Text.Wrap
    }
}
