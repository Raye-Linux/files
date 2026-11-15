/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QList>
#include <QUrl>

class QWidget;

namespace Raye::Properties
{

// Shows Properties for these items: the Raye card for local files, folders
// and drives, KDE's dialog for anything else (a phone, a share). Deleted
// on close.
DOLPHIN_EXPORT void showProperties(const QList<QUrl> &urls, QWidget *parent = nullptr);

}
