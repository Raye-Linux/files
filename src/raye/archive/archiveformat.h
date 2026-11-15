/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archiveexport.h"

#include <QString>
#include <QStringList>

// What Files reads and writes, and how archives are named (explorer spec
// 1.12 and 3.12).
namespace Raye::Archive
{

// The formats "Compress to" writes. RAR, ISO and CAB are read only.
enum class Format {
    Zip,
    SevenZip,
    Tar,
};

// The levels of the Add to archive window, as 7-Zip names them. The
// numbers are the engine's 0..9 scale.
enum class Level {
    Store = 0,
    Fastest = 1,
    Fast = 3,
    Normal = 5,
    Maximum = 7,
    Ultra = 9,
};

// The compression wrapped around a TAR.
enum class TarFilter {
    None,
    Gzip,
    Bzip2,
    Xz,
    Zstd,
};

// What to do when the archive already exists (Add to archive window).
enum class UpdateMode {
    // a new archive; an existing file is an error
    Create,
    // add, replacing entries with the same name
    Add,
    // add only what is new or newer
    Update,
    // like Update, and drop entries whose file is gone
    Sync,
};

struct RAYE_ARCHIVE_EXPORT CreateOptions {
    Format format = Format::Zip;
    Level level = Level::Normal;
    TarFilter tarFilter = TarFilter::None;
    // Off: a link is stored as the file or folder it points to.
    bool keepSymlinks = false;

    // Advanced options. Empty or 0 means "what the level picks".
    QString method;
    qint64 dictionary = 0;
    int wordSize = 0;
    bool solid = true;
    qint64 solidBlockSize = 0;
    int threads = 0;
    qint64 volumeSize = 0;
    QString password;
    bool encryptNames = false;
    // ZIP only: the old encryption Windows opens on its own.
    bool zipCrypto = false;
    UpdateMode mode = UpdateMode::Create;
    bool deleteAfter = false;
    bool storeModified = true;
    bool storeAccessed = false;
    bool storeCreated = false;
    QStringList excludes;

    // ".zip", ".7z", ".tar", ".tar.gz", ".tar.bz2", ".tar.xz", ".tar.zst"
    QString extension() const;
    // The engine's level, 0 (Store) to 9 (Ultra).
    int compressionLevel() const;
};

// Methods the format can write, engine names ("Deflate", "LZMA2"...),
// the default first.
RAYE_ARCHIVE_EXPORT QStringList methodsFor(Format format);

// What the engine finds in a file by its content, whatever its name:
// "zip", "7z", "rar", "rar5", "tar", "gz", ... or empty.
RAYE_ARCHIVE_EXPORT QString detectFormat(const QString &path);

// Opened as a folder in Files: ZIP, 7z, RAR, TAR (plain, gz, bz2, xz, zst,
// lzma), ISO and CAB. Exact MIME types only: an OpenDocument file is a ZIP
// inside but must open in its app, not as a folder.
RAYE_ARCHIVE_EXPORT bool isBrowsableMimeType(const QString &mimeType);
RAYE_ARCHIVE_EXPORT QStringList browsableMimeTypes();

// Files can add to and remove from these in place: ZIP, 7z and plain TAR.
RAYE_ARCHIVE_EXPORT bool isWritableMimeType(const QString &mimeType);

// The short name of the format of a file ("ZIP", "7z", "RAR", "TAR", "ISO",
// "CAB"), from its name; empty when it is not an archive Files knows.
RAYE_ARCHIVE_EXPORT QString formatName(const QString &fileName);

// "Files can't add to RAR archives." for a format Files only reads.
RAYE_ARCHIVE_EXPORT QString cannotWriteMessage(const QString &fileName);

// The archive extension of a file name, compound ones included: ".zip",
// ".tar.gz", ".tgz"; empty when it has none Files knows.
RAYE_ARCHIVE_EXPORT QString archiveExtension(const QString &fileName);

// "Holiday.tar.gz" → "Holiday": the folder Extract all offers.
RAYE_ARCHIVE_EXPORT QString baseNameOf(const QString &archiveFileName);

// The name "Compress to" gives the archive, before the extension: the
// name of the first item, without its extension for a file ("Report.docx"
// → "Report"), whole for a folder (pass isDir).
RAYE_ARCHIVE_EXPORT QString archiveBaseNameFor(const QString &firstItemName, bool isDir);

// A file name free in folder: fileName itself, or "Report (2).zip",
// "Report (3).zip"... The extension stays whole ("Logs (2).tar.gz").
RAYE_ARCHIVE_EXPORT QString uniqueFileName(const QString &folder, const QString &fileName);

// Whether name can be a file name: not empty, no "/", no NUL, not "." or "..".
RAYE_ARCHIVE_EXPORT bool isValidFileName(const QString &name);

}
