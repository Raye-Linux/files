/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.raye.files
import org.raye.files.kit

// The page of the share flyout (explorer spec 3.11), drawn inside the Raye
// flyout (src/raye/share/shareflyout.cpp): the files at the top, the "Share
// with" row of 64 x 64 targets, then "More options" with every target.
// shareModel: Raye::Share::ShareModel. purposeModel: Purpose's list of
// targets (PurposeAlternativesModel), which runs them.
Item {
    id: sheet

    required property QtObject shareModel
    property QtObject purposeModel: null
    // "main", "more" or "job"
    property string page: "main"
    property string jobError: ""

    signal closeRequested()

    implicitWidth: 412
    implicitHeight: (page === "main" ? mainPage.implicitHeight : page === "more" ? morePage.implicitHeight : jobPage.implicitHeight) + 24

    function activate(target) {
        if (target.row < 0) {
            sheet.shareModel.activateOwn(target.id)
            return
        }
        if (!sheet.purposeModel) {
            return
        }
        sheet.jobError = ""
        jobLoader.setSource(Qt.resolvedUrl("ShareJob.qml"), {model: sheet.purposeModel, row: target.row})
        sheet.page = "job"
    }

    // ---- the files and the row of targets
    Column {
        id: mainPage
        visible: sheet.page === "main"
        x: 12
        y: 12
        width: sheet.width - 24
        spacing: 0

        Row {
            width: parent.width
            height: 48
            spacing: 12
            Kirigami.Icon {
                anchors.verticalCenter: parent.verticalCenter
                width: 40
                height: 40
                source: sheet.shareModel.iconName
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 52
                Text {
                    width: parent.width
                    text: sheet.shareModel.title
                    font: Kit.bodyStrong
                    color: Kit.colors.textPrimary
                    elide: Text.ElideMiddle
                }
                Text {
                    width: parent.width
                    visible: text.length > 0
                    text: sheet.shareModel.subtitle
                    font: Kit.caption
                    color: Kit.colors.textSecondary
                    elide: Text.ElideRight
                }
            }
        }

        Item { width: 1; height: 12 }

        Text {
            text: i18nc("@title the targets of the share flyout", "Share with")
            font: Kit.bodyStrong
            color: Kit.colors.textPrimary
        }

        Item { width: 1; height: 8 }

        Flow {
            width: parent.width
            spacing: 4
            Repeater {
                model: sheet.shareModel.rowTargets
                delegate: Item {
                    id: tile
                    required property var modelData
                    width: 76
                    height: 92
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.name
                    Keys.onReturnPressed: sheet.activate(modelData)
                    Keys.onEnterPressed: sheet.activate(modelData)
                    Keys.onSpacePressed: sheet.activate(modelData)

                    Rectangle {
                        id: plate
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 64
                        height: 64
                        radius: Kit.radiusOverlay
                        color: tap.pressed ? Kit.colors.fillSubtlePressed
                             : hover.hovered ? Kit.colors.fillSubtleHover : Kit.colors.fillControl
                        border.width: 1
                        border.color: Kit.colors.strokeControl
                        Behavior on color { ColorAnimation { duration: Kit.durationFaster } }
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: 32
                            height: 32
                            source: tile.modelData.icon
                            isMask: tile.modelData.glyph
                            color: Kit.colors.textPrimary
                        }
                        FocusRing {
                            visible: tile.activeFocus
                            controlRadius: Kit.radiusOverlay
                        }
                    }
                    Text {
                        anchors.top: plate.bottom
                        anchors.topMargin: 4
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: tile.modelData.name
                        font: Kit.caption
                        color: Kit.colors.textPrimary
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    HoverHandler { id: hover }
                    TapHandler {
                        id: tap
                        onTapped: sheet.activate(tile.modelData)
                    }
                }
            }
        }

        Item { width: 1; height: 8 }
        Rectangle { width: parent.width; height: 1; color: Kit.colors.strokeDivider }
        Item { width: 1; height: 4 }

        CommandButton {
            width: parent.width
            visible: sheet.shareModel.moreTargets.length > 0
            alignment: Qt.AlignLeft
            text: i18nc("@action:button every share target, in a list", "More options")
            icon.name: "overflow-menu"
            onClicked: sheet.page = "more"
        }
    }

    // ---- every target, in a list
    Column {
        id: morePage
        visible: sheet.page === "more"
        x: 12
        y: 12
        width: sheet.width - 24
        spacing: 4

        Row {
            spacing: 8
            CommandButton {
                icon.name: "go-previous"
                display: T.AbstractButton.IconOnly
                text: i18nc("@action:button back to the share row", "Back")
                onClicked: sheet.page = "main"
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: i18nc("@title every share target", "More options")
                font: Kit.bodyStrong
                color: Kit.colors.textPrimary
            }
        }

        ListView {
            width: parent.width
            height: Math.min(contentHeight, 10 * Kit.controlHeight)
            clip: true
            interactive: contentHeight > height
            model: sheet.shareModel.moreTargets
            delegate: CommandButton {
                required property var modelData
                width: ListView.view.width
                alignment: Qt.AlignLeft
                tintIcon: false
                text: modelData.name
                icon.name: modelData.icon
                onClicked: sheet.activate(modelData)
            }
        }
    }

    // ---- a target that asks something first (a device, an account)
    Column {
        id: jobPage
        visible: sheet.page === "job"
        x: 12
        y: 12
        width: sheet.width - 24
        spacing: 8

        Loader {
            id: jobLoader
            width: parent.width
            height: item ? Math.max(item.implicitHeight, 64) : 64
            active: sheet.page === "job"
            onActiveChanged: if (!active) source = ""
        }
        Connections {
            target: jobLoader.item
            ignoreUnknownSignals: true
            function onFinished(ok, message) {
                if (ok) {
                    sheet.closeRequested()
                } else if (message.length > 0) {
                    sheet.jobError = message
                } else {
                    sheet.page = "main"
                }
            }
        }
        Text {
            width: parent.width
            visible: sheet.jobError.length > 0
            text: i18nc("@info %1 is the reason", "Couldn't share. %1", sheet.jobError)
            font: Kit.body
            color: Kit.colors.statusCritical
            wrapMode: Text.Wrap
        }
        CommandButton {
            visible: sheet.jobError.length > 0
            kind: "standard"
            text: i18nc("@action:button", "Back")
            onClicked: sheet.page = "main"
        }
    }
}
