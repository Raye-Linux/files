/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import org.kde.kirigami as Kirigami
import org.raye.files
import org.raye.files.kit

// The body of Properties (explorer spec 3.15): the item at the top with its
// name ready to edit, then General, Security and, for media, Details. A
// drive shows its space as a ring and a Clean up button. OK, Apply and
// Cancel are the card's buttons.
//
// One height whatever the tab, so the card never resizes after it maps.
Item {
    id: page

    property var model: null
    readonly property int labelWidth: 132

    implicitWidth: 452
    // A network place with no space to show has only a few lines.
    implicitHeight: model && model.network && !model.hasSpace ? 220 : 440

    function showTab(index) {
        tabs.currentIndex = Math.min(index, tabNames.length - 1)
    }

    function focusField() {
        if (nameField.visible && nameField.enabled)
            nameField.forceActiveFocus()
        else
            tabs.forceActiveFocus()
    }

    readonly property var tabNames: {
        if (!model)
            return []
        if (model.drive)
            return [i18nc("@title:tab", "General")]
        const names = [i18nc("@title:tab", "General"), i18nc("@title:tab", "Security")]
        if (model.details.length > 0)
            names.push(i18nc("@title:tab", "Details"))
        return names
    }

    component Label: Text {
        Layout.preferredWidth: page.labelWidth
        Layout.alignment: Qt.AlignTop
        textFormat: Text.PlainText
        font: Kit.body
        color: Kit.colors.textSecondary
        elide: Text.ElideRight
    }
    component Value: Text {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        font: Kit.body
        color: Kit.colors.textPrimary
        wrapMode: Text.Wrap
        maximumLineCount: 3
        elide: Text.ElideRight
    }
    component Divider: Rectangle {
        Layout.fillWidth: true
        Layout.columnSpan: 2
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        implicitHeight: 1
        color: Kit.colors.strokeDivider
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            Kirigami.Icon {
                implicitWidth: 56
                implicitHeight: 56
                source: page.model ? page.model.iconName : ""
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                ArchiveTextField {
                    id: nameField
                    Layout.fillWidth: true
                    visible: page.model && page.model.canRename
                    text: page.model ? page.model.name : ""
                    Accessible.name: i18nc("@label", "Name")
                    onTextEdited: page.model.name = text
                }
                Text {
                    Layout.fillWidth: true
                    visible: !nameField.visible
                    textFormat: Text.PlainText
                    text: page.model ? page.model.name : ""
                    font: Kit.subtitle
                    color: Kit.colors.textPrimary
                    elide: Text.ElideMiddle
                }
                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: nameField.visible ? 11 : 0
                    textFormat: Text.PlainText
                    text: page.model ? page.model.typeText : ""
                    font: Kit.caption
                    color: Kit.colors.textSecondary
                    elide: Text.ElideRight
                }
            }
        }

        PivotBar {
            id: tabs
            visible: page.tabNames.length > 1
            model: page.tabNames
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            // General
            Item {
                ColumnLayout {
                    visible: page.model && !page.model.drive
                    width: parent.width
                    spacing: 8

                    InfoCard {
                        // not from the row inside: a hidden card hides it too
                        visible: page.model && page.model.count === 1 && !page.model.folder
                        Label { text: i18nc("@label", "Opens with:"); Layout.alignment: Qt.AlignVCenter }
                        RowLayout {
                            id: openWith
                            Layout.fillWidth: true
                            spacing: 8
                            Kirigami.Icon {
                                implicitWidth: 20
                                implicitHeight: 20
                                visible: page.model && page.model.openWithIcon.length > 0
                                source: page.model ? page.model.openWithIcon : ""
                            }
                            Value {
                                text: page.model && page.model.openWithName.length > 0 ? page.model.openWithName
                                                                                         : i18nc("@info no app opens this file", "No app chosen")
                            }
                            CommandButton {
                                kind: "standard"
                                text: i18nc("@action:button choose the app that opens a file", "Change…")
                                onClicked: page.model.changeApp()
                            }
                        }
                    }

                    InfoCard {
                        Label { text: i18nc("@label", "Location:") }
                        Value { text: page.model ? page.model.location : "" }
                        Label { text: i18nc("@label", "Size:") }
                        Value { text: page.model && !page.model.measuring ? page.model.sizeText : i18nc("@info still counting", "Counting…") }
                        Label { text: i18nc("@label", "Size on disk:") }
                        Value { text: page.model && !page.model.measuring ? page.model.onDiskText : i18nc("@info still counting", "Counting…") }
                        Label { text: i18nc("@label", "Contains:"); visible: contains.visible }
                        Value {
                            id: contains
                            visible: page.model && (page.model.folder || page.model.count > 1)
                            text: page.model && !page.model.measuring ? page.model.containsText : i18nc("@info still counting", "Counting…")
                        }
                    }

                    InfoCard {
                        visible: page.model && (page.model.created.length > 0 || page.model.modified.length > 0 || page.model.accessed.length > 0)
                        Label { text: i18nc("@label", "Created:"); visible: created.visible }
                        Value { id: created; visible: text.length > 0; text: page.model ? page.model.created : "" }
                        Label { text: i18nc("@label", "Modified:"); visible: modified.visible }
                        Value { id: modified; visible: text.length > 0; text: page.model ? page.model.modified : "" }
                        Label { text: i18nc("@label", "Accessed:"); visible: accessed.visible }
                        Value { id: accessed; visible: text.length > 0; text: page.model ? page.model.accessed : "" }
                    }

                    InfoCard {
                        Label { text: i18nc("@label", "Attributes:"); Layout.alignment: Qt.AlignVCenter }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 24
                            OptionCheckBox {
                                text: i18nc("@option:check file attribute", "Read-only")
                                enabled: page.model && page.model.canChange
                                checked: page.model ? page.model.readOnly : false
                                onToggled: page.model.readOnly = checked
                            }
                            OptionCheckBox {
                                text: i18nc("@option:check file attribute", "Hidden")
                                enabled: page.model && page.model.canHide
                                checked: page.model ? page.model.hidden : false
                                onToggled: page.model.hidden = checked
                            }
                        }
                    }
                }

                // A drive: what it is, then its space as a ring.
                ColumnLayout {
                    visible: page.model && page.model.drive
                    width: parent.width
                    spacing: 16

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 8
                        readonly property bool network: page.model && page.model.network
                        readonly property bool space: page.model && page.model.hasSpace
                        Label { text: i18nc("@label", "File system:"); visible: !parent.network }
                        Value { text: page.model ? page.model.fileSystem : ""; visible: !parent.network }
                        Label { text: i18nc("@label shared folder on another computer", "Folder:"); visible: parent.network && folderValue.text.length > 0 }
                        Value { id: folderValue; text: page.model ? page.model.networkFolder : ""; visible: parent.network && text.length > 0 }
                        Label { text: i18nc("@label computer a share is on", "Computer:"); visible: parent.network }
                        Value { text: page.model ? page.model.server : ""; visible: parent.network }
                        Label { text: i18nc("@label network protocol", "Connected through:"); visible: parent.network }
                        Value { text: page.model ? page.model.protocol : ""; visible: parent.network }
                        Label { text: i18nc("@label user name used for a share", "Signed in as:"); visible: parent.network && asValue.text.length > 0 }
                        Value { id: asValue; text: page.model ? page.model.connectedAs : ""; visible: parent.network && text.length > 0 }
                        Label { text: i18nc("@label folder a network drive is in", "Shown in:"); visible: parent.network && atValue.text.length > 0 }
                        Value { id: atValue; text: page.model ? page.model.mountedAt : ""; visible: parent.network && text.length > 0 }
                        Divider { visible: parent.space }
                        Label { text: i18nc("@label drive space", "Used space:"); visible: parent.space }
                        Value { text: page.model ? page.model.usedText : ""; visible: parent.space }
                        Label { text: i18nc("@label drive space", "Free space:"); visible: parent.space }
                        Value { text: page.model ? page.model.freeText : ""; visible: parent.space }
                        Divider { visible: parent.space }
                        Label { text: i18nc("@label drive space", "Capacity:"); visible: parent.space }
                        Value { text: page.model ? page.model.capacityText : ""; visible: parent.space }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: page.model && page.model.hasSpace
                        spacing: 24
                        Item {
                            id: ring
                            implicitWidth: 120
                            implicitHeight: 120
                            readonly property real used: page.model ? page.model.usage : 0
                            readonly property color usedColor: used > 0.9 ? Kit.colors.statusCaution : Kit.colors.accent
                            Shape {
                                anchors.fill: parent
                                preferredRendererType: Shape.CurveRenderer
                                ShapePath {
                                    fillColor: "transparent"
                                    strokeColor: Kit.colors.strokeControlSolid
                                    strokeWidth: 14
                                    capStyle: ShapePath.FlatCap
                                    PathAngleArc { centerX: 60; centerY: 60; radiusX: 52; radiusY: 52; startAngle: 0; sweepAngle: 360 }
                                }
                                ShapePath {
                                    fillColor: "transparent"
                                    strokeColor: ring.usedColor
                                    strokeWidth: 14
                                    capStyle: ShapePath.FlatCap
                                    PathAngleArc { centerX: 60; centerY: 60; radiusX: 52; radiusY: 52; startAngle: -90; sweepAngle: 360 * ring.used }
                                }
                            }
                            Text {
                                anchors.centerIn: parent
                                textFormat: Text.PlainText
                                text: Math.round(ring.used * 100) + " %"
                                font: Kit.bodyStrong
                                color: Kit.colors.textPrimary
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                Layout.fillWidth: true
                                visible: page.model && !page.model.network
                                textFormat: Text.PlainText
                                text: i18nc("@info", "Free up space by removing temporary files, old updates and restore points you no longer need.")
                                font: Kit.caption
                                color: Kit.colors.textSecondary
                                wrapMode: Text.Wrap
                            }
                            CommandButton {
                                kind: "standard"
                                visible: page.model && !page.model.network
                                text: i18nc("@action:button free up space on a drive", "Clean up")
                                onClicked: page.model.cleanUp()
                            }
                        }
                    }
                }
            }

            // Security
            ColumnLayout {
                spacing: 8
                InfoCard {
                    Label { text: i18nc("@label", "Owner:") }
                    Value { text: page.model ? page.model.ownerName : "" }
                    Label { text: i18nc("@label", "Group:") }
                    Value { text: page.model ? page.model.groupName : "" }
                }
                InfoCard {
                    columns: 1
                    Text {
                        Layout.bottomMargin: 2
                        textFormat: Text.PlainText
                        text: page.model && page.model.folder ? i18nc("@title", "Who can open this folder")
                                                              : i18nc("@title", "Who can open this file")
                        font: Kit.bodyStrong
                        color: Kit.colors.textPrimary
                    }
                    Repeater {
                        model: [
                            { who: 0, name: page.model ? i18nc("@label permission for the owner", "Owner (%1)", page.model.ownerName) : "" },
                            { who: 1, name: page.model ? i18nc("@label permission for the group", "Group (%1)", page.model.groupName) : "" },
                            { who: 2, name: i18nc("@label permission for everyone else", "Everyone else") }
                        ]
                        RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 12
                            Label { text: modelData.name; Layout.preferredWidth: 128; Layout.alignment: Qt.AlignVCenter }
                            ArchiveChoice {
                                enabled: page.model && page.model.canChange
                                accessibleName: modelData.name
                                options: [i18nc("@option permission", "None"), i18nc("@option permission", "Read"), i18nc("@option permission", "Read and write")]
                                currentIndex: !page.model ? 0 : modelData.who === 0 ? page.model.ownerAccess
                                                           : modelData.who === 1 ? page.model.groupAccess : page.model.othersAccess
                                onActivated: index => {
                                    if (modelData.who === 0) page.model.ownerAccess = index
                                    else if (modelData.who === 1) page.model.groupAccess = index
                                    else page.model.othersAccess = index
                                }
                            }
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    visible: page.model && !page.model.canChange
                    textFormat: Text.PlainText
                    text: i18nc("@info", "Only the owner can change who can open this.")
                    font: Kit.caption
                    color: Kit.colors.textSecondary
                    wrapMode: Text.Wrap
                }
                Item { Layout.fillHeight: true }
                CommandButton {
                    kind: "subtle"
                    text: i18nc("@action:button the full permissions dialog", "Advanced settings…")
                    onClicked: page.model.advanced()
                }
            }

            // Details
            Flickable {
                id: detailsFlick
                WheelScroll { target: detailsFlick }
                clip: true
                contentHeight: details.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                GridLayout {
                    id: details
                    width: parent.width
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8
                    Repeater {
                        model: page.model ? page.model.details : []
                        Label {
                            required property var modelData
                            required property int index
                            Layout.row: index
                            Layout.column: 0
                            text: modelData.label
                        }
                    }
                    Repeater {
                        model: page.model ? page.model.details : []
                        Value {
                            required property var modelData
                            required property int index
                            Layout.row: index
                            Layout.column: 1
                            text: modelData.value
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: page.model && page.model.error.length > 0
            textFormat: Text.PlainText
            text: page.model ? page.model.error : ""
            font: Kit.caption
            color: Kit.colors.statusCritical
            wrapMode: Text.Wrap
        }
    }
}
