/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.raye.files.kit

// A group title of Folder options: Body strong, 12 px above it.
Text {
    textFormat: Text.PlainText
    Layout.fillWidth: true
    Layout.topMargin: 12
    Layout.bottomMargin: 2
    font: Kit.bodyStrong
    color: Kit.colors.textPrimary
    elide: Text.ElideRight
    Accessible.role: Accessible.Heading
}
