/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archiveexport.h"

#include <QDateTime>
#include <QString>

#include <functional>

namespace Raye::Archive
{

struct RAYE_ARCHIVE_EXPORT Entry {
    enum Type {
        File,
        Folder,
        Symlink,
        // anything else (devices, pipes): listed, never written to disk
        Other,
    };

    // Clean relative path ("docs/a.txt"), see cleanEntryPath().
    QString path;
    Type type = File;
    // -1 when the archive does not say
    qint64 size = -1;
    QDateTime modified;
    // permission bits only (0777)
    uint permissions = 0644;
    // a Symlink's target, or for a hard link (a File with hardlink set)
    // the clean path of the entry it shares its data with
    QString linkTarget;
    bool hardlink = false;
    bool encrypted = false;
};

enum class Error {
    None,
    // the file cannot be opened (missing, no permission)
    CantOpen,
    // not an archive Files reads, or a format option it does not have
    NotAnArchive,
    // damaged or cut short
    Corrupt,
    PasswordNeeded,
    WrongPassword,
    // names or links that lead outside the destination
    Unsafe,
    // BombGuard stopped it
    TooLarge,
    Cancelled,
    // the destination cannot be written (full disk, permission)
    WriteFailed,
    // no such entry in the archive
    NotFound,
    // a format or feature Files does not handle (RAR writing, old ciphers)
    Unsupported,
    // a part of a split archive is not next to the others
    MissingPart,
};

// The sentence shown to the user for an error, in plain words.
RAYE_ARCHIVE_EXPORT QString errorMessage(Error error);

// Called while working with the bytes done so far; returning false cancels.
using Progress = std::function<bool(qint64 done)>;

}
