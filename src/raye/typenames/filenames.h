/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QString>

// File names as the views show them (explorer spec 3.6, "Hide known
// extensions"). Like Windows, the extension of a known type is not shown:
// "report.pdf" reads "report". Unlike Windows, programs and scripts always
// show theirs, so "photo.jpg.exe" can never pass for a picture.
namespace Raye::FileNames
{

// The extension the type database knows for this name, without the dot
// ("pdf", "tar.gz"), or an empty string. A name that is only an extension
// (".bashrc") has none.
DOLPHIN_EXPORT QString knownExtension(const QString &fileName);

// True for names whose extension is always shown: programs, installers,
// scripts and launchers (.exe .msi .bat .cmd .sh .desktop .AppImage .lnk and
// the like), whatever the setting.
DOLPHIN_EXPORT bool alwaysShowsExtension(const QString &fileName);

// What a view writes for this item. Folders keep their whole name, and so
// does a file the system may run (executable bit set), an unknown
// extension, or every file when showExtensions is on.
DOLPHIN_EXPORT QString displayName(const QString &fileName, bool isDir, bool showExtensions, bool isExecutable = false);

// How many characters of the name inline rename selects: the name without
// its extension (the known one, else what follows the last dot), the whole
// name for a folder or a name with no real extension. Counted in QChars.
DOLPHIN_EXPORT int baseNameLength(const QString &fileName, bool isDir);

// The Size column of Details, as Windows fills it: whole kilobytes rounded
// up ("1 KB" for a single byte, "0 KB" only when empty), with the locale's
// digit grouping ("3,412 KB", "3 412 Ko").
DOLPHIN_EXPORT QString sizeColumnText(qint64 bytes);

}
