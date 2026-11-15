/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "locationnames.h"

#include "netdrive/networkdrives.h"

#include <KLocalizedString>
#include <KUser>

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrlQuery>

#include <algorithm>

namespace Raye::Locations
{
namespace
{
DriveLister &driveLister()
{
    static DriveLister lister;
    return lister;
}

// The KIO schemes Files shows under Network.
bool isNetworkScheme(const QString &scheme)
{
    static const QStringList schemes = {
        QStringLiteral("smb"),
        QStringLiteral("remote"),
        QStringLiteral("sftp"),
        QStringLiteral("fish"),
        QStringLiteral("ftp"),
        QStringLiteral("ftps"),
        QStringLiteral("webdav"),
        QStringLiteral("webdavs"),
        QStringLiteral("nfs"),
        QStringLiteral("afp"),
    };
    return schemes.contains(scheme);
}

// Archives Files opens as folders (explorer spec 3.12).
bool hasArchiveName(const QString &name)
{
    static const QRegularExpression re(
        QStringLiteral(R"(\.(zip|7z|rar|tar|tgz|tbz|tbz2|txz|tzst|iso|cab|jar|tar\.(gz|bz2|xz|zst|lz|lzma))$)"),
        QRegularExpression::CaseInsensitiveOption);
    return re.match(name).hasMatch();
}

// "/a/b" with "c" -> "/a/b/c"; "/" with "c" -> "/c".
QString joinPath(const QString &base, const QString &name)
{
    return base.endsWith(QLatin1Char('/')) ? QString(base + name) : QString(base + QLatin1Char('/') + name);
}

bool isInside(const QString &path, const QString &folder)
{
    if (folder.isEmpty()) {
        return false;
    }
    if (folder == QLatin1String("/")) {
        return path.startsWith(QLatin1Char('/'));
    }
    return path == folder || path.startsWith(folder + QLatin1Char('/'));
}

QUrl pathUrl(const QString &scheme, const QString &path)
{
    if (scheme == QLatin1String("file")) {
        return QUrl::fromLocalFile(path);
    }
    QUrl url;
    url.setScheme(scheme);
    url.setPath(path);
    return url;
}

QString cleanLocalPath(const QString &path)
{
    const QString clean = QDir::cleanPath(path);
    return clean.isEmpty() || clean == QLatin1String(".") ? QStringLiteral("/") : clean;
}

Crumb thisPcCrumb()
{
    return {thisPcUrl(), i18nc("@label the page with the drives and folders of the computer", "This PC"), QStringLiteral("computer"), Kind::ThisPc, false};
}

QString homePageName()
{
    return i18nc("@label Files' start page", "Home");
}
QString galleryName()
{
    return i18nc("@label the page with all pictures", "Gallery");
}
QString recycleBinName()
{
    return i18nc("@label the trash", "Recycle Bin");
}
QString networkName()
{
    return i18nc("@label computers and shared folders on the network", "Network");
}
QString recentName()
{
    return i18nc("@label recently used files", "Recent");
}

const QList<KnownFolder> knownFolderOrder = {
    KnownFolder::Desktop,
    KnownFolder::Documents,
    KnownFolder::Downloads,
    KnownFolder::Music,
    KnownFolder::Pictures,
    KnownFolder::Videos,
};

QString defaultDriveName(const QStorageInfo &volume, bool removable)
{
    const QString label = volume.name();
    if (!label.isEmpty()) {
        return label;
    }
    return removable ? i18nc("@label a USB stick or disk with no name", "USB Drive") : i18nc("@label a drive with no name", "Local Disk");
}

// Until the drive naming of This PC lands: "/" and what is mounted under
// /media, /run/media and /mnt, named by their label.
QList<Drive> mountedVolumes()
{
    QList<Drive> out;
    out.append({QStringLiteral("/"), LocationNames::systemDriveName(), QStringLiteral("drive-harddisk-system"), false});
    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &volume : volumes) {
        const QString root = volume.rootPath();
        const bool removable = root.startsWith(QLatin1String("/media/")) || root.startsWith(QLatin1String("/run/media/"));
        if (!volume.isValid() || !volume.isReady() || !(removable || root.startsWith(QLatin1String("/mnt/")))
            || NetDrives::isNetworkFileSystem(QString::fromLatin1(volume.fileSystemType()))) {
            continue;
        }
        out.append({root,
                    defaultDriveName(volume, removable),
                    removable ? QStringLiteral("drive-removable-media-usb") : QStringLiteral("drive-harddisk"),
                    removable});
    }
    for (const NetDrives::Mount &m : NetDrives::networkMounts()) {
        const NetDrives::Mapping mapping = NetDrives::mappingAt(m.mountPoint);
        const NetDrives::Address a = NetDrives::addressOfSource(m.source);
        QString name = mapping.name;
        if (name.isEmpty()) {
            name = a.share.isEmpty() ? QFileInfo(m.mountPoint).fileName() : a.share;
        }
        out.append({m.mountPoint, name, QStringLiteral("folder-network"), false, true});
    }
    return out;
}
}

QUrl homePageUrl()
{
    return QUrl(QStringLiteral("raye:home"));
}

QUrl galleryUrl()
{
    return QUrl(QStringLiteral("raye:gallery"));
}

QUrl thisPcUrl()
{
    return QUrl(QStringLiteral("raye:thispc"));
}

QUrl recycleBinUrl()
{
    return QUrl(QStringLiteral("trash:/"));
}

QUrl networkUrl()
{
    return QUrl(QStringLiteral("smb:/"));
}

void setDriveLister(const DriveLister &lister)
{
    driveLister() = lister;
}

QList<Drive> listDrives()
{
    return driveLister() ? driveLister()() : mountedVolumes();
}

Context Context::system()
{
    Context c;
    c.homePath = QDir::homePath();
    const KUser user;
    const QString fullName = user.property(KUser::FullName).toString().trimmed();
    c.userName = fullName.isEmpty() ? user.loginName() : fullName;
    const QList<std::pair<KnownFolder, QStandardPaths::StandardLocation>> locations = {
        {KnownFolder::Desktop, QStandardPaths::DesktopLocation},
        {KnownFolder::Documents, QStandardPaths::DocumentsLocation},
        {KnownFolder::Downloads, QStandardPaths::DownloadLocation},
        {KnownFolder::Music, QStandardPaths::MusicLocation},
        {KnownFolder::Pictures, QStandardPaths::PicturesLocation},
        {KnownFolder::Videos, QStandardPaths::MoviesLocation},
    };
    for (const auto &[folder, location] : locations) {
        const QString path = QDir::cleanPath(QStandardPaths::writableLocation(location));
        // An unset XDG folder points at the home itself: not a known folder
        // (nor is the root, which would swallow every path).
        if (!path.isEmpty() && path != QDir::cleanPath(c.homePath) && path != QLatin1String("/")) {
            c.knownFolders.insert(folder, path);
        }
    }
    c.drives = listDrives();
    // An archive is cut where the path stops being a folder.
    c.isFile = [](const QString &path) {
        return QFileInfo(path).isFile();
    };
    return c;
}

LocationNames::LocationNames(Context context)
    : m_context(std::move(context))
{
    m_context.homePath = m_context.homePath.isEmpty() ? QString() : QDir::cleanPath(m_context.homePath);
    for (auto it = m_context.knownFolders.begin(); it != m_context.knownFolders.end(); ++it) {
        it.value() = QDir::cleanPath(it.value());
    }
}

QString LocationNames::knownFolderName(KnownFolder folder)
{
    switch (folder) {
    case KnownFolder::Desktop:
        return i18nc("@label known folder", "Desktop");
    case KnownFolder::Documents:
        return i18nc("@label known folder", "Documents");
    case KnownFolder::Downloads:
        return i18nc("@label known folder", "Downloads");
    case KnownFolder::Music:
        return i18nc("@label known folder", "Music");
    case KnownFolder::Pictures:
        return i18nc("@label known folder", "Pictures");
    case KnownFolder::Videos:
        return i18nc("@label known folder", "Videos");
    }
    return QString();
}

QString LocationNames::knownFolderIcon(KnownFolder folder)
{
    switch (folder) {
    case KnownFolder::Desktop:
        return QStringLiteral("user-desktop");
    case KnownFolder::Documents:
        return QStringLiteral("folder-documents");
    case KnownFolder::Downloads:
        return QStringLiteral("folder-download");
    case KnownFolder::Music:
        return QStringLiteral("folder-music");
    case KnownFolder::Pictures:
        return QStringLiteral("folder-pictures");
    case KnownFolder::Videos:
        return QStringLiteral("folder-videos");
    }
    return QStringLiteral("folder");
}

QString LocationNames::systemDriveName()
{
    return i18nc("@label the drive Raye is installed on", "Raye");
}

QList<Drive> LocationNames::drives() const
{
    QList<Drive> out;
    bool hasRoot = false;
    for (Drive d : m_context.drives) {
        d.mountPoint = cleanLocalPath(d.mountPoint);
        hasRoot = hasRoot || d.mountPoint == QLatin1String("/");
        out.append(d);
    }
    if (!hasRoot) {
        out.prepend({QStringLiteral("/"), systemDriveName(), QStringLiteral("drive-harddisk-system"), false});
    }
    return out;
}

QList<Crumb> LocationNames::localCrumbs(const QString &rawPath, const QString &scheme) const
{
    const QString path = cleanLocalPath(rawPath);
    QList<Crumb> out{thisPcCrumb()};

    QString base;
    Crumb first;
    // A network drive first (it usually lives in the home, like Windows'
    // Z: it is shown as a drive of This PC); then the known folders,
    // wherever they are; then the home; then the drive holding the path.
    for (const Drive &d : drives()) {
        if (d.network && isInside(path, d.mountPoint) && d.mountPoint.length() > base.length()) {
            base = d.mountPoint;
            first = {pathUrl(scheme, base), d.name, d.iconName, Kind::Drive, true};
        }
    }
    const bool onNetworkDrive = !base.isEmpty();
    for (KnownFolder folder : knownFolderOrder) {
        if (onNetworkDrive) {
            break;
        }
        const QString k = m_context.knownFolders.value(folder);
        if (isInside(path, k) && k.length() > base.length()) {
            base = k;
            first = {pathUrl(scheme, k), knownFolderName(folder), knownFolderIcon(folder), Kind::KnownFolder, true};
        }
    }
    if (base.isEmpty() && isInside(path, m_context.homePath)) {
        base = m_context.homePath;
        first = {pathUrl(scheme, base), m_context.userName, QStringLiteral("user-home"), Kind::UserFolder, true};
    }
    if (base.isEmpty()) {
        for (const Drive &d : drives()) {
            if (isInside(path, d.mountPoint) && (base.isEmpty() || d.mountPoint.length() > base.length())) {
                base = d.mountPoint;
                first = {pathUrl(scheme, base), d.name, d.iconName, Kind::Drive, true};
            }
        }
    }
    out.append(first);

    QString current = base;
    const QStringList rest = path.mid(base.length()).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &name : rest) {
        current = joinPath(current, name);
        out.append({pathUrl(scheme, current), name, QStringLiteral("folder"), Kind::Folder, true});
    }
    return out;
}

QList<Crumb> LocationNames::archiveCrumbs(const QUrl &url) const
{
    const QString path = cleanLocalPath(url.path());
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString prefix;
    int archiveAt = -1;
    for (int i = 0; i < parts.size(); ++i) {
        prefix = joinPath(prefix.isEmpty() ? QStringLiteral("/") : prefix, parts.at(i));
        const bool isArchive = m_context.isFile ? m_context.isFile(prefix) : hasArchiveName(parts.at(i));
        if (isArchive) {
            archiveAt = i;
            break;
        }
    }
    if (archiveAt < 0) {
        return localCrumbs(path, QStringLiteral("file"));
    }

    QList<Crumb> out = localCrumbs(QFileInfo(prefix).path(), QStringLiteral("file"));
    const QString scheme = url.scheme();
    out.append({pathUrl(scheme, prefix), parts.at(archiveAt), QStringLiteral("package-x-generic"), Kind::Archive, true});
    QString current = prefix;
    for (int i = archiveAt + 1; i < parts.size(); ++i) {
        current = joinPath(current, parts.at(i));
        out.append({pathUrl(scheme, current), parts.at(i), QStringLiteral("folder"), Kind::Folder, true});
    }
    return out;
}

QList<Crumb> LocationNames::trashCrumbs(const QUrl &url) const
{
    QList<Crumb> out{{recycleBinUrl(), recycleBinName(), QStringLiteral("user-trash"), Kind::RecycleBin, true}};
    const QStringList parts = QDir::cleanPath(url.path()).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    // The first part is "<trash id>-<name>": the user knows only the name.
    static const QRegularExpression trashId(QStringLiteral("^\\d+-"));
    QString current = QStringLiteral("/");
    for (int i = 0; i < parts.size(); ++i) {
        current = joinPath(current, parts.at(i));
        QString name = parts.at(i);
        if (i == 0) {
            name.remove(trashId);
        }
        out.append({pathUrl(QStringLiteral("trash"), current), name, QStringLiteral("folder"), Kind::Folder, true});
    }
    return out;
}

QList<Crumb> LocationNames::networkCrumbs(const QUrl &url) const
{
    QList<Crumb> out{{networkUrl(), networkName(), QStringLiteral("network-workgroup"), Kind::Network, false}};
    if (url.scheme() == QLatin1String("remote") || url.host().isEmpty()) {
        return out;
    }
    QUrl hostUrl;
    hostUrl.setScheme(url.scheme());
    hostUrl.setUserName(url.userName());
    hostUrl.setHost(url.host());
    hostUrl.setPort(url.port());
    out.append({hostUrl, url.host(), QStringLiteral("network-server"), Kind::Computer, true});

    const bool smb = url.scheme() == QLatin1String("smb");
    const QStringList parts = QDir::cleanPath(url.path()).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString current = QStringLiteral("/");
    for (int i = 0; i < parts.size(); ++i) {
        current = joinPath(current, parts.at(i));
        QUrl u = hostUrl;
        u.setPath(current);
        const bool share = smb && i == 0;
        out.append({u, parts.at(i), share ? QStringLiteral("folder-remote") : QStringLiteral("folder"), share ? Kind::Share : Kind::Folder, true});
    }
    return out;
}

QList<Crumb> LocationNames::phoneCrumbs(const QUrl &url) const
{
    QList<Crumb> out{thisPcCrumb()};
    const QStringList parts = QDir::cleanPath(url.path()).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        out.append({pathUrl(url.scheme(), QStringLiteral("/")),
                    i18nc("@label phones and cameras plugged into the computer", "Phones and cameras"),
                    QStringLiteral("smartphone"),
                    Kind::Phones,
                    false});
        return out;
    }
    QString current = QStringLiteral("/");
    for (int i = 0; i < parts.size(); ++i) {
        current = joinPath(current, parts.at(i));
        const bool device = i == 0;
        out.append({pathUrl(url.scheme(), current),
                    parts.at(i),
                    device ? QStringLiteral("smartphone") : QStringLiteral("folder"),
                    device ? Kind::Phone : Kind::Folder,
                    true});
    }
    return out;
}

QList<Crumb> LocationNames::searchCrumbs(const QUrl &url) const
{
    const QUrl folder(QUrlQuery(url).queryItemValue(QStringLiteral("url"), QUrl::FullyDecoded));
    QList<Crumb> out;
    QString text = i18nc("@label the results of a search", "Search results");
    if (folder.isValid() && !folder.isEmpty() && folder.scheme() != url.scheme()) {
        out = crumbs(folder);
        if (!out.isEmpty()) {
            text = i18nc("@label %1 is the folder searched", "Search results in %1", out.last().text);
        }
    }
    out.append({url, text, QStringLiteral("edit-find"), Kind::Search, false});
    return out;
}

QList<Crumb> LocationNames::crumbs(const QUrl &url) const
{
    const QString scheme = url.scheme();
    if (scheme.isEmpty() || url.isLocalFile()) {
        return localCrumbs(url.isLocalFile() ? url.toLocalFile() : url.path(), QStringLiteral("file"));
    }
    if (scheme == QLatin1String("admin")) {
        return localCrumbs(url.path(), scheme);
    }
    if (scheme == QStringView(pageScheme)) {
        const QString page = url.path();
        if (page == QLatin1String("gallery")) {
            return {{galleryUrl(), galleryName(), QStringLiteral("folder-pictures"), Kind::Gallery, false}};
        }
        if (page == QLatin1String("thispc")) {
            return {thisPcCrumb()};
        }
        return {{homePageUrl(), homePageName(), QStringLiteral("go-home"), Kind::HomePage, false}};
    }
    if (scheme == QLatin1String("trash")) {
        return trashCrumbs(url);
    }
    if (scheme == QLatin1String("rayearchive")) {
        return archiveCrumbs(url);
    }
    if (scheme == QLatin1String("mtp") || scheme == QLatin1String("afc")) {
        return phoneCrumbs(url);
    }
    if (scheme == QLatin1String("desktop")) {
        const QString desktop = m_context.knownFolders.value(KnownFolder::Desktop);
        if (!desktop.isEmpty()) {
            return localCrumbs(desktop + QLatin1Char('/') + url.path(), QStringLiteral("file"));
        }
    }
    if (scheme == QLatin1String("recentlyused")) {
        return {{url, recentName(), QStringLiteral("document-open-recent"), Kind::Recent, false}};
    }
    if (scheme == QLatin1String("filenamesearch") || scheme == QLatin1String("baloosearch")) {
        return searchCrumbs(url);
    }
    if (isNetworkScheme(scheme) || !url.host().isEmpty()) {
        return networkCrumbs(url);
    }

    // Anything else: the scheme as its own root, then the path.
    QList<Crumb> out{{pathUrl(scheme, QStringLiteral("/")), scheme, QStringLiteral("folder"), Kind::Folder, true}};
    QString current = QStringLiteral("/");
    for (const QString &name : QDir::cleanPath(url.path()).split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        current = joinPath(current, name);
        out.append({pathUrl(scheme, current), name, QStringLiteral("folder"), Kind::Folder, true});
    }
    return out;
}

QString LocationNames::displayName(const QUrl &url) const
{
    const QList<Crumb> c = crumbs(url);
    return c.isEmpty() ? QString() : c.last().text;
}

QString LocationNames::iconName(const QUrl &url) const
{
    const QList<Crumb> c = crumbs(url);
    return c.isEmpty() ? QStringLiteral("folder") : c.last().iconName;
}

Kind LocationNames::kind(const QUrl &url) const
{
    const QList<Crumb> c = crumbs(url);
    return c.isEmpty() ? Kind::Folder : c.last().kind;
}

QUrl LocationNames::upUrl(const QUrl &url) const
{
    const QList<Crumb> c = crumbs(url);
    return c.size() < 2 ? QUrl() : c.at(c.size() - 2).url;
}

QList<Crumb> LocationNames::pageChildren(const QUrl &url) const
{
    QList<Crumb> out;
    if (url.scheme() != QStringView(pageScheme) || url.path() != QLatin1String("thispc")) {
        return out;
    }
    for (KnownFolder folder : knownFolderOrder) {
        const QString path = m_context.knownFolders.value(folder);
        if (!path.isEmpty()) {
            out.append({QUrl::fromLocalFile(path), knownFolderName(folder), knownFolderIcon(folder), Kind::KnownFolder, true});
        }
    }
    for (const Drive &d : drives()) {
        out.append({QUrl::fromLocalFile(d.mountPoint), d.name, d.iconName, Kind::Drive, true});
    }
    return out;
}

QString LocationNames::editText(const QUrl &url) const
{
    if (url.isLocalFile()) {
        return cleanLocalPath(url.toLocalFile());
    }
    if (url.scheme() == QLatin1String("rayearchive")) {
        return cleanLocalPath(url.path());
    }
    const QList<Crumb> c = crumbs(url);
    if (c.size() == 1 && !c.first().isFolder) {
        return c.first().text; // a page: "This PC", "Network"...
    }
    if (url.scheme() == QLatin1String("trash") && c.size() == 1) {
        return c.first().text;
    }
    return url.toDisplayString(QUrl::StripTrailingSlash);
}

QUrl LocationNames::fromEditText(const QString &typed, const QUrl &current) const
{
    const QString text = typed.trimmed();
    if (text.isEmpty()) {
        return QUrl();
    }

    // Page names, in English and in the user's language.
    const QList<std::tuple<const char *, QString, QUrl>> pages = {
        {"Home", homePageName(), homePageUrl()},
        {"Gallery", galleryName(), galleryUrl()},
        {"This PC", thisPcCrumb().text, thisPcUrl()},
        {"Recycle Bin", recycleBinName(), recycleBinUrl()},
        {"Network", networkName(), networkUrl()},
        {"Recent", recentName(), QUrl(QStringLiteral("recentlyused:/"))},
    };
    for (const auto &[english, translated, url] : pages) {
        if (text.compare(QLatin1String(english), Qt::CaseInsensitive) == 0 || text.compare(translated, Qt::CaseInsensitive) == 0) {
            return url;
        }
    }

    auto localUrl = [this](const QString &path) {
        const QString clean = cleanLocalPath(path);
        // A path through an archive opens the archive as a folder.
        QString prefix;
        const QStringList parts = clean.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            prefix = joinPath(prefix.isEmpty() ? QStringLiteral("/") : prefix, part);
            if (m_context.isFile ? m_context.isFile(prefix) : hasArchiveName(part)) {
                return pathUrl(QStringLiteral("rayearchive"), clean);
            }
        }
        return QUrl::fromLocalFile(clean);
    };

    // \\server\share, as Windows writes it.
    if (text.startsWith(QLatin1String("\\\\"))) {
        QString rest = text.mid(2);
        rest.replace(QLatin1Char('\\'), QLatin1Char('/'));
        return QUrl(QStringLiteral("smb://") + rest);
    }
    if (text == QLatin1String("~") || text.startsWith(QLatin1String("~/"))) {
        return localUrl(m_context.homePath + text.mid(1));
    }
    if (text.startsWith(QLatin1Char('/'))) {
        return localUrl(text);
    }
    static const QRegularExpression schemeRe(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+.-]*:"));
    if (schemeRe.match(text).hasMatch()) {
        const QUrl url(text);
        return url.isValid() ? url : QUrl();
    }

    // A name relative to the current folder; Files' pages have none.
    if (!current.isValid() || current.isEmpty() || current.scheme() == QStringView(pageScheme)) {
        return QUrl();
    }
    if (current.isLocalFile()) {
        return localUrl(joinPath(current.toLocalFile(), text));
    }
    QUrl url = current;
    url.setPath(QDir::cleanPath(joinPath(current.path().isEmpty() ? QStringLiteral("/") : current.path(), text)));
    return url;
}

}
