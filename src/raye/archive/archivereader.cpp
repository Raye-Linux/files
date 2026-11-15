/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archivereader.h"

#include "archiveengine_p.h"
#include "archiveguard.h"
#include "archivepaths.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLoggingCategory>

#include <fcntl.h>
#include <ftw.h>
#include <sys/stat.h>

#include <limits>

Q_LOGGING_CATEGORY(RayeArchiveLog, "org.raye.files.archive", QtWarningMsg)

namespace ra = raye::archive;

namespace Raye::Archive
{

namespace
{
// A link inside the archive may point to another link: this many at most.
constexpr int linkDepthLimit = 8;

QDateTime toDate(const std::optional<ra::Timestamp> &t)
{
    if (!t) {
        return QDateTime();
    }
    return QDateTime::fromMSecsSinceEpoch(t->seconds * 1000 + t->nanoseconds / 1000000);
}

bool isRegularFile(const QString &path)
{
    struct stat st;
    return ::stat(QFile::encodeName(path).constData(), &st) == 0 && S_ISREG(st.st_mode);
}

class CallbackSink : public ra::Sink
{
public:
    explicit CallbackSink(const std::function<bool(const char *, qint64)> &sink)
        : m_sink(sink)
    {
    }
    void write(const void *data, std::size_t n) override
    {
        if (!m_sink(static_cast<const char *>(data), qint64(n))) {
            ra::fail(ra::ErrorCode::Cancelled, "cancelled");
        }
    }

private:
    const std::function<bool(const char *, qint64)> &m_sink;
};

// What Files gives every extracted item, whatever the archive says: the
// owner can always read and write it, nobody else can write.
int tidyMode(const char *path, const struct stat *st, int type, struct FTW *)
{
    if (type == FTW_SL || type == FTW_SLN) {
        return 0;
    }
    mode_t mode = st->st_mode & 07777 & ~mode_t(S_ISUID | S_ISGID | 022);
    mode |= (type == FTW_D || type == FTW_DP) ? 0700 : 0600;
    if ((st->st_mode & 07777) != mode) {
        ::chmod(path, mode);
    }
    return 0;
}

ra::Archive openArchive(const QString &path, const QString &password)
{
    ra::OpenOptions options;
    if (!password.isEmpty()) {
        options.password = password.toStdString();
    }
    return ra::Archive::open(toPath(path), options);
}
}

bool Reader::available()
{
    return true;
}

Reader::Reader(const QString &archivePath)
    : m_path(archivePath)
{
}

#define RAYE_ARCHIVE_CATCH                                                                                                                                     \
    catch (const ra::Error &e)                                                                                                                                 \
    {                                                                                                                                                          \
        m_error = mapError(e.code());                                                                                                                          \
        m_detail = QString::fromUtf8(e.what());                                                                                                                \
        qCDebug(RayeArchiveLog) << m_path << m_detail;                                                                                                         \
        return false;                                                                                                                                          \
    }                                                                                                                                                          \
    catch (const std::exception &e)                                                                                                                            \
    {                                                                                                                                                          \
        m_error = Error::Corrupt;                                                                                                                              \
        m_detail = QString::fromUtf8(e.what());                                                                                                                \
        return false;                                                                                                                                          \
    }

bool Reader::list(QList<Entry> *entries)
{
    entries->clear();
    m_skipped = 0;
    m_error = Error::None;
    if (!isRegularFile(m_path)) {
        m_error = Error::CantOpen;
        return false;
    }
    try {
        ra::Archive a = openArchive(m_path, m_password);
        m_formatDetail = fromStd(a.info().detail);
        m_encrypted = a.info().encrypted_headers || a.info().has_encrypted_entries;
        const auto &all = a.entries();
        entries->reserve(qsizetype(all.size()));
        for (const ra::Entry &e : all) {
            QString clean;
            const PathVerdict verdict = cleanEntryPath(fromStd(e.path), &clean);
            if (verdict == PathVerdict::Empty) {
                continue;
            }
            if (verdict != PathVerdict::Ok) {
                ++m_skipped;
                continue;
            }
            Entry out;
            out.path = clean;
            switch (e.type) {
            case ra::EntryType::File:
                out.type = Entry::File;
                break;
            case ra::EntryType::Directory:
                out.type = Entry::Folder;
                break;
            case ra::EntryType::Symlink:
                out.type = Entry::Symlink;
                out.linkTarget = fromStd(e.link_target);
                break;
            case ra::EntryType::Hardlink: {
                QString target;
                if (cleanEntryPath(fromStd(e.link_target), &target) != PathVerdict::Ok) {
                    ++m_skipped;
                    continue;
                }
                out.type = Entry::File;
                out.hardlink = true;
                out.linkTarget = target;
                break;
            }
            case ra::EntryType::Other:
                out.type = Entry::Other;
                break;
            }
            if (out.type == Entry::Symlink && !linkTargetStaysInside(clean, out.linkTarget)) {
                ++m_skipped;
                continue;
            }
            out.size = e.size_known ? qint64(e.size) : -1;
            out.modified = toDate(e.mtime);
            out.permissions = e.mode ? (*e.mode & 0777) : (out.type == Entry::Folder ? 0755 : 0644);
            out.encrypted = e.encrypted;
            entries->append(out);
        }
        return true;
    }
    RAYE_ARCHIVE_CATCH
}

bool Reader::read(const QString &entryPath, const std::function<bool(const char *, qint64)> &sink)
{
    m_error = Error::None;
    if (!isRegularFile(m_path)) {
        m_error = Error::CantOpen;
        return false;
    }
    try {
        ra::Archive a = openArchive(m_path, m_password);
        const auto &all = a.entries();
        auto find = [&all](const QString &path) -> const ra::Entry * {
            // the last one wins, like on extraction
            for (auto it = all.rbegin(); it != all.rend(); ++it) {
                QString clean;
                if (cleanEntryPath(fromStd(it->path), &clean) == PathVerdict::Ok && clean == path) {
                    return &*it;
                }
            }
            return nullptr;
        };
        const ra::Entry *entry = find(entryPath);
        for (int depth = 0; entry && depth < linkDepthLimit; ++depth) {
            QString next;
            if (entry->type == ra::EntryType::Hardlink) {
                if (cleanEntryPath(fromStd(entry->link_target), &next) != PathVerdict::Ok) {
                    entry = nullptr;
                    break;
                }
            } else if (entry->type == ra::EntryType::Symlink) {
                QString clean;
                cleanEntryPath(fromStd(entry->path), &clean);
                const QString target = fromStd(entry->link_target);
                if (!linkTargetStaysInside(clean, target)) {
                    entry = nullptr;
                    break;
                }
                const int slash = clean.lastIndexOf(QLatin1Char('/'));
                const QString joined = QDir::cleanPath((slash > 0 ? clean.left(slash) + QLatin1Char('/') : QString()) + target);
                if (cleanEntryPath(joined, &next) != PathVerdict::Ok) {
                    entry = nullptr;
                    break;
                }
            } else {
                break;
            }
            entry = find(next);
        }
        if (!entry || entry->type != ra::EntryType::File) {
            m_error = Error::NotFound;
            return false;
        }
        ra::ExtractOptions options;
        if (!m_password.isEmpty()) {
            options.password = m_password.toStdString();
        }
        ra::Callbacks callbacks;
        callbacks.control = m_control;
        CallbackSink out(sink);
        a.extract_to(entry->index, out, options, callbacks);
        out.finish();
        return true;
    }
    RAYE_ARCHIVE_CATCH
}

bool Reader::extractAll(const QString &root, const Progress &progress, const std::function<void(qint64)> &total)
{
    m_error = Error::None;
    if (!isRegularFile(m_path)) {
        m_error = Error::CantOpen;
        return false;
    }
    ra::Control ownControl;
    ra::Control *control = m_control ? m_control : &ownControl;
    try {
        ra::Archive a = openArchive(m_path, m_password);
        m_formatDetail = fromStd(a.info().detail);

        ra::ExtractOptions options;
        // root is a fresh folder: a name met twice is the later entry
        options.overwrite = ra::OverwriteMode::Replace;
        if (!m_password.isEmpty()) {
            options.password = m_password.toStdString();
        }
        // "/etc/x" lands in root/etc/x, like tar does
        options.safety.allow_absolute = true;
        options.safety.keep_setuid = false;
        if (m_guard) {
            options.safety.max_ratio = double(BombGuard::ratioLimit);
            options.safety.ratio_grace = quint64(BombGuard::sizeLimit);
            options.safety.max_entries = quint64(BombGuard::itemLimit);
        } else {
            options.safety.max_ratio = std::numeric_limits<double>::max();
            options.safety.max_entries = std::numeric_limits<std::uint64_t>::max();
            // the sizes it declares are what the warning was about
            options.safety.check_free_space = false;
        }

        ra::Callbacks callbacks;
        callbacks.control = control;
        std::uint64_t knownTotal = 0;
        callbacks.progress = [&](const ra::Progress &p) {
            if (total && p.bytes_total && p.bytes_total != knownTotal) {
                knownTotal = p.bytes_total;
                total(qint64(knownTotal));
            }
            if (progress && !progress(qint64(p.bytes_done))) {
                control->cancel();
            }
        };
        a.extract_all(toPath(root), options, callbacks);
        if (control->cancelled()) {
            m_error = Error::Cancelled;
            return false;
        }
        // Links that each stay inside can still lead out together
        // ("a/b" -> "." then "l" -> "a/b/../.."): check where they end up.
        QDirIterator it(root, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (it.fileInfo().isSymLink() && !resolvesInside(root, path)) {
                qCWarning(RayeArchiveLog) << "refusing link" << path << "of" << m_path << "(leads outside)";
                m_error = Error::Unsafe;
                return false;
            }
        }
        ::nftw(QFile::encodeName(root).constData(), tidyMode, 32, FTW_PHYS);
        return true;
    }
    RAYE_ARCHIVE_CATCH
}

}
