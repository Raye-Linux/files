/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.files.kit

// The mouse wheel, as on Windows: one notch moves the view by the "lines
// per notch" setting times Kit.wheelLine, about 100 px, in a short smooth
// step. Qt Quick's own Flickable turns a notch into a flick instead, a
// short and uneven distance.
//
//   Flickable { id: flick; WheelScroll { target: flick } }
Kirigami.WheelHandler {
    verticalStepSize: Qt.styleHints.wheelScrollLines * Kit.wheelLine
    horizontalStepSize: Qt.styleHints.wheelScrollLines * Kit.wheelLine
    keyNavigationEnabled: true
}
