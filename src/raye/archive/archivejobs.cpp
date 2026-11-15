/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archivejobs.h"

#include "archivejobs_p.h"
#include "archivereader.h"
#include "archivewriter.h"

#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KIO/JobUiDelegateFactory>
#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QThread>

#include <cerrno>
#include <cstdlib>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>

namespace Raye::Archive
{

namespace
{
bool lstatIs(const QString &path, mode_t type, bool *exists)
{
    struct stat st;
    *exists = ::lstat(QFile::encodeName(path).constData(), &st) == 0;
    return *exists && (st.st_mode & S_IFMT) == type;
}

bool moveOne(const QString &from, const QString &to)
{
    return ::rename(QFile::encodeName(from).constData(), QFile::encodeName(to).constData()) == 0;
}

bool hasConflict(const QString &staging, const QString &destination)
{
    const QStringList names = QDir(staging).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Unsorted);
    for (const QString &name : names) {
        bool exists = false;
        lstatIs(destination + QLatin1Char('/') + name, S_IFDIR, &exists);
        if (exists) {
            return true;
        }
    }
    return false;
}

bool merge(const QString &staging, const QString &destination, bool overwrite, QStringList *placed)
{
    const QStringList names = QDir(staging).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Unsorted);
    for (const QString &name : names) {
        const QString from = staging + QLatin1Char('/') + name;
        QString to = destination + QLatin1Char('/') + name;
        bool fromExists = false;
        bool toExists = false;
        const bool fromIsDir = lstatIs(from, S_IFDIR, &fromExists);
        const bool toIsDir = lstatIs(to, S_IFDIR, &toExists);
        if (!toExists) {
            if (!moveOne(from, to)) {
                return false;
            }
        } else if (fromIsDir && toIsDir) {
            // Both real folders: their contents meet.
            if (!merge(from, to, overwrite, nullptr)) {
                return false;
            }
        } else if (overwrite && !toIsDir) {
            if (::unlink(QFile::encodeName(to).constData()) != 0 || !moveOne(from, to)) {
                return false;
            }
        } else {
            to = destination + QLatin1Char('/') + uniqueFileName(destination, name);
            if (!moveOne(from, to)) {
                return false;
            }
        }
        if (placed) {
            placed->append(to);
        }
    }
    return true;
}
}

bool moveInto(const QString &staging, const QString &destination, bool overwrite, QStringList *placed)
{
    placed->clear();
    return merge(staging, destination, overwrite, placed);
}

// ---- ArchiveJob -------------------------------------------------------------

ArchiveJob::ArchiveJob(QObject *parent)
    : KJob(parent)
    , m_relay(new JobRelay, [](JobRelay *relay) {
        relay->deleteLater();
    })
{
    setCapabilities(KJob::Killable | KJob::Suspendable);
}

ArchiveJob::~ArchiveJob()
{
    // A thread still running finds the flag and stops on its own.
    m_relay->killed = true;
    m_relay->control.cancel();
}

bool ArchiveJob::doSuspend()
{
    m_relay->control.pause();
    return true;
}

bool ArchiveJob::doResume()
{
    m_relay->control.resume();
    return true;
}

bool ArchiveJob::doKill()
{
    m_relay->killed = true;
    m_relay->control.cancel();
    // KJob reports the kill itself; the thread's late answer is not wanted.
    m_relay->disconnect(this);
    return true;
}

bool ArchiveJob::finished(const Outcome &)
{
    return false;
}

void ArchiveJob::run(std::function<Outcome(JobRelay *)> work)
{
    auto result = std::make_shared<Outcome>();
    auto mutex = std::make_shared<std::mutex>();
    std::shared_ptr<JobRelay> relay = m_relay;

    connect(relay.get(), &JobRelay::totalKnown, this, [this](qint64 total) {
        setTotalAmount(KJob::Bytes, qMax<qint64>(total, 1));
    });
    connect(relay.get(), &JobRelay::progressed, this, [this](qint64 done) {
        setProcessedAmount(KJob::Bytes, done);
    });
    connect(relay.get(), &JobRelay::done, this, [this, result, mutex] {
        Outcome outcome;
        {
            std::lock_guard lock(*mutex);
            outcome = *result;
        }
        m_archiveError = outcome.error;
        m_detail = outcome.detail;
        if (outcome.error != Error::None) {
            setError(KJob::UserDefinedError + int(outcome.error));
            setErrorText(errorMessage(outcome.error));
        } else {
            // A ZIP is read from its index, not to its last byte.
            setProcessedAmount(KJob::Bytes, totalAmount(KJob::Bytes));
        }
        if (!finished(outcome)) {
            emitResult();
        }
    });

    QThread *thread = QThread::create([relay, result, mutex, work] {
        Outcome outcome = work(relay.get());
        if (relay->killed && outcome.error == Error::None) {
            outcome.error = Error::Cancelled;
        }
        {
            std::lock_guard lock(*mutex);
            *result = outcome;
        }
        Q_EMIT relay->done();
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start(QThread::LowPriority);
}

// ---- ArchiveExtractJob ------------------------------------------------------

ArchiveExtractJob::ArchiveExtractJob(const QString &archivePath, const QString &destination, QObject *parent)
    : ArchiveJob(parent)
    , m_archive(QFileInfo(archivePath).absoluteFilePath())
    , m_destination(QDir::cleanPath(QFileInfo(destination).absoluteFilePath()))
{
}

void ArchiveExtractJob::start()
{
    Q_EMIT description(this,
                       i18nc("@title job", "Extracting"),
                       qMakePair(i18nc("The source of a job", "From"), QFileInfo(m_archive).fileName()),
                       qMakePair(i18nc("The destination of a job", "To"), m_destination));
    setTotalAmount(KJob::Bytes, qMax<qint64>(QFileInfo(m_archive).size(), 1));

    const QString archive = m_archive;
    const QString destination = m_destination;
    const QString password = m_password;
    const bool guard = m_guard;
    const Conflicts conflicts = m_conflicts;
    const bool collapse = m_collapse;
    run([=](JobRelay *relay) {
        Outcome outcome;
        const bool existed = QFileInfo::exists(destination);
        if (!existed && !QDir().mkpath(destination)) {
            outcome.error = Error::WriteFailed;
            return outcome;
        }
        if (!QFileInfo(destination).isDir()) {
            outcome.error = Error::WriteFailed;
            return outcome;
        }
        QByteArray staging = QFile::encodeName(destination + QStringLiteral("/.raye-extract-XXXXXX"));
        if (!::mkdtemp(staging.data())) {
            outcome.error = Error::WriteFailed;
            if (!existed) {
                QDir().rmdir(destination);
            }
            return outcome;
        }
        const QString stagingPath = QFile::decodeName(staging);

        Reader reader(archive);
        reader.setPassword(password);
        reader.setGuardEnabled(guard);
        reader.setControl(&relay->control);
        bool ok = reader.extractAll(
            stagingPath,
            [relay](qint64 done) {
                Q_EMIT relay->progressed(done);
                return !relay->killed;
            },
            [relay](qint64 total) {
                Q_EMIT relay->totalKnown(total);
            });
        if (ok && relay->killed) {
            ok = false;
            outcome.error = Error::Cancelled;
        } else if (!ok) {
            outcome.error = reader.error();
            outcome.detail = reader.detail();
        }

        // "Photos.zip" holding only "Photos/": its content goes straight in.
        QString from = stagingPath;
        if (ok && collapse && !existed) {
            const QStringList top = QDir(stagingPath).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
            bool isDir = false;
            if (top.size() == 1 && lstatIs(stagingPath + QLatin1Char('/') + top.first(), S_IFDIR, &isDir)
                && top.first().compare(QFileInfo(destination).fileName(), Qt::CaseInsensitive) == 0) {
                from = stagingPath + QLatin1Char('/') + top.first();
            }
        }
        if (ok && conflicts == Conflicts::Ask && hasConflict(from, destination)) {
            // Answered in the main thread by the Raye conflict window.
            outcome.staging = stagingPath;
            outcome.items = QStringList{from};
            return outcome;
        }
        if (ok && !moveInto(from, destination, conflicts == Conflicts::Replace, &outcome.items)) {
            outcome.error = Error::WriteFailed;
        }
        // Whatever is left (all of it when it failed) goes. Links inside
        // are removed, never followed.
        QDir(stagingPath).removeRecursively();
        if (outcome.error != Error::None && !existed) {
            QDir().rmdir(destination);
        }
        return outcome;
    });
}

bool ArchiveExtractJob::finished(const Outcome &outcome)
{
    m_items = outcome.items;
    if (outcome.staging.isEmpty()) {
        return false;
    }
    const QString staging = outcome.staging;
    const QString from = outcome.items.value(0, staging);
    QList<QUrl> sources;
    const QStringList names = QDir(from).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QString &name : names) {
        sources.append(QUrl::fromLocalFile(from + QLatin1Char('/') + name));
    }
    m_items.clear();
    KIO::CopyJob *move = KIO::move(sources, QUrl::fromLocalFile(m_destination), KIO::HideProgressInfo);
    move->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingDisabled, m_window));
    connect(move, &KIO::CopyJob::copyingDone, this, [this](KIO::Job *, const QUrl &, const QUrl &to, const QDateTime &, bool, bool) {
        if (QFileInfo(to.toLocalFile()).absolutePath() == m_destination) {
            m_items.append(to.toLocalFile());
        }
    });
    connect(move, &KJob::result, this, [this, staging](KJob *job) {
        QDir(staging).removeRecursively();
        if (job->error() == KJob::KilledJobError || job->error() == KIO::ERR_USER_CANCELED) {
            setError(KJob::UserDefinedError + int(Error::Cancelled));
            setErrorText(errorMessage(Error::Cancelled));
        } else if (job->error()) {
            setError(KJob::UserDefinedError + int(Error::WriteFailed));
            setErrorText(job->errorString());
        }
        emitResult();
    });
    return true;
}

// ---- ArchiveCreateJob -------------------------------------------------------

ArchiveCreateJob::ArchiveCreateJob(const QStringList &sources, const QString &archivePath, const CreateOptions &options, QObject *parent)
    : ArchiveJob(parent)
    , m_sources(sources)
    , m_archive(QFileInfo(archivePath).absoluteFilePath())
    , m_options(options)
{
}

void ArchiveCreateJob::start()
{
    Q_EMIT description(this,
                       i18nc("@title job", "Compressing"),
                       qMakePair(i18nc("The source of a job", "From"),
                                 m_sources.size() == 1 ? QFileInfo(m_sources.first()).fileName() : i18np("%1 item", "%1 items", m_sources.size())),
                       qMakePair(i18nc("The destination of a job", "To"), m_archive));
    const QStringList sources = m_sources;
    const QString archive = m_archive;
    const CreateOptions options = m_options;
    run([=](JobRelay *relay) {
        Outcome outcome;
        Q_EMIT relay->totalKnown(Writer::totalSize(sources, options));
        Writer writer;
        writer.setControl(&relay->control);
        if (!writer.create(archive, sources, options, [relay](qint64 done) {
                Q_EMIT relay->progressed(done);
                return !relay->killed;
            })) {
            outcome.error = relay->killed ? Error::Cancelled : writer.error();
            outcome.detail = writer.detail();
        }
        return outcome;
    });
}

bool ArchiveCreateJob::finished(const Outcome &outcome)
{
    if (outcome.error != Error::None || !m_options.deleteAfter) {
        return false;
    }
    // "Delete files after compression" sends them to the Recycle Bin: the
    // archive is there and checked, the originals can still come back.
    QList<QUrl> urls;
    for (const QString &source : std::as_const(m_sources)) {
        urls.append(QUrl::fromLocalFile(source));
    }
    KIO::Job *trash = KIO::trash(urls, KIO::HideProgressInfo);
    connect(trash, &KJob::result, this, [this] {
        emitResult();
    });
    return true;
}

// ---- ArchiveListJob ---------------------------------------------------------

ArchiveListJob::ArchiveListJob(const QString &archivePath, QObject *parent)
    : ArchiveJob(parent)
    , m_archive(archivePath)
{
}

void ArchiveListJob::start()
{
    const QString archive = m_archive;
    run([=](JobRelay *) {
        Outcome outcome;
        Reader reader(archive);
        if (!reader.list(&outcome.entries)) {
            outcome.error = reader.error();
            outcome.detail = reader.detail();
        }
        outcome.skipped = reader.skippedUnsafe();
        return outcome;
    });
}

bool ArchiveListJob::finished(const Outcome &outcome)
{
    m_entries = outcome.entries;
    m_skipped = outcome.skipped;
    return false;
}

}
