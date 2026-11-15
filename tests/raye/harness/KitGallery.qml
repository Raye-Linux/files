/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The QML controls of org.raye.files in their states, for the render
// harness (--widget KitQml, KitQmlDialog). Same rows as the widget page.

import QtQuick
import QtQuick.Templates as T
import org.raye.files
import org.raye.files.kit

Rectangle {
    id: root

    property bool french: false
    property bool showDialog: false
    function t(en, fr) { return french ? fr : en }

    color: Kit.colors.bgBase

    component Caption: Text {
        font: Kit.caption
        color: Kit.colors.textSecondary
    }
    component Heading: Text {
        font: Kit.bodyStrong
        color: Kit.colors.textPrimary
    }

    Column {
        x: 32
        y: 24
        spacing: 14

        Text {
            text: root.t("Raye QML controls", "Contrôles QML Raye")
            font: Kit.subtitle
            color: Kit.colors.textPrimary
        }

        Grid {
            id: grid
            columns: 6
            columnSpacing: 24
            rowSpacing: 14
            verticalItemAlignment: Grid.AlignVCenter

            Item { width: 130; height: 1 }
            Caption { text: root.t("Normal", "Normal") }
            Caption { text: root.t("Pressed", "Appuyé") }
            Caption { text: root.t("Disabled", "Désactivé") }
            Caption { text: root.t("Checked", "Coché") }
            Caption { text: root.t("Keyboard focus", "Focus clavier") }

            Caption { text: root.t("Icon button", "Bouton icône") }
            Repeater {
                model: 5
                CommandButton {
                    required property int index
                    text: root.t("Copy", "Copier")
                    display: T.AbstractButton.IconOnly
                    icon.name: "edit-copy"
                    down: index === 1 ? true : undefined
                    enabled: index !== 2
                    checkable: index === 3
                    checked: index === 3
                }
            }

            Caption { text: root.t("Command button", "Bouton de commande") }
            Repeater {
                model: 5
                CommandButton {
                    required property int index
                    text: root.t("Sort", "Trier")
                    icon.name: "view-sort"
                    showChevron: true
                    down: index === 1 ? true : undefined
                    enabled: index !== 2
                    checkable: index === 3
                    checked: index === 3
                }
            }

            Caption { text: root.t("Standard button", "Bouton standard") }
            Repeater {
                model: 5
                CommandButton {
                    required property int index
                    kind: "standard"
                    width: Math.max(96, implicitWidth)
                    text: root.t("Cancel", "Annuler")
                    down: index === 1 ? true : undefined
                    enabled: index !== 2
                    checkable: index === 3
                    checked: index === 3
                }
            }

            Caption { text: root.t("Accent button", "Bouton principal") }
            Repeater {
                model: 5
                CommandButton {
                    id: accent
                    required property int index
                    kind: "accent"
                    width: Math.max(96, implicitWidth)
                    text: root.t("Save", "Enregistrer")
                    down: index === 1 ? true : undefined
                    enabled: index !== 2
                    Component.onCompleted: if (index === 4 && !root.showDialog) focusTimer.target = accent
                }
            }

            Caption { text: root.t("Split button", "Bouton partagé") }
            Repeater {
                model: 5
                SplitButton {
                    required property int index
                    kind: index === 3 ? "standard" : "subtle"
                    text: root.t("Extract all", "Tout extraire")
                    icon.name: "archive-extract"
                    enabled: index !== 2
                }
            }
        }

        Item { width: 1; height: 8 }
        Heading { text: root.t("Pivot", "Onglets") }
        PivotBar {
            model: [root.t("Recent", "Récents"), root.t("Favorites", "Favoris"), root.t("Shared", "Partagés")]
            currentIndex: 1
        }

        Heading { text: root.t("Drive usage", "Espace utilisé") }
        Row {
            spacing: 48
            Column {
                spacing: 6
                UsageBar { width: 240; value: 0.45; animateOnShow: false }
                Caption { text: root.t("212 GB free of 476 GB", "212 Go libres sur 476 Go") }
            }
            Column {
                spacing: 6
                UsageBar { width: 240; value: 0.95; animateOnShow: false }
                Caption { text: root.t("11.8 GB free of 238 GB", "11,8 Go libres sur 238 Go") }
            }
        }

        Heading { text: root.t("Separators", "Séparateurs") }
        Row {
            spacing: 12
            Caption { text: "A"; anchors.verticalCenter: parent.verticalCenter }
            Separator {}
            Caption { text: "B"; anchors.verticalCenter: parent.verticalCenter }
            Separator {}
            Caption { text: "C"; anchors.verticalCenter: parent.verticalCenter }
        }
    }

    // A flyout open on the right, as under a command bar button.
    CommandButton {
        id: shareButton
        x: 640
        y: 340
        text: root.t("Share", "Partager")
        icon.name: "emblem-shared-symbolic"
        showChevron: true
    }
    Flyout {
        id: flyout
        x: shareButton.x
        y: shareButton.y + shareButton.height + 4
        closePolicy: Flyout.NoAutoClose
        contentItem: Column {
            padding: 4
            spacing: 2
            Heading { text: root.t("Share", "Partager"); leftPadding: 8; bottomPadding: 4 }
            CommandButton { width: 220; alignment: Qt.AlignLeft; text: root.t("Copy link", "Copier le lien"); icon.name: "edit-copy" }
            CommandButton { width: 220; alignment: Qt.AlignLeft; text: root.t("Send by email", "Envoyer par e-mail"); icon.name: "mail-message-new" }
            CommandButton { width: 220; alignment: Qt.AlignLeft; text: "Bluetooth"; icon.name: "preferences-system-bluetooth"; tintIcon: false }
        }
    }

    CardDialog {
        id: dialog
        title: root.t("Replace or skip files", "Remplacer ou ignorer des fichiers")
        primaryText: root.t("Replace", "Remplacer")
        secondaryText: root.t("Skip", "Ignorer")
        closeText: root.t("Cancel", "Annuler")
        width: 480
        contentItem: Text {
            text: root.t("The destination already has a file named \"report.pdf\".",
                         "La destination contient déjà un fichier nommé « report.pdf ».")
            wrapMode: Text.Wrap
            font: Kit.body
            color: Kit.colors.textPrimary
        }
    }

    Timer {
        id: focusTimer
        property Item target: null
        interval: 0
        running: true
        onTriggered: {
            if (root.showDialog)
                dialog.open()
            else
                flyout.open()
            if (target)
                target.forceActiveFocus(Qt.TabFocusReason)
        }
    }
}
