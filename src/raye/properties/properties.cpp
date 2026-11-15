/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "properties.h"

#include "locations/locationnames.h"
#include "netdrive/networkdrives.h"

#include <KApplicationTrader>
#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/PropertyInfo>
#include <KFileMetaData/SimpleExtractionResult>
#include <KFormat>
#include <KLocalizedString>
#include <KUser>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QLocale>
#include <QMimeDatabase>
#include <QProcess>
#include <QStorageInfo>
#include <QtConcurrentRun>

#include <sys/stat.h>
#include <unistd.h>

namespace Raye::Properties
{
namespace
{
int shift(Who who)
{
    return who == Who::Owner ? 6 : who == Who::Group ? 3 : 0;
}

QString byteSize(qint64 bytes)
{
    if (bytes < 1024) {
        return i18ncp("@info file size", "%1 byte", "%1 bytes", int(qMax<qint64>(0, bytes)));
    }
    const int precision = bytes >= qint64(10) * 1024 * 1024 * 1024 ? 0 : 1;
    return KFormat().formatByteSize(double(qMax<qint64>(0, bytes)), precision, KFormat::JEDECBinaryDialect);
}

QString hiddenListPath(const QString &path)
{
    return QFileInfo(path).absolutePath() + QStringLiteral("/.hidden");
}

QStringList readHiddenList(const QString &file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    QStringList names;
    const QList<QByteArray> lines = f.readAll().split('\n');
    for (const QByteArray &line : lines) {
        const QString name = QString::fromUtf8(line).trimmed();
        if (!name.isEmpty()) {
            names << name;
        }
    }
    return names;
}

bool measureOne(const QString &path, Totals &t)
{
    struct stat st;
    if (::lstat(QFile::encodeName(path).constData(), &st) != 0) {
        return false;
    }
    t.size += st.st_size;
    t.onDisk += qint64(st.st_blocks) * 512;
    if (S_ISDIR(st.st_mode)) {
        ++t.folders;
    } else {
        ++t.files;
    }
    return S_ISDIR(st.st_mode);
}

QString userName(uid_t uid)
{
    if (uid == ::getuid()) {
        return i18nc("@label owner of a file: the person using the computer", "You");
    }
    if (uid == 0) {
        return i18nc("@label owner of a file: the system", "System");
    }
    const KUser user{K_UID(uid)};
    const QString full = user.property(KUser::FullName).toString().trimmed();
    return !full.isEmpty() ? full : user.isValid() ? user.loginName() : QString::number(uid);
}
}

Access accessFor(mode_t mode, Who who)
{
    const mode_t bits = (mode >> shift(who)) & 07;
    if ((bits & 06) == 06) {
        return Access::ReadWrite;
    }
    return (bits & 04) ? Access::Read : Access::None;
}

mode_t withAccess(mode_t mode, Who who, Access access, bool folder)
{
    const int s = shift(who);
    const mode_t keepX = folder ? 0 : (mode >> s) & 01;
    mode_t bits = 0;
    if (access == Access::Read) {
        bits = folder ? 05 : 04 | keepX;
    } else if (access == Access::ReadWrite) {
        bits = folder ? 07 : 06 | keepX;
    }
    return (mode & ~(mode_t(07) << s)) | (bits << s);
}

Totals measure(const QStringList &paths)
{
    Totals t;
    for (const QString &path : paths) {
        if (!measureOne(path, t)) {
            continue;
        }
        QDirIterator it(path, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            measureOne(it.next(), t);
        }
    }
    return t;
}

QString sizeText(qint64 bytes)
{
    return i18ncp("@info size of a file, then its exact size", "%2 (%3 byte)", "%2 (%3 bytes)", bytes, byteSize(bytes), QLocale().toString(bytes));
}

QString containsText(int files, int folders)
{
    return i18nc("@info what a folder holds, e.g. 12 files, 3 folders",
                 "%1, %2",
                 i18ncp("@info", "%1 file", "%1 files", files),
                 i18ncp("@info", "%1 folder", "%1 folders", folders));
}

QString fileSystemName(const QString &type)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("btrfs"), QStringLiteral("Btrfs")},
        {QStringLiteral("vfat"), QStringLiteral("FAT32")},
        {QStringLiteral("msdos"), QStringLiteral("FAT")},
        {QStringLiteral("exfat"), QStringLiteral("exFAT")},
        {QStringLiteral("ntfs"), QStringLiteral("NTFS")},
        {QStringLiteral("ntfs3"), QStringLiteral("NTFS")},
        {QStringLiteral("fuseblk"), QStringLiteral("NTFS")},
        {QStringLiteral("iso9660"), QStringLiteral("CDFS")},
        {QStringLiteral("udf"), QStringLiteral("UDF")},
        {QStringLiteral("xfs"), QStringLiteral("XFS")},
    };
    return names.value(type.toLower(), type);
}

QString dateText(const QDateTime &date)
{
    if (!date.isValid()) {
        return QString();
    }
    const QLocale locale;
    return i18nc("@info a date then a time", "%1, %2", locale.toString(date.date(), QLocale::LongFormat), locale.toString(date.time(), QLocale::ShortFormat));
}

bool isHidden(const QString &path)
{
    const QString name = QFileInfo(path).fileName();
    return name.startsWith(QLatin1Char('.')) || readHiddenList(hiddenListPath(path)).contains(name);
}

bool setHidden(const QString &path, bool hidden)
{
    const QString name = QFileInfo(path).fileName();
    if (name.startsWith(QLatin1Char('.'))) {
        return hidden;
    }
    const QString file = hiddenListPath(path);
    QStringList names = readHiddenList(file);
    if (names.contains(name) == hidden) {
        return true;
    }
    if (hidden) {
        names << name;
    } else {
        names.removeAll(name);
    }
    if (names.isEmpty()) {
        return QFile::remove(file);
    }
    QFile f(file);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(names.join(QLatin1Char('\n')).toUtf8() + '\n');
    return true;
}

QVariantList mediaDetails(const QString &path)
{
    QVariantList out;
    const auto add = [&out](const QString &label, const QString &value) {
        if (!value.isEmpty()) {
            out.append(QVariantMap{{QStringLiteral("label"), label}, {QStringLiteral("value"), value}});
        }
    };
    const QString dimensions = i18nc("@label size of a picture or video in pixels", "Dimensions");
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(path);
    const bool media = mime.name().startsWith(QLatin1String("image/")) || mime.name().startsWith(QLatin1String("video/"))
        || mime.name().startsWith(QLatin1String("audio/"));
    if (!media) {
        return out;
    }
    if (mime.name().startsWith(QLatin1String("image/"))) {
        const QSize size = QImageReader(path).size();
        if (size.isValid()) {
            add(dimensions, i18nc("@info width x height", "%1 x %2", QString::number(size.width()), QString::number(size.height())));
        }
    }

    KFileMetaData::ExtractorCollection extractors;
    KFileMetaData::SimpleExtractionResult result(path, mime.name(), KFileMetaData::ExtractionResult::ExtractMetaData);
    const QList<KFileMetaData::Extractor *> list = extractors.fetchExtractors(mime.name());
    for (KFileMetaData::Extractor *e : list) {
        e->extract(&result);
    }
    const auto props = result.properties();
    const QVariant width = props.value(KFileMetaData::Property::Width);
    const QVariant height = props.value(KFileMetaData::Property::Height);
    if (width.isValid() && height.isValid() && out.isEmpty()) {
        add(dimensions, i18nc("@info width x height", "%1 x %2", QString::number(width.toInt()), QString::number(height.toInt())));
    }
    for (auto it = props.constKeyValueBegin(); it != props.constKeyValueEnd(); ++it) {
        if (it->first == KFileMetaData::Property::Width || it->first == KFileMetaData::Property::Height) {
            continue;
        }
        const KFileMetaData::PropertyInfo info(it->first);
        add(info.displayName(), info.formatAsDisplayString(it->second));
    }
    return out;
}

bool PropertiesModel::handles(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return false;
    }
    if (urls.size() == 1 && !urls.first().isLocalFile()) {
        return !NetDrives::protocolName(urls.first().scheme()).isEmpty() && !urls.first().host().isEmpty();
    }
    for (const QUrl &url : urls) {
        if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).exists()) {
            return false;
        }
    }
    return true;
}

PropertiesModel::PropertiesModel(const QList<QUrl> &urls, QObject *parent, const QString &driveName, const SpaceReader &space)
    : QObject(parent)
{
    if (urls.size() == 1 && !urls.first().isLocalFile()) {
        readRemoteUrl(urls.first());
        m_originalName = m_name;
        return;
    }
    for (const QUrl &url : urls) {
        m_paths << QDir::cleanPath(url.toLocalFile());
    }
    if (m_paths.isEmpty()) {
        return;
    }
    const QMimeDatabase db;
    const QString first = m_paths.first();
    const QFileInfo info(first);
    struct stat st = {};
    ::lstat(QFile::encodeName(first).constData(), &st);

    if (m_paths.size() == 1) {
        m_folder = info.isDir();
        // A drive: "/" or the root of what is mounted.
        QString drive = driveName;
        const NetDrives::Mount share = m_folder ? NetDrives::mountAt(first, NetDrives::networkMounts()) : NetDrives::Mount();
        if (!share.type.isEmpty()) {
            readNetworkMount(first, share.type, share.source, space);
            m_originalName = m_name;
            return;
        }
        if (drive.isEmpty() && m_folder) {
            const QStorageInfo volume(first);
            if (volume.isValid() && QDir::cleanPath(volume.rootPath()) == first) {
                for (const Locations::Drive &d : Locations::listDrives()) {
                    if (QDir::cleanPath(d.mountPoint) == first) {
                        drive = d.name;
                    }
                }
                if (drive.isEmpty() && first == QLatin1String("/")) {
                    drive = Locations::LocationNames::systemDriveName();
                }
            }
        }
        m_drive = !drive.isEmpty();
        m_name = m_drive ? drive : info.fileName();
        const QMimeType mime = db.mimeTypeForFile(first);
        if (m_drive) {
            m_iconName = first == QLatin1String("/") ? QStringLiteral("drive-harddisk-system") : QStringLiteral("drive-harddisk");
            m_typeText = i18nc("@info type of a drive", "Local Disk");
            const Space s = space ? space(first) : [&first] {
                const QStorageInfo v(first);
                return Space{v.bytesTotal(), v.bytesAvailable(), QString::fromLatin1(v.fileSystemType())};
            }();
            m_fileSystem = fileSystemName(s.fileSystemType);
            m_usedText = Raye::Properties::sizeText(s.total - s.free);
            m_freeText = Raye::Properties::sizeText(s.free);
            m_capacityText = Raye::Properties::sizeText(s.total);
            m_usage = s.total > 0 ? qreal(s.total - s.free) / qreal(s.total) : 0;
        } else if (m_folder) {
            m_iconName = QStringLiteral("folder");
            m_typeText = i18nc("@info type of a folder", "File folder");
        } else {
            m_iconName = mime.iconName();
            const QString suffix = info.suffix();
            m_typeText = suffix.isEmpty() ? mime.comment() : i18nc("@info file type, then its extension", "%1 (.%2)", mime.comment(), suffix);
            if (const KService::Ptr app = KApplicationTrader::preferredService(mime.name())) {
                m_openWithName = app->name();
                m_openWithIcon = app->icon();
            }
        }
        m_location = info.absolutePath();
        m_created = dateText(info.birthTime());
        m_modified = dateText(info.lastModified());
        m_accessed = dateText(info.lastRead());
        if (!m_folder && !m_drive) {
            m_details = mediaDetails(first);
        }
    } else {
        m_name = i18ncp("@info several files chosen", "%1 item", "%1 items", m_paths.size());
        m_iconName = QStringLiteral("document-multiple");
        m_typeText = i18nc("@info type of several items", "Several types");
        m_location = info.absolutePath();
        for (const QString &p : std::as_const(m_paths)) {
            if (QFileInfo(p).absolutePath() != m_location) {
                m_location = i18nc("@info items from different folders", "Several locations");
                break;
            }
        }
    }
    m_originalName = m_name;

    m_readOnly = true;
    for (const QString &p : std::as_const(m_paths)) {
        m_readOnly = m_readOnly && !(QFileInfo(p).permissions() & QFileDevice::WriteOwner);
    }
    m_originalReadOnly = m_readOnly;
    m_hidden = !m_drive && isHidden(first);
    m_originalHidden = m_hidden;
    m_canHide = !m_drive && !info.fileName().startsWith(QLatin1Char('.')) && QFileInfo(info.absolutePath()).isWritable();
    m_canChange = !m_drive && (st.st_uid == ::getuid() || ::getuid() == 0);
    m_ownerName = userName(st.st_uid);
    const KUserGroup group{K_GID(st.st_gid)};
    m_groupName = group.isValid() ? group.name() : QString::number(st.st_gid);
    for (int i = 0; i < 3; ++i) {
        m_access[i] = m_originalAccess[i] = int(accessFor(st.st_mode, Who(i)));
    }

    if (m_drive) {
        return;
    }
    if (!m_folder && m_paths.size() == 1) {
        m_totals = measure(m_paths);
        return;
    }
    m_measuring = true;
    auto *watcher = new QFutureWatcher<Totals>(this);
    connect(watcher, &QFutureWatcher<Totals>::finished, this, [this, watcher] {
        m_totals = watcher->result();
        if (m_folder && m_paths.size() == 1) {
            --m_totals.folders; // the folder itself
        }
        m_measuring = false;
        watcher->deleteLater();
        Q_EMIT measured();
    });
    watcher->setFuture(QtConcurrent::run(&measure, m_paths));
}

void PropertiesModel::readNetworkMount(const QString &path, const QString &type, const QString &source, const SpaceReader &space)
{
    using namespace Raye::NetDrives;
    const Address a = addressOfSource(source);
    const Mapping mapping = mappingAt(path);
    m_drive = true;
    m_network = true;
    m_folder = true;
    m_name = !mapping.name.isEmpty() ? mapping.name : QFileInfo(path).fileName();
    m_iconName = QStringLiteral("folder-network");
    m_typeText = i18nc("@info type of a shared folder connected as a drive", "Network drive");
    m_protocol = protocolName(type);
    const bool smb = m_protocol == QLatin1String("SMB");
    m_server = smb ? QStringLiteral("\\\\") + a.server : a.server;
    m_networkFolder = smb && a.isValid() ? uncText(a) : source;
    m_mountedAt = path;
    m_location = QFileInfo(path).absolutePath();
    if (!mapping.mountPoint.isEmpty()) {
        m_connectedAs = mapping.user.isEmpty() ? i18nc("@info connected without a user name", "Guest")
                                               : (mapping.domain.isEmpty() ? mapping.user : mapping.domain + QLatin1Char('\\') + mapping.user);
    }
    const Space s = space ? space(path) : [&path] {
        const QStorageInfo v(path);
        return Space{v.bytesTotal(), v.bytesAvailable(), QString::fromLatin1(v.fileSystemType())};
    }();
    if (s.total > 0) {
        m_usedText = Raye::Properties::sizeText(s.total - s.free);
        m_freeText = Raye::Properties::sizeText(s.free);
        m_capacityText = Raye::Properties::sizeText(s.total);
        m_usage = qreal(s.total - s.free) / qreal(s.total);
    }
}

void PropertiesModel::readRemoteUrl(const QUrl &url)
{
    using namespace Raye::NetDrives;
    m_drive = true;
    m_network = true;
    m_folder = true;
    m_protocol = protocolName(url.scheme());
    const QStringList parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    m_name = parts.isEmpty() ? url.host() : parts.last();
    m_iconName = parts.isEmpty() ? QStringLiteral("network-server") : QStringLiteral("folder-network");
    m_typeText = parts.isEmpty() ? i18nc("@info type of a computer seen on the network", "Computer on the network")
                                 : i18nc("@info type of a shared folder not connected as a drive", "Network location");
    const bool smb = m_protocol == QLatin1String("SMB");
    m_server = smb ? QStringLiteral("\\\\") + url.host() : url.host();
    if (!parts.isEmpty()) {
        m_networkFolder = smb ? uncText({url.host(), parts.first(), parts.mid(1).join(QLatin1Char('/'))}) : url.toDisplayString(QUrl::RemovePassword);
    }
    m_connectedAs = url.userName();
}

void PropertiesModel::markDirty()
{
    m_dirty = m_name != m_originalName || m_readOnly != m_originalReadOnly || m_hidden != m_originalHidden || m_access[0] != m_originalAccess[0]
        || m_access[1] != m_originalAccess[1] || m_access[2] != m_originalAccess[2];
    Q_EMIT changed();
}

bool PropertiesModel::canRename() const
{
    return m_paths.size() == 1 && !m_drive && QFileInfo(QFileInfo(m_paths.first()).absolutePath()).isWritable();
}

void PropertiesModel::setName(const QString &name)
{
    if (name != m_name && canRename()) {
        m_name = name;
        markDirty();
    }
}

void PropertiesModel::setReadOnly(bool readOnly)
{
    if (readOnly != m_readOnly) {
        m_readOnly = readOnly;
        // Read-only is the owner without write: keep the Security tab in step.
        if (readOnly && m_access[0] == int(Access::ReadWrite)) {
            m_access[0] = int(Access::Read);
        } else if (!readOnly && m_access[0] != int(Access::ReadWrite)) {
            m_access[0] = int(Access::ReadWrite);
        }
        markDirty();
    }
}

void PropertiesModel::setHidden(bool hidden)
{
    if (hidden != m_hidden && m_canHide) {
        m_hidden = hidden;
        markDirty();
    }
}

void PropertiesModel::setAccess(int who, int access)
{
    if (who < 0 || who > 2 || access < 0 || access > 2 || m_access[who] == access) {
        return;
    }
    m_access[who] = access;
    if (who == 0) {
        m_readOnly = access != int(Access::ReadWrite);
    }
    markDirty();
}

void PropertiesModel::setOpenWith(const QString &name, const QString &icon)
{
    m_openWithName = name;
    m_openWithIcon = icon;
    Q_EMIT changed();
}

bool PropertiesModel::apply()
{
    m_error.clear();
    const bool accessChanged = m_access[0] != m_originalAccess[0] || m_access[1] != m_originalAccess[1] || m_access[2] != m_originalAccess[2];
    if (accessChanged || m_readOnly != m_originalReadOnly) {
        for (const QString &p : std::as_const(m_paths)) {
            struct stat st;
            if (::stat(QFile::encodeName(p).constData(), &st) != 0) {
                continue;
            }
            mode_t mode = st.st_mode & 07777;
            const bool dir = S_ISDIR(st.st_mode);
            if (accessChanged) {
                for (int i = 0; i < 3; ++i) {
                    if (m_paths.size() == 1 || m_access[i] != m_originalAccess[i]) {
                        mode = withAccess(mode, Who(i), Access(m_access[i]), dir);
                    }
                }
            } else {
                mode = m_readOnly ? mode & ~mode_t(S_IWUSR) : mode | S_IWUSR;
            }
            if (::chmod(QFile::encodeName(p).constData(), mode) != 0) {
                m_error = i18nc("@info", "You can't change who can open \"%1\".", QFileInfo(p).fileName());
                Q_EMIT changed();
                return false;
            }
        }
        m_originalReadOnly = m_readOnly;
        std::copy(std::begin(m_access), std::end(m_access), std::begin(m_originalAccess));
    }
    if (m_name != m_originalName && m_paths.size() == 1) {
        const QString name = m_name.trimmed();
        if (name.isEmpty() || name.contains(QLatin1Char('/')) || name == QLatin1String(".") || name == QLatin1String("..")) {
            m_error = i18nc("@info", "A file name can't be empty or contain /.");
            Q_EMIT changed();
            return false;
        }
        const QString target = QFileInfo(m_paths.first()).absolutePath() + QLatin1Char('/') + name;
        if (QFileInfo::exists(target)) {
            m_error = i18nc("@info", "There is already a file named \"%1\" here.", name);
            Q_EMIT changed();
            return false;
        }
        const bool wasHidden = m_originalHidden && !QFileInfo(m_paths.first()).fileName().startsWith(QLatin1Char('.'));
        if (wasHidden) {
            Raye::Properties::setHidden(m_paths.first(), false);
        }
        if (!QFile::rename(m_paths.first(), target)) {
            m_error = i18nc("@info", "\"%1\" could not be renamed.", m_originalName);
            Q_EMIT changed();
            return false;
        }
        if (wasHidden) {
            Raye::Properties::setHidden(target, true);
        }
        m_paths[0] = target;
        m_originalName = name;
        m_name = name;
    }
    if (m_hidden != m_originalHidden) {
        for (const QString &p : std::as_const(m_paths)) {
            if (!Raye::Properties::setHidden(p, m_hidden)) {
                m_error = i18nc("@info", "\"%1\" could not be hidden.", QFileInfo(p).fileName());
                Q_EMIT changed();
                return false;
            }
        }
        m_originalHidden = m_hidden;
    }
    markDirty();
    return true;
}

QString PropertiesModel::sizeText() const
{
    return m_measuring ? QString() : Raye::Properties::sizeText(m_totals.size);
}

QString PropertiesModel::onDiskText() const
{
    return m_measuring ? QString() : Raye::Properties::sizeText(m_totals.onDisk);
}

QString PropertiesModel::containsText() const
{
    if (m_measuring || (!m_folder && m_paths.size() < 2)) {
        return QString();
    }
    return Raye::Properties::containsText(m_totals.files, m_totals.folders);
}

void PropertiesModel::changeApp()
{
    Q_EMIT changeAppRequested();
}

void PropertiesModel::cleanUp()
{
    QProcess::startDetached(QStringLiteral("raye-settings"), {QStringLiteral("--page"), QStringLiteral("system/storage")});
}

void PropertiesModel::advanced()
{
    Q_EMIT advancedRequested();
}

}
