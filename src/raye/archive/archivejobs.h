/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archiveentry.h"
#include "archiveexport.h"
#include "archiveformat.h"

#include <KJob>

#include <QList>
#include <QPointer>
#include <QWidget>
#include <QStringList>

#include <memory>

namespace Raye::Archive
{

class JobRelay;

// The archive jobs of Files: they run in a thread of their own, report
// bytes and a description through KJob (so the Raye progress window and
// the taskbar show them like a copy) and stop when killed. error() is
// KJob::UserDefinedError plus the Error value, errorText() the sentence
// for the user; archiveError() gives the Error itself.
class RAYE_ARCHIVE_EXPORT ArchiveJob : public KJob
{
    Q_OBJECT
public:
    ~ArchiveJob() override;

    Error archiveError() const
    {
        return m_archiveError;
    }
    // The engine's own words, for the log.
    QString detail() const
    {
        return m_detail;
    }

protected:
    explicit ArchiveJob(QObject *parent);
    bool doKill() override;

    // Runs work in a thread. work gets the relay to report progress on and
    // to check for a kill; it returns the outcome.
    struct Outcome {
        Error error = Error::None;
        QString detail;
        QStringList items;
        QList<Entry> entries;
        int skipped = 0;
        // extracted, waiting for the conflicts to be answered
        QString staging;
    };
    void run(std::function<Outcome(JobRelay *)> work);
    // The outcome is in; error fields are already set. Returning true
    // keeps the job running: it calls emitResult() itself later.
    virtual bool finished(const Outcome &outcome);
    bool doSuspend() override;
    bool doResume() override;

private:
    std::shared_ptr<JobRelay> m_relay;
    Error m_archiveError = Error::None;
    QString m_detail;
};

// What an extraction does with a name already in the destination.
enum class Conflicts {
    // the new item gets " (2)"
    KeepBoth,
    Replace,
    // the Raye "Replace or skip files" window asks, item by item
    Ask,
};

// "Extract all": everything into destination, made if missing. The
// archive goes into a hidden folder inside destination first and is moved
// out only once it is all there and checked, so a refused, damaged or
// cancelled archive leaves nothing behind.
class RAYE_ARCHIVE_EXPORT ArchiveExtractJob : public ArchiveJob
{
    Q_OBJECT
public:
    ArchiveExtractJob(const QString &archivePath, const QString &destination, QObject *parent = nullptr);

    void setPassword(const QString &password)
    {
        m_password = password;
    }
    // Off once the user said go on to "it expands to much more than its size".
    void setGuardEnabled(bool enabled)
    {
        m_guard = enabled;
    }
    void setOverwrite(bool overwrite)
    {
        m_conflicts = overwrite ? Conflicts::Replace : Conflicts::KeepBoth;
    }
    void setConflicts(Conflicts conflicts)
    {
        m_conflicts = conflicts;
    }
    // An archive holding just a folder named like the destination
    // ("Photos.zip" with "Photos/" inside) does not give Photos/Photos.
    void setCollapseSingleFolder(bool collapse)
    {
        m_collapse = collapse;
    }
    // Where the questions are asked.
    void setWindow(QWidget *window)
    {
        m_window = window;
    }

    QString archivePath() const
    {
        return m_archive;
    }
    QString destination() const
    {
        return m_destination;
    }
    // What is now in destination, full paths, in archive order.
    QStringList extractedItems() const
    {
        return m_items;
    }

    void start() override;

protected:
    bool finished(const Outcome &outcome) override;

private:
    void done(const QString &staging);

    QString m_archive;
    QString m_destination;
    QString m_password;
    bool m_guard = true;
    Conflicts m_conflicts = Conflicts::KeepBoth;
    bool m_collapse = false;
    QPointer<QWidget> m_window;
    QStringList m_items;
};

// "Compress to": a new archive from local files and folders.
class RAYE_ARCHIVE_EXPORT ArchiveCreateJob : public ArchiveJob
{
    Q_OBJECT
public:
    ArchiveCreateJob(const QStringList &sources, const QString &archivePath, const CreateOptions &options, QObject *parent = nullptr);

    QString archivePath() const
    {
        return m_archive;
    }

    void start() override;

protected:
    bool finished(const Outcome &outcome) override;

private:
    QStringList m_sources;
    QString m_archive;
    CreateOptions m_options;
};

// Reads the list of entries (the Details pane, a preview).
class RAYE_ARCHIVE_EXPORT ArchiveListJob : public ArchiveJob
{
    Q_OBJECT
public:
    explicit ArchiveListJob(const QString &archivePath, QObject *parent = nullptr);

    QList<Entry> entries() const
    {
        return m_entries;
    }
    int skippedUnsafe() const
    {
        return m_skipped;
    }

    void start() override;

protected:
    bool finished(const Outcome &outcome) override;

private:
    QString m_archive;
    QList<Entry> m_entries;
    int m_skipped = 0;
};

// Moves what a staging folder holds into destination (the last step of an
// extraction), merging folders that are already there. Existing files are
// kept and the new one renamed, unless overwrite is set. A link already in
// destination is never walked into. Returns the full paths of the items
// placed at the top, or false when something could not be moved.
RAYE_ARCHIVE_EXPORT bool moveInto(const QString &staging, const QString &destination, bool overwrite, QStringList *placed);

}
