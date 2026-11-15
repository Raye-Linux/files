/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archiveexport.h"

#include <QString>

// Path rules for what comes out of an archive. Every name read from an
// archive goes through cleanEntryPath() before it touches the disk, and
// every symbolic link through linkTargetStaysInside() and resolvesInside().
namespace Raye::Archive
{

enum class PathVerdict {
    Ok,
    // "", "/", "./": nothing to write (the root of the archive)
    Empty,
    // a ".." component: it would land outside the destination
    Traversal,
    // a name no file system takes (too long, a NUL)
    Invalid,
};

// Turns an entry name into a clean relative path ("docs/a.txt"). Backslashes
// count as separators (archives made on Windows), "." and empty components
// are dropped, and a leading "/" or drive letter is removed, so an absolute
// name lands inside the destination like it does with tar. Any ".." is
// refused: there is no safe way to keep it.
RAYE_ARCHIVE_EXPORT PathVerdict cleanEntryPath(const QString &name, QString *clean);

// Whether a symbolic link stored at entryPath (a clean path) pointing to
// target stays inside the extracted tree, reading the target as text. An
// absolute target never does.
RAYE_ARCHIVE_EXPORT bool linkTargetStaysInside(const QString &entryPath, const QString &target);

// Whether path, on disk, ends up inside root once every link on the way is
// followed. A part that does not exist yet is read as text, so a link that
// would only escape once a missing folder appears is caught too.
RAYE_ARCHIVE_EXPORT bool resolvesInside(const QString &root, const QString &path);

}
