/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The rayearchive: KIO worker (explorer spec 3.12). It lets the view, the
// thumbnails, drag and drop and the Open dialogs walk into a ZIP, 7z, RAR,
// TAR, ISO or CAB like into a folder, and add to or take from a ZIP.
//
// Built as its own plugin, not into the private library: this file is .cc
// so the glob in src/raye/CMakeLists.txt leaves it alone.

#include "archive/archiveformat.h"
#include "archive/archivepaths.h"
#include "archive/archivereader.h"
#include "archive/archiveurl.h"
#include "archive/archivewriter.h"
#include "kioworker/archiveindex.h"
#include "kioworker/workertext.h"

#include <KIO/AuthInfo>
#include <KIO/WorkerBase>
#include <KLocalizedString>

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QTemporaryFile>

#include <sys/stat.h>

using namespace Raye::Archive;

namespace
{

bool isRegularFile(const QString &path)
{
    struct stat st;
    return ::stat(QFile::encodeName(path).constData(), &st) == 0 && S_ISREG(st.st_mode);
}

KIO::WorkerResult failWith(Error error, const QString &what)
{
    switch (error) {
    case Error::CantOpen:
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, what);
    case Error::NotFound:
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, what);
    case Error::Cancelled:
        return KIO::WorkerResult::fail(KIO::ERR_USER_CANCELED, what);
    default:
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, errorMessage(error));
    }
}
}

class RayeArchiveWorker : public KIO::WorkerBase
{
public:
    RayeArchiveWorker(const QByteArray &pool, const QByteArray &app)
        : KIO::WorkerBase(QByteArrayLiteral("rayearchive"), pool, app)
    {
    }

    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult listDir(const QUrl &url) override;
    KIO::WorkerResult get(const QUrl &url) override;
    KIO::WorkerResult put(const QUrl &url, int permissions, KIO::JobFlags flags) override;
    KIO::WorkerResult mkdir(const QUrl &url, int permissions) override;
    KIO::WorkerResult del(const QUrl &url, bool isFile) override;

private:
    // Splits the URL and reads the archive's tree (kept while the file
    // does not change).
    KIO::WorkerResult open(const QUrl &url, ArchiveLocation *location);
    KIO::WorkerResult writable(const QUrl &url, const ArchiveLocation &location);
    KIO::WorkerResult rewrite(const QUrl &url, const ArchiveLocation &location, const QList<Writer::Change> &add, const QStringList &remove, bool overwrite);
    KIO::AuthInfo authInfo() const;
    bool askPassword(const QString &errorText);

    ArchiveIndex m_index;
    QString m_indexPath;
    QDateTime m_indexTime;
    qint64 m_indexSize = -1;
    QString m_password;
};

KIO::WorkerResult RayeArchiveWorker::open(const QUrl &url, ArchiveLocation *location)
{
    *location = splitArchivePath(url.path(), isRegularFile);
    if (!location->isValid()) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
    const QFileInfo info(location->archivePath);
    if (location->archivePath == m_indexPath && info.lastModified() == m_indexTime && info.size() == m_indexSize) {
        return KIO::WorkerResult::pass();
    }
    m_indexPath.clear();
    m_password.clear();
    Reader reader(location->archivePath);
    QList<Entry> entries;
    if (!reader.list(&entries)) {
        return failWith(reader.error(), location->archivePath);
    }
    m_index.build(entries);
    m_indexPath = location->archivePath;
    m_indexTime = info.lastModified();
    m_indexSize = info.size();
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult RayeArchiveWorker::stat(const QUrl &url)
{
    ArchiveLocation location;
    if (const auto r = open(url, &location); !r.success()) {
        return r;
    }
    if (location.innerPath.isEmpty()) {
        statEntry(udsEntry(m_index, *m_index.find(QString()), QFileInfo(location.archivePath).fileName()));
        return KIO::WorkerResult::pass();
    }
    const Entry *entry = m_index.find(location.innerPath);
    if (!entry) {
        // inside a linked folder: show what the link leads to
        QString resolved;
        if (m_index.resolve(location.innerPath, &resolved)) {
            entry = m_index.find(resolved);
        }
    }
    if (!entry) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
    statEntry(udsEntry(m_index, *entry, location.innerPath.mid(location.innerPath.lastIndexOf(QLatin1Char('/')) + 1)));
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult RayeArchiveWorker::listDir(const QUrl &url)
{
    ArchiveLocation location;
    if (const auto r = open(url, &location); !r.success()) {
        return r;
    }
    QString folder;
    if (!m_index.resolve(location.innerPath, &folder)) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
    const Entry *entry = m_index.find(folder);
    if (!entry) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
    if (entry->type != Entry::Folder) {
        return KIO::WorkerResult::fail(KIO::ERR_IS_FILE, url.toDisplayString());
    }
    const QList<Entry> children = m_index.children(folder);
    totalSize(children.size() + 1);
    KIO::UDSEntry dot = udsEntry(m_index, *entry, QStringLiteral("."));
    listEntry(dot);
    for (const Entry &child : children) {
        listEntry(udsEntry(m_index, child, QString()));
    }
    return KIO::WorkerResult::pass();
}

KIO::AuthInfo RayeArchiveWorker::authInfo() const
{
    KIO::AuthInfo info;
    info.url = entryUrl(m_indexPath, QString());
    info.caption = WorkerText::passwordCaption();
    info.prompt = errorMessage(Error::PasswordNeeded);
    info.comment = QFileInfo(m_indexPath).fileName();
    info.commentLabel = WorkerText::passwordCommentLabel();
    info.username = QFileInfo(m_indexPath).fileName();
    info.readOnly = true;
    info.keepPassword = true;
    return info;
}

bool RayeArchiveWorker::askPassword(const QString &errorText)
{
    KIO::AuthInfo info = authInfo();
    if (errorText.isEmpty() && checkCachedAuthentication(info) && !info.password.isEmpty()) {
        m_password = info.password;
        return true;
    }
    if (openPasswordDialog(info, errorText) != 0 || info.password.isEmpty()) {
        return false;
    }
    m_password = info.password;
    return true;
}

KIO::WorkerResult RayeArchiveWorker::get(const QUrl &url)
{
    ArchiveLocation location;
    if (const auto r = open(url, &location); !r.success()) {
        return r;
    }
    QString path;
    const Entry *entry = nullptr;
    if (m_index.resolve(location.innerPath, &path)) {
        entry = m_index.find(path);
    }
    if (!entry) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
    if (entry->type == Entry::Folder) {
        return KIO::WorkerResult::fail(KIO::ERR_IS_DIRECTORY, url.toDisplayString());
    }

    const QString name = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
    mimeType(QMimeDatabase().mimeTypeForFile(name, QMimeDatabase::MatchExtension).name());
    if (entry->size >= 0) {
        totalSize(entry->size);
    }

    QString errorText;
    for (int attempt = 0; attempt < 4; ++attempt) {
        Reader reader(location.archivePath);
        reader.setPassword(m_password);
        KIO::filesize_t sent = 0;
        const bool ok = reader.read(path, [this, &sent](const char *data, qint64 size) {
            if (wasKilled()) {
                return false;
            }
            this->data(QByteArray::fromRawData(data, int(size)));
            sent += KIO::filesize_t(size);
            processedSize(sent);
            return true;
        });
        if (ok) {
            if (!m_password.isEmpty()) {
                // asked once per archive and session, like Windows
                KIO::AuthInfo info = authInfo();
                info.password = m_password;
                cacheAuthentication(info);
            }
            data(QByteArray());
            return KIO::WorkerResult::pass();
        }
        const Error error = reader.error();
        if ((error != Error::PasswordNeeded && error != Error::WrongPassword) || sent > 0) {
            return failWith(error, url.toDisplayString());
        }
        if (error == Error::WrongPassword) {
            errorText = errorMessage(Error::WrongPassword);
        }
        if (!askPassword(errorText)) {
            return KIO::WorkerResult::fail(KIO::ERR_USER_CANCELED, url.toDisplayString());
        }
    }
    return failWith(Error::WrongPassword, url.toDisplayString());
}

KIO::WorkerResult RayeArchiveWorker::writable(const QUrl &url, const ArchiveLocation &location)
{
    const QString mime = QMimeDatabase().mimeTypeForFile(location.archivePath).name();
    const QString found = detectFormat(location.archivePath);
    const bool known = found == QLatin1String("zip") || found == QLatin1String("7z") || found == QLatin1String("tar");
    if (!isWritableMimeType(mime) && !known) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, cannotWriteMessage(QFileInfo(location.archivePath).fileName()));
    }
    if (location.innerPath.isEmpty() || !m_index.canAddAt(location.innerPath)) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_WRITE, url.toDisplayString());
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult RayeArchiveWorker::rewrite(const QUrl &url,
                                             const ArchiveLocation &location,
                                             const QList<Writer::Change> &add,
                                             const QStringList &remove,
                                             bool overwrite)
{
    Writer writer;
    if (!writer.rewriteZip(location.archivePath, add, remove, overwrite)) {
        if (writer.error() == Error::Unsupported) {
            return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, WorkerText::cannotChangeProtected());
        }
        return failWith(writer.error(), url.toDisplayString());
    }
    // Read again next time.
    m_indexPath.clear();
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult RayeArchiveWorker::put(const QUrl &url, int permissions, KIO::JobFlags flags)
{
    Q_UNUSED(permissions);
    ArchiveLocation location;
    if (const auto r = open(url, &location); !r.success()) {
        return r;
    }
    if (const auto r = writable(url, location); !r.success()) {
        return r;
    }
    const bool overwrite = flags & KIO::Overwrite;
    if (const Entry *existing = m_index.find(location.innerPath)) {
        if (existing->type == Entry::Folder) {
            return KIO::WorkerResult::fail(KIO::ERR_DIR_ALREADY_EXIST, url.toDisplayString());
        }
        if (!overwrite) {
            return KIO::WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, url.toDisplayString());
        }
    }

    QTemporaryFile incoming;
    if (!incoming.open()) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_WRITE, url.toDisplayString());
    }
    while (true) {
        dataReq();
        QByteArray buffer;
        const int n = readData(buffer);
        if (n < 0) {
            return KIO::WorkerResult::fail(KIO::ERR_CANNOT_WRITE, url.toDisplayString());
        }
        if (n == 0) {
            break;
        }
        if (incoming.write(buffer) != buffer.size()) {
            return KIO::WorkerResult::fail(KIO::ERR_DISK_FULL, url.toDisplayString());
        }
    }
    if (!incoming.flush()) {
        return KIO::WorkerResult::fail(KIO::ERR_DISK_FULL, url.toDisplayString());
    }
    return rewrite(url, location, {{location.innerPath, incoming.fileName(), false}}, {}, overwrite);
}

KIO::WorkerResult RayeArchiveWorker::mkdir(const QUrl &url, int permissions)
{
    Q_UNUSED(permissions);
    ArchiveLocation location;
    if (const auto r = open(url, &location); !r.success()) {
        return r;
    }
    if (const auto r = writable(url, location); !r.success()) {
        return r;
    }
    if (const Entry *existing = m_index.find(location.innerPath)) {
        return KIO::WorkerResult::fail(existing->type == Entry::Folder ? KIO::ERR_DIR_ALREADY_EXIST : KIO::ERR_FILE_ALREADY_EXIST, url.toDisplayString());
    }
    return rewrite(url, location, {{location.innerPath, QString(), true}}, {}, false);
}

KIO::WorkerResult RayeArchiveWorker::del(const QUrl &url, bool isFile)
{
    Q_UNUSED(isFile);
    ArchiveLocation location;
    if (const auto r = open(url, &location); !r.success()) {
        return r;
    }
    if (const auto r = writable(url, location); !r.success()) {
        return r;
    }
    if (!m_index.find(location.innerPath)) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
    return rewrite(url, location, {}, {location.innerPath}, false);
}

// The plugin's metadata (protocol, what it can do) for KIO.
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.rayearchive" FILE "kio_rayearchive.json")
};

extern "C" Q_DECL_EXPORT int kdemain(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kio_rayearchive"));
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("raye-files"));
    if (argc != 4) {
        return 1;
    }
    RayeArchiveWorker worker(argv[2], argv[3]);
    worker.dispatchLoop();
    return 0;
}

#include "kio_rayearchive.moc"
