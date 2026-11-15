/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archivewriter.h"

#include "archiveengine_p.h"
#include "archivepaths.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>

#include <sys/stat.h>

namespace ra = raye::archive;

namespace Raye::Archive
{

namespace
{
struct FileId {
    dev_t dev;
    ino_t ino;
    bool operator==(const FileId &o) const
    {
        return dev == o.dev && ino == o.ino;
    }
};

// Bytes under path, links followed or not; a folder already on the way
// down (a link back up) is not counted twice.
void sizeOf(const QString &path, bool keepLinks, QList<FileId> &ancestors, qint64 *total)
{
    struct stat st;
    const QByteArray encoded = QFile::encodeName(path);
    if (::lstat(encoded.constData(), &st) != 0) {
        return;
    }
    if (S_ISLNK(st.st_mode)) {
        if (keepLinks || ::stat(encoded.constData(), &st) != 0) {
            return;
        }
    }
    if (S_ISREG(st.st_mode)) {
        *total += st.st_size;
        return;
    }
    if (!S_ISDIR(st.st_mode)) {
        return;
    }
    const FileId id{st.st_dev, st.st_ino};
    if (ancestors.contains(id)) {
        return;
    }
    ancestors.append(id);
    const QStringList names = QDir(path).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Unsorted);
    for (const QString &name : names) {
        sizeOf(path + QLatin1Char('/') + name, keepLinks, ancestors, total);
    }
    ancestors.removeLast();
}

ra::Format engineFormat(const CreateOptions &o)
{
    switch (o.format) {
    case Format::Zip:
        return ra::Format::Zip;
    case Format::SevenZip:
        return ra::Format::SevenZip;
    case Format::Tar:
        switch (o.tarFilter) {
        case TarFilter::None:
            return ra::Format::Tar;
        case TarFilter::Gzip:
            return ra::Format::Gzip;
        case TarFilter::Bzip2:
            return ra::Format::Bzip2;
        case TarFilter::Xz:
            return ra::Format::Xz;
        case TarFilter::Zstd:
            return ra::Format::Zstd;
        }
    }
    return ra::Format::Zip;
}

ra::WriteOptions writeOptions(const CreateOptions &o)
{
    ra::WriteOptions w;
    w.format = engineFormat(o);
    w.level = o.compressionLevel();
    if (!o.method.isEmpty()) {
        if (auto m = ra::method_from_name(o.method.toStdString())) {
            w.method = *m;
        }
    }
    w.dictionary = quint64(qMax<qint64>(0, o.dictionary));
    w.word_size = o.wordSize;
    w.solid = o.solid;
    w.solid_block_size = quint64(qMax<qint64>(0, o.solidBlockSize));
    w.threads = o.threads;
    w.volume_size = quint64(qMax<qint64>(0, o.volumeSize));
    if (!o.password.isEmpty()) {
        w.password = o.password.toStdString();
        w.encryption = (o.format == Format::Zip && o.zipCrypto) ? ra::Encryption::ZipCrypto : ra::Encryption::Aes256;
        w.encrypt_names = o.encryptNames && o.format == Format::SevenZip;
    }
    w.store_mtime = o.storeModified;
    w.store_atime = o.storeAccessed;
    w.store_ctime = o.storeCreated;
    w.store_links = o.keepSymlinks;
    for (const QString &pattern : o.excludes) {
        const QString p = pattern.trimmed();
        if (!p.isEmpty()) {
            w.exclude.push_back(p.toStdString());
        }
    }
    return w;
}

// The archive's own path inside the archive, when it is written into one of
// the folders being compressed: it is left out, never stored in itself.
QString selfPath(const QString &archive, const QStringList &sources)
{
    const QString cleanArchive = QDir::cleanPath(QFileInfo(archive).absoluteFilePath());
    for (const QString &source : sources) {
        const QFileInfo info(source);
        if (!info.isDir()) {
            continue;
        }
        const QString dir = QDir::cleanPath(info.absoluteFilePath());
        if (cleanArchive.startsWith(dir + QLatin1Char('/'))) {
            return info.fileName() + cleanArchive.mid(dir.size());
        }
    }
    return QString();
}

std::vector<ra::InputItem> inputs(const QStringList &sources)
{
    std::vector<ra::InputItem> items;
    items.reserve(size_t(sources.size()));
    for (const QString &source : sources) {
        ra::InputItem item;
        item.source = toPath(QFileInfo(source).absoluteFilePath());
        item.archive_path = QFileInfo(source).fileName().toStdString();
        items.push_back(std::move(item));
    }
    return items;
}

// "x.7z" also counts as taken when its first volume "x.7z.001" exists.
bool archiveExists(const QString &path, qint64 volumeSize)
{
    struct stat st;
    if (::lstat(QFile::encodeName(path).constData(), &st) == 0) {
        return true;
    }
    return volumeSize > 0 && ::lstat(QFile::encodeName(path + QStringLiteral(".001")).constData(), &st) == 0;
}
}

qint64 Writer::totalSize(const QStringList &sources, const CreateOptions &options)
{
    qint64 total = 0;
    QList<FileId> ancestors;
    for (const QString &source : sources) {
        sizeOf(source, options.keepSymlinks, ancestors, &total);
    }
    return total;
}

bool Writer::create(const QString &archivePath, const QStringList &sources, const CreateOptions &options, const Progress &progress)
{
    m_error = Error::None;
    m_detail.clear();
    const bool exists = archiveExists(archivePath, options.volumeSize);
    if (options.mode == UpdateMode::Create && exists) {
        m_error = Error::WriteFailed;
        m_detail = archivePath;
        return false;
    }
    ra::WriteOptions w = writeOptions(options);
    const QString self = selfPath(archivePath, sources);
    if (!self.isEmpty()) {
        w.exclude.push_back(self.toStdString());
    }
    ra::Control ownControl;
    ra::Control *control = m_control ? m_control : &ownControl;
    ra::Callbacks callbacks;
    callbacks.control = control;
    callbacks.progress = [&](const ra::Progress &p) {
        if (progress && !progress(qint64(p.bytes_done))) {
            control->cancel();
        }
    };
    try {
        const auto items = inputs(sources);
        const std::filesystem::path path = toPath(archivePath);
        if (!exists || options.mode == UpdateMode::Create) {
            ra::create(path, items, w, callbacks);
        } else if (options.mode == UpdateMode::Add) {
            ra::add(path, items, w, callbacks);
        } else if (options.mode == UpdateMode::Update) {
            ra::update(path, items, w, callbacks);
        } else {
            ra::sync(path, items, w, callbacks);
        }
    } catch (const ra::Error &e) {
        m_error = mapError(e.code());
        m_detail = QString::fromUtf8(e.what());
        return false;
    } catch (const std::exception &e) {
        m_error = Error::WriteFailed;
        m_detail = QString::fromUtf8(e.what());
        return false;
    }
    if (control->cancelled()) {
        m_error = Error::Cancelled;
        return false;
    }
    if (progress) {
        progress(totalSize(sources, options));
    }
    return true;
}

bool Writer::rewriteZip(const QString &archivePath, const QList<Change> &additions, const QStringList &removals, bool overwrite)
{
    m_error = Error::None;
    m_detail.clear();
    for (const Change &change : additions) {
        QString clean;
        if (cleanEntryPath(change.path, &clean) != PathVerdict::Ok || clean != change.path) {
            m_error = Error::Unsafe;
            return false;
        }
    }
    try {
        const QString format = detectFormat(archivePath);
        ra::WriteOptions w;
        if (format == QLatin1String("zip")) {
            w.format = ra::Format::Zip;
        } else if (format == QLatin1String("7z")) {
            w.format = ra::Format::SevenZip;
        } else if (format == QLatin1String("tar")) {
            w.format = ra::Format::Tar;
        } else {
            m_error = QFileInfo(archivePath).isFile() ? Error::NotAnArchive : Error::CantOpen;
            return false;
        }
        {
            ra::Archive a = ra::Archive::open(toPath(archivePath));
            if (a.info().has_encrypted_entries || a.info().encrypted_headers || a.info().volume_count > 1) {
                m_error = Error::Unsupported;
                return false;
            }
            if (!overwrite) {
                QSet<QString> taken;
                for (const ra::Entry &e : a.entries()) {
                    QString clean;
                    if (cleanEntryPath(fromStd(e.path), &clean) == PathVerdict::Ok) {
                        taken.insert(clean);
                    }
                }
                for (const Change &change : additions) {
                    if (!change.isFolder && taken.contains(change.path)) {
                        m_error = Error::WriteFailed;
                        m_detail = change.path;
                        return false;
                    }
                }
            }
        }
        // A new file keeps the old one's permissions.
        struct stat before;
        const bool hadMode = ::stat(QFile::encodeName(archivePath).constData(), &before) == 0;
        const std::filesystem::path path = toPath(archivePath);
        ra::Callbacks callbacks;
        callbacks.control = m_control;
        if (!removals.isEmpty()) {
            std::vector<std::string> paths;
            for (const QString &r : removals) {
                paths.push_back(r.toStdString());
            }
            ra::remove(path, paths, w, callbacks);
        }
        if (!additions.isEmpty()) {
            QTemporaryDir empty;
            std::vector<ra::InputItem> items;
            for (const Change &change : additions) {
                ra::InputItem item;
                item.source = toPath(change.isFolder ? empty.path() : change.sourceFile);
                item.archive_path = change.path.toStdString();
                items.push_back(std::move(item));
            }
            ra::add(path, items, w, callbacks);
        }
        if (hadMode) {
            ::chmod(QFile::encodeName(archivePath).constData(), before.st_mode & 07777);
        }
        return true;
    } catch (const ra::Error &e) {
        m_error = mapError(e.code());
        m_detail = QString::fromUtf8(e.what());
        return false;
    } catch (const std::exception &e) {
        m_error = Error::WriteFailed;
        m_detail = QString::fromUtf8(e.what());
        return false;
    }
}

}
