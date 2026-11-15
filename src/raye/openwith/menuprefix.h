/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QByteArray>
#include <QStringList>

namespace Raye::OpenWith
{

// The app database is built from <XDG_MENU_PREFIX>applications.menu. Only
// the desktop session sets the prefix; with none, it reads
// applications.menu, which Raye does not ship, and holds no app at all.
// Returns the prefix to use when current is empty and configDirs have no
// applications.menu: raye-, else plasma-, if that menu is there. Empty
// means leave it as it is.
DOLPHIN_EXPORT QByteArray missingMenuPrefix(const QByteArray &current, const QStringList &configDirs);

}
