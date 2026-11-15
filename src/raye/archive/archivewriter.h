/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archiveentry.h"
#include "archiveexport.h"
#include "archiveformat.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace raye::archive
{
class Control;
}

namespace Raye::Archive
{

// Writes archives with the Raye archive engine: ZIP, 7z and TAR (with gzip,
// bzip2, xz or zstd), on every core. Blocking, like Reader. The file appears
// only once it is complete: it is written next to its final place under a
// hidden name, then moved.
class RAYE_ARCHIVE_EXPORT Writer
{
public:
    Writer() = default;

    // Bytes the sources hold, for a progress bar (links counted as their
    // target unless options keep them).
    static qint64 totalSize(const QStringList &sources, const CreateOptions &options);

    // Creates archivePath from local files and folders. Each source is
    // stored under its own name ("Photos/1.jpg"). With UpdateMode::Create
    // the file must not exist yet; the other modes change an existing
    // archive (and create it when it is not there). progress gets the bytes
    // of the sources read so far.
    bool create(const QString &archivePath, const QStringList &sources, const CreateOptions &options, const Progress &progress = {});

    // One change for rewriteZip().
    struct Change {
        // Clean path inside the archive.
        QString path;
        // A local file to store there; empty with isFolder for a folder.
        QString sourceFile;
        bool isFolder = false;
    };

    // ZIP, 7z or TAR: writes a copy of archivePath with the entries under
    // each path of removals left out and additions put in, then swaps it
    // in. An addition over an existing entry fails with WriteFailed unless
    // overwrite is set. Protected archives are refused (Unsupported).
    bool rewriteZip(const QString &archivePath, const QList<Change> &additions, const QStringList &removals, bool overwrite = false);

    Error error() const
    {
        return m_error;
    }
    QString detail() const
    {
        return m_detail;
    }

    // Pause and cancel from another thread.
    void setControl(raye::archive::Control *control)
    {
        m_control = control;
    }

private:
    Error m_error = Error::None;
    QString m_detail;
    raye::archive::Control *m_control = nullptr;
};

}
