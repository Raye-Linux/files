/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QList>
#include <QUrl>

// What dropping files on a place does, the Windows way (explorer spec 3.5):
// on the same drive they move, on another drive they are copied; Shift
// moves, Ctrl copies, Alt (or Ctrl+Shift) makes a link; onto the Recycle
// Bin they always go to the bin.
namespace Raye::NavPane
{

DOLPHIN_EXPORT Qt::DropAction dropAction(Qt::KeyboardModifiers modifiers, bool sameDrive, bool toRecycleBin);

// Whether every source sits on the drive of the target. Local paths are
// compared by device (the nearest existing folder of each), remote ones by
// scheme, host and port. Anything unclear counts as another drive, so the
// safe answer (a copy) wins.
DOLPHIN_EXPORT bool sameDrive(const QList<QUrl> &sources, const QUrl &target);

// A drop that would do nothing or break something: a folder into itself or
// below itself, or a file onto the folder it is already in with a move.
DOLPHIN_EXPORT bool isPointlessDrop(const QList<QUrl> &sources, const QUrl &target, Qt::DropAction action);

}
