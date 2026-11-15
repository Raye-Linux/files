/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archiveentry.h"
#include "archiveexport.h"

#include <QList>
#include <QString>

namespace raye::archive
{
class Control;
}

namespace Raye::Archive
{

// Reads an archive with the Raye archive engine: ZIP, 7z, RAR 1.5 to 5,
// TAR and its compressed forms, ISO, CAB, cpio, ar and more. The format is
// found from the content, never the name. Blocking: the jobs run it in a
// thread, the KIO worker in its own process.
class RAYE_ARCHIVE_EXPORT Reader
{
public:
    explicit Reader(const QString &archivePath);

    void setPassword(const QString &password)
    {
        m_password = password;
    }
    // Pause and cancel from another thread (the job's control).
    void setControl(raye::archive::Control *control)
    {
        m_control = control;
    }
    // Off: BombGuard does not stop extractAll() (the user said go on).
    void setGuardEnabled(bool enabled)
    {
        m_guard = enabled;
    }

    // Every entry with a safe name. Entries whose name leads outside the
    // archive are left out and counted in skippedUnsafe(). Folders that
    // only appear in the paths of their files are not added here.
    bool list(QList<Entry> *entries);

    // Writes the data of one file entry to sink, in pieces. sink returns
    // false to stop (the call then fails with Cancelled).
    bool read(const QString &entryPath, const std::function<bool(const char *, qint64)> &sink);

    // Extracts everything into root, an empty folder made for it. All or
    // nothing: on Unsafe, the caller throws root away. Names are cleaned,
    // nothing is written through a link, links that lead outside are
    // refused, set-user-id bits and device files are dropped. progress gets
    // the bytes written so far; total, when given, the size everything
    // unpacks to as soon as it is known.
    bool extractAll(const QString &root, const Progress &progress = {}, const std::function<void(qint64)> &total = {});

    // Format and layout in a few words ("RAR 5.0, solid"), once list() ran.
    QString formatDetail() const
    {
        return m_formatDetail;
    }
    // The archive has a protected entry or protected names.
    bool encrypted() const
    {
        return m_encrypted;
    }

    Error error() const
    {
        return m_error;
    }
    // The engine's own words, for the log only.
    QString detail() const
    {
        return m_detail;
    }
    int skippedUnsafe() const
    {
        return m_skipped;
    }

    // Whether this build reads archives at all.
    static bool available();

private:
    friend class ReaderPrivate;

    QString m_path;
    QString m_password;
    bool m_guard = true;
    Error m_error = Error::None;
    QString m_detail;
    int m_skipped = 0;
    QString m_formatDetail;
    bool m_encrypted = false;
    raye::archive::Control *m_control = nullptr;
};

}
