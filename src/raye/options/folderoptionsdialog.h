/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

class QWidget;

namespace Raye::Ui
{
class CardDialog;
}

namespace Raye::Options
{

class FolderOptions;

// Folder options (explorer spec 3.15): General, View and Search on a Raye
// pivot, then OK / Apply / Cancel. Replaces Dolphin's settings dialog
// ("…" › Options). Deleted on close. tab: 0 General, 1 View, 2 Search.
DOLPHIN_EXPORT Raye::Ui::CardDialog *folderOptionsDialog(QWidget *parent, int tab = 0, FolderOptions *options = nullptr);

}
