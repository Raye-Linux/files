/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.raye.files
import org.raye.files.kit

// The body of the progress window (explorer spec 3.13): one card per
// operation, stacked with dividers. Each card: the headline, "37%
// complete", pause and cancel, the speed graph, Name / Time remaining /
// Items remaining, and errors inline with Try again / Skip / Cancel.
// "Fewer details / More details" hides the graph and the rows.
Rectangle {
    id: page

    property var model: null
    property bool details: true
    signal detailsToggled(bool details)

    implicitWidth: 480
    implicitHeight: column.implicitHeight
    color: Kit.colors.bgBase

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 0

        Repeater {
            model: page.model
            delegate: ColumnLayout {
                id: card
                required property int index
                required property string headline
                required property int percent
                required property string percentText
                required property bool paused
                required property bool canPause
                required property string speedText
                required property real speed
                required property var history
                required property real peak
                required property string name
                required property string remaining
                required property string items
                required property string error
                required property bool errorCanRetry
                required property bool errorMultiple
                required property bool done
                required property bool finishing

                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 16
                Layout.bottomMargin: 16
                spacing: 8

                Rectangle {
                    visible: card.index > 0
                    Layout.fillWidth: true
                    Layout.leftMargin: -20
                    Layout.rightMargin: -20
                    Layout.topMargin: -16
                    Layout.bottomMargin: 8
                    implicitHeight: 1
                    color: Kit.colors.strokeDivider
                }

                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: card.headline
                    font: Kit.body
                    color: Kit.colors.textPrimary
                    elide: Text.ElideMiddle
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: card.percentText
                        font: Kit.subtitle
                        color: Kit.colors.textPrimary
                        elide: Text.ElideRight
                    }
                    // the soft check before the window closes
                    Text {
                        textFormat: Text.PlainText
                        visible: card.done
                        text: "✓"
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        color: Kit.colors.statusSuccess
                        opacity: card.done ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: Kit.durationNormal } }
                    }
                    CommandButton {
                        visible: card.canPause && !card.done && !card.finishing
                        icon.name: card.paused ? "media-playback-start" : "media-playback-pause"
                        text: ""
                        Accessible.name: card.paused ? i18nc("@action:button", "Resume") : i18nc("@action:button", "Pause")
                        onClicked: page.model.pause(card.index)
                    }
                    CommandButton {
                        visible: !card.done && !card.finishing
                        icon.name: "dialog-cancel"
                        text: ""
                        Accessible.name: i18nc("@action:button", "Cancel")
                        onClicked: page.model.cancel(card.index)
                    }
                }

                // the bar under the headline
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 4
                    radius: 2
                    color: Kit.colors.fillControl
                    Rectangle {
                        width: parent.width * Math.max(0, Math.min(100, card.percent)) / 100
                        height: parent.height
                        radius: 2
                        color: card.paused ? Kit.colors.statusCaution : Kit.colors.accent
                        Behavior on width { NumberAnimation { duration: Kit.durationNormal; easing.type: Easing.OutCubic } }
                    }
                }

                SpeedGraph {
                    visible: page.details && card.history.length > 1
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    history: card.history
                    speed: card.speed
                    peak: card.peak
                    label: card.speedText
                }

                GridLayout {
                    visible: page.details
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 4
                    Text { textFormat: Text.PlainText; text: i18nc("@label", "Name:"); font: Kit.body; color: Kit.colors.textSecondary }
                    Text { textFormat: Text.PlainText; Layout.fillWidth: true; text: card.name; font: Kit.body; color: Kit.colors.textPrimary; elide: Text.ElideMiddle }
                    Text { textFormat: Text.PlainText; visible: !card.finishing; text: i18nc("@label", "Time remaining:"); font: Kit.body; color: Kit.colors.textSecondary }
                    Text { textFormat: Text.PlainText; visible: !card.finishing; Layout.fillWidth: true; text: card.remaining; font: Kit.body; color: Kit.colors.textPrimary }
                    Text { textFormat: Text.PlainText; visible: !card.finishing; text: i18nc("@label", "Items remaining:"); font: Kit.body; color: Kit.colors.textSecondary }
                    Text { textFormat: Text.PlainText; visible: !card.finishing; Layout.fillWidth: true; text: card.items; font: Kit.body; color: Kit.colors.textPrimary }
                }

                // the copy is over, the drive is still being written
                Text {
                    visible: card.finishing
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: i18nc("@info", "The files are being written to the drive. Don't remove it until this is done.")
                    font: Kit.body
                    color: Kit.colors.textSecondary
                    wrapMode: Text.Wrap
                }

                // an error, inline in its card
                Rectangle {
                    visible: card.error.length > 0
                    Layout.fillWidth: true
                    implicitHeight: errorColumn.implicitHeight + 24
                    radius: Kit.radiusControl
                    color: Kit.alpha(Kit.colors.statusCritical, 0.12)
                    border.width: 1
                    border.color: Kit.alpha(Kit.colors.statusCritical, 0.4)
                    ColumnLayout {
                        id: errorColumn
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: card.error
                            font: Kit.body
                            color: Kit.colors.textPrimary
                            wrapMode: Text.Wrap
                            Accessible.role: Accessible.AlertMessage
                        }
                        OptionCheckBox {
                            id: forAll
                            visible: card.errorMultiple
                            text: i18nc("@option:check", "Do this for all current items")
                        }
                        RowLayout {
                            spacing: 8
                            CommandButton {
                                visible: card.errorCanRetry
                                kind: "accent"
                                text: i18nc("@action:button", "Try again")
                                onClicked: page.model.answerError(card.index, 0, false)
                            }
                            CommandButton {
                                kind: "standard"
                                text: i18nc("@action:button", "Skip")
                                onClicked: page.model.answerError(card.index, 1, forAll.checked)
                            }
                            CommandButton {
                                kind: "standard"
                                text: i18nc("@action:button", "Cancel")
                                onClicked: page.model.answerError(card.index, 2, false)
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Kit.colors.strokeDivider
        }

        CommandButton {
            Layout.leftMargin: 12
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            text: page.details ? i18nc("@action:button", "Fewer details") : i18nc("@action:button", "More details")
            icon.name: page.details ? "go-up" : "go-down"
            onClicked: {
                page.details = !page.details
                page.detailsToggled(page.details)
            }
        }
    }
}
