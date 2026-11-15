/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "locations/locationnames.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

class QAction;

namespace Raye::Commands
{

// What the command bar knows of the window: where it is and what is
// selected. The main window and the pages (This PC, Gallery...) fill it in.
struct CommandContext {
    QUrl url;
    // The selected items and their MIME types, in the same order.
    QList<QUrl> selection;
    QStringList selectionMimeTypes;
    // False in a folder that cannot be written (New, Cut, Paste, Rename and
    // Delete are then disabled).
    bool writable = true;
    // A search is showing its results.
    bool searching = false;
    // A drive is selected (This PC): its Solid UDI and whether it can be
    // ejected or safely removed.
    QString driveUdi;
    bool driveRemovable = false;
};

// Context buttons (explorer spec 1.3): extra buttons for a kind of place
// or selection. They join the bar after View, each set its own group;
// a set may instead replace New and the editing buttons.
enum class Placement {
    Join,
    ReplaceEditing,
};

using Predicate = std::function<bool(const CommandContext &)>;

struct ContextSet {
    QString id;
    QList<QAction *> actions;
    Predicate predicate;
    Placement placement = Placement::Join;
};

}
