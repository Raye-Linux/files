/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QString>
#include <QStringList>

// The Type column, the details pane and the status bar say what a file is
// the way Windows does: "Text document", "PNG file", "Compressed (zipped)
// folder", "Application" (explorer spec 3.6). A table covers the common
// types; anything else keeps the description shared-mime-info gives.
namespace Raye::TypeNames
{

// The Raye name of a MIME type (aliases are followed), or the fallback when
// the table has none. An empty fallback gives the shared-mime-info comment.
DOLPHIN_EXPORT QString name(const QString &mimeType, const QString &fallback = QString());

// Whether the table has its own name for this type (aliases followed).
DOLPHIN_EXPORT bool hasName(const QString &mimeType);

// Every MIME type of the table, as written there.
DOLPHIN_EXPORT QStringList mimeTypes();

}
