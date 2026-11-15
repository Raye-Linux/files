/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "thispcmodel.h"

#include "menu/menuplan.h"
#include "netdrive/networkdrives.h"
#include "pages/homemodel.h"

#include <KIO/ApplicationLauncherJob>
#include <KIO/ListJob>
#include <KService>
#include <Solid/OpticalDrive>
#include <Solid/StorageAccess>

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

#include <KFilePlacesModel>
#include <KFormat>
#include <KLocalizedString>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/PortableMediaPlayer>

#include <QStorageInfo>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QTimer>

#include <algorithm>

namespace Raye::Pages
{
QString spaceText(qint64 freeBytes, qint64 totalBytes)
{
    if (totalBytes <= 0) {
        return QString();
    }
    const KFormat format;
    // Whole units like Windows past 10 GB, one decimal below.
    const auto size = [&format](qint64 bytes) {
        const int precision = bytes >= qint64(10) * 1024 * 1024 * 1024 ? 0 : 1;
        return format.formatByteSize(double(qMax<qint64>(0, bytes)), precision, KFormat::JEDECBinaryDialect);
    };
    return i18nc("@info drive space under its name", "%1 free of %2", size(freeBytes), size(totalBytes));
}

QString ThisPcModel::shareName(const QString &share, const QString &server)
{
    return i18nc("@item a shared folder, then the computer it is on", "%1 (%2)", share, QStringLiteral("\\\\") + server);
}

ThisPcModel::ThisPcModel(const DriveLister &drives, const SpaceReader &space, QObject *parent)
    : QAbstractListModel(parent)
    , m_lister(drives)
    , m_space(space)
{
    if (!m_lister) {
        m_lister = &Locations::listDrives;
        // A stick plugged in or pulled out while This PC is open.
        auto *notifier = Solid::DeviceNotifier::instance();
        const auto later = [this] {
            QTimer::singleShot(500, this, &ThisPcModel::reload);
        };
        connect(notifier, &Solid::DeviceNotifier::deviceAdded, this, later);
        connect(notifier, &Solid::DeviceNotifier::deviceRemoved, this, later);
        m_phones = &ThisPcModel::systemPhones;
        m_network = &ThisPcModel::systemNetworkLocations;
        m_computers = &ThisPcModel::findSystemComputers;
        connect(NetDrives::Notifier::instance(), &NetDrives::Notifier::changed, this, &ThisPcModel::reload);
    }
    if (!m_space) {
        m_space = [](const QString &mountPoint) {
            const QStorageInfo info(mountPoint);
            return Space{info.bytesTotal(), info.bytesAvailable()};
        };
    }
    reload();
    findComputers();
}

void ThisPcModel::findComputers()
{
    if (!m_computers) {
        return;
    }
    QPointer<ThisPcModel> self(this);
    m_computers(this, [self](const QList<Place> &found) {
        if (self && !found.isEmpty()) {
            self->m_foundComputers = found;
            self->reload();
        }
    });
}

void ThisPcModel::reload()
{
    beginResetModel();
    m_rows.clear();
    const QList<Place> network = m_network ? m_network() : QList<Place>();
    QStringList networkMounts;
    for (const Place &p : network) {
        if (p.url.isLocalFile()) {
            networkMounts << p.url.toLocalFile();
        }
    }
    const QList<Locations::Drive> drives = m_lister();
    for (const Locations::Drive &d : drives) {
        if (!networkMounts.contains(d.mountPoint)) {
            m_rows.append({d, m_space(d.mountPoint), QStringLiteral("drives")});
        }
    }
    const auto addPlaces = [this](const QList<Place> &places, const QString &section) {
        for (const Place &p : places) {
            Locations::Drive d;
            d.mountPoint = p.url.isLocalFile() ? p.url.toLocalFile() : p.url.toString();
            d.name = p.name;
            d.iconName = p.iconName;
            // A connected network drive shows how full it is, like a disk.
            const Space space = p.url.isLocalFile() && p.connected && section == QLatin1String("network") ? m_space(d.mountPoint) : Space();
            m_rows.append({d, space, section, p.mapped, p.connected});
            m_urls.insert(m_rows.size() - 1, p.url);
        }
    };
    m_urls.clear();
    addPlaces(m_phones ? m_phones() : QList<Place>(), QStringLiteral("phones"));
    addPlaces(network, QStringLiteral("network"));
    addPlaces(m_foundComputers, QStringLiteral("computers"));
    endResetModel();
    Q_EMIT countChanged();
}

void ThisPcModel::setComputerFinder(const ComputerFinder &finder)
{
    m_computers = finder;
    m_foundComputers.clear();
    reload();
    findComputers();
}

int ThisPcModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.count();
}

QVariant ThisPcModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.count()) {
        return {};
    }
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case UrlRole:
        return m_urls.contains(index.row()) ? m_urls.value(index.row()) : QUrl::fromLocalFile(r.drive.mountPoint);
    case SectionRole:
        return r.section;
    case Qt::DisplayRole:
    case NameRole:
        return r.drive.name;
    case IconRole:
        return r.drive.iconName;
    case UsageRole:
        return r.space.total > 0 ? qreal(r.space.total - r.space.free) / qreal(r.space.total) : 0.0;
    case SpaceRole:
        if (!r.connected) {
            return i18nc("@info a network drive that is not connected right now", "Disconnected");
        }
        return spaceText(r.space.free, r.space.total);
    case RemovableRole:
        return r.drive.removable;
    case MappedRole:
        return r.mapped;
    case ConnectedRole:
        return r.connected;
    }
    return {};
}

QHash<int, QByteArray> ThisPcModel::roleNames() const
{
    return {
        {UrlRole, "url"},
        {NameRole, "name"},
        {IconRole, "iconName"},
        {UsageRole, "usage"},
        {SpaceRole, "spaceText"},
        {RemovableRole, "removable"},
        {SectionRole, "section"},
        {MappedRole, "mapped"},
        {ConnectedRole, "connected"},
    };
}

void ThisPcModel::setPlaceListers(const PlaceLister &phones, const PlaceLister &network)
{
    m_phones = phones;
    m_network = network;
    reload();
}

QStringList ThisPcModel::sections() const
{
    QStringList out;
    for (const Row &r : m_rows) {
        if (!out.contains(r.section)) {
            out << r.section;
        }
    }
    // Devices and drives always shows, if only to say there are none.
    if (!out.contains(QStringLiteral("drives"))) {
        out.prepend(QStringLiteral("drives"));
    }
    return out;
}

QString ThisPcModel::sectionTitle(const QString &section) const
{
    if (section == QLatin1String("phones")) {
        return i18nc("@title section of This PC", "Phones and cameras");
    }
    if (section == QLatin1String("network")) {
        return i18nc("@title section of This PC", "Network locations");
    }
    if (section == QLatin1String("computers")) {
        return i18nc("@title section of This PC", "Computers on the network");
    }
    return i18nc("@title section of This PC", "Devices and drives");
}

bool ThisPcModel::isNetworkFileSystem(const QByteArray &type)
{
    static const QList<QByteArray> types = {"cifs", "smb3", "smbfs", "nfs", "nfs4", "fuse.sshfs", "sshfs", "davfs", "fuse.davfs2", "9p"};
    return types.contains(type);
}

bool ThisPcModel::isRemotePlace(const QUrl &url)
{
    static const QStringList schemes = {QStringLiteral("smb"), QStringLiteral("sftp"), QStringLiteral("fish"), QStringLiteral("nfs"),
                                        QStringLiteral("webdav"), QStringLiteral("webdavs"), QStringLiteral("ftp"), QStringLiteral("ftps")};
    return schemes.contains(url.scheme()) && !url.host().isEmpty();
}

QList<ThisPcModel::Place> ThisPcModel::systemPhones()
{
    QList<Place> out;
    const QList<Solid::Device> players = Solid::Device::listFromType(Solid::DeviceInterface::PortableMediaPlayer);
    for (const Solid::Device &device : players) {
        const auto *player = device.as<Solid::PortableMediaPlayer>();
        if (!player) {
            continue;
        }
        const QStringList protocols = player->supportedProtocols();
        QUrl url;
        if (protocols.contains(QLatin1String("mtp"))) {
            url = QUrl(QStringLiteral("mtp:udi=%1").arg(device.udi()));
        } else if (protocols.contains(QLatin1String("afc"))) {
            url = QUrl(QStringLiteral("afc:/"));
        } else {
            continue;
        }
        const QString name = !device.product().isEmpty() ? device.product() : device.description();
        out.append({url, name, QStringLiteral("smartphone")});
    }
    return out;
}

QList<ThisPcModel::Place> ThisPcModel::systemNetworkLocations()
{
    QList<Place> out;
    // Shares the person added (Add a network location, or Places).
    KFilePlacesModel places;
    for (int i = 0; i < places.rowCount(); ++i) {
        const QModelIndex index = places.index(i, 0);
        const QUrl url = places.url(index);
        if (!places.isHidden(index) && isRemotePlace(url)) {
            out.append({url, places.text(index), QStringLiteral("folder-network")});
        }
    }
    // Shares mounted on the system, and the drives the person mapped (a
    // mapped drive that is not connected stays, so it can be connected or
    // removed).
    const QList<NetDrives::Mapping> mappings = NetDrives::loadMappings();
    const QList<NetDrives::Mount> mounts = NetDrives::networkMounts();
    QStringList seen;
    for (const NetDrives::Mount &m : mounts) {
        const QString path = QDir::cleanPath(m.mountPoint);
        if (seen.contains(path)) {
            continue;
        }
        seen << path;
        const NetDrives::Mapping mapping = NetDrives::mappingAt(path);
        const NetDrives::Address a = NetDrives::addressOfSource(m.source);
        QString name = mapping.name;
        if (name.isEmpty()) {
            name = a.isValid() ? shareName(a.share, a.server) : QFileInfo(path).fileName();
        }
        Place p{QUrl::fromLocalFile(path), name, QStringLiteral("folder-network")};
        p.mapped = !mapping.mountPoint.isEmpty();
        out.append(p);
    }
    for (const NetDrives::Mapping &m : mappings) {
        const QString path = QDir::cleanPath(m.mountPoint);
        if (seen.contains(path)) {
            continue;
        }
        Place p{QUrl::fromLocalFile(path), m.name, QStringLiteral("network-disconnect")};
        p.mapped = true;
        p.connected = false;
        out.append(p);
    }
    return out;
}

void ThisPcModel::findSystemComputers(QObject *context, const std::function<void(const QList<Place> &)> &found)
{
    // What the network page lists: computers and NAS found by WS-Discovery,
    // mDNS and NetBIOS (the smb worker does the looking).
    auto *job = KIO::listDir(QUrl(QStringLiteral("smb:/")), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    auto *list = new QList<Place>();
    QObject::connect(job, &KIO::ListJob::entries, context, [list](KIO::Job *, const KIO::UDSEntryList &entries) {
        for (const KIO::UDSEntry &e : entries) {
            const QString name = e.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME).isEmpty() ? e.stringValue(KIO::UDSEntry::UDS_NAME)
                                                                                          : e.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
            if (name.isEmpty() || name == QLatin1String(".")) {
                continue;
            }
            QUrl url(e.stringValue(KIO::UDSEntry::UDS_URL));
            if (!url.isValid() || url.isEmpty()) {
                url = QUrl(QStringLiteral("smb://") + e.stringValue(KIO::UDSEntry::UDS_NAME));
            }
            const QString icon = e.stringValue(KIO::UDSEntry::UDS_ICON_NAME);
            list->append({url, name, icon.isEmpty() ? QStringLiteral("network-server") : icon});
        }
    });
    QObject::connect(job, &KJob::result, context, [list, found](KJob *) {
        std::sort(list->begin(), list->end(), [](const Place &a, const Place &b) {
            return a.name.localeAwareCompare(b.name) < 0;
        });
        found(*list);
        delete list;
    });
}

void ThisPcModel::open(const QUrl &url)
{
    Q_EMIT openRequested(url);
}
}

void Raye::Pages::ThisPcModel::properties(const QUrl &url)
{
    Q_EMIT propertiesRequested(url);
}

void Raye::Pages::ThisPcModel::openInNewTab(const QUrl &url)
{
    Q_EMIT openInNewTabRequested(url);
}

void Raye::Pages::ThisPcModel::openInNewWindow(const QUrl &url)
{
    Q_EMIT openInNewWindowRequested(url);
}

void Raye::Pages::ThisPcModel::mapNetworkDrive(const QUrl &share)
{
    Q_EMIT mapNetworkDriveRequested(share);
}

void Raye::Pages::ThisPcModel::disconnectDrive(const QUrl &url)
{
    QPointer<ThisPcModel> self(this);
    Raye::NetDrives::disconnectDrive(url.toLocalFile(), this, [self](Raye::NetDrives::Failure failure, const QString &text) {
        if (!self) {
            return;
        }
        if (failure != Raye::NetDrives::Failure::None) {
            Q_EMIT self->errorOccurred(text);
        }
        self->reload();
    });
}

void Raye::Pages::ThisPcModel::reconnect(const QUrl &url)
{
    using namespace Raye::NetDrives;
    const Mapping m = mappingAt(url.toLocalFile());
    if (m.mountPoint.isEmpty()) {
        Q_EMIT openRequested(url);
        return;
    }
    QString password;
    if (!m.user.isEmpty() && (!m.remember || !readPassword(m, &password))) {
        Q_EMIT reconnectRequested(m.mountPoint);
        return;
    }
    QPointer<ThisPcModel> self(this);
    connectDrive(m, password, this, [self, url](Failure failure, const QString &text) {
        if (!self) {
            return;
        }
        self->reload();
        if (failure != Failure::None) {
            Q_EMIT self->errorOccurred(text);
        } else {
            Q_EMIT self->openRequested(url);
        }
    });
}

void Raye::Pages::ThisPcModel::eject(const QUrl &url)
{
    const QString path = QDir::cleanPath(url.toLocalFile());
    const QList<Solid::Device> devices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    for (Solid::Device device : devices) {
        auto *access = device.as<Solid::StorageAccess>();
        if (!access || QDir::cleanPath(access->filePath()) != path) {
            continue;
        }
        for (Solid::Device d = device; d.isValid(); d = d.parent()) {
            if (auto *optical = d.as<Solid::OpticalDrive>()) {
                optical->eject();
                return;
            }
        }
        access->teardown();
        return;
    }
}

bool Raye::Pages::ThisPcModel::canFormat() const
{
    return KService::serviceByDesktopName(QStringLiteral("org.kde.partitionmanager")) != nullptr;
}

void Raye::Pages::ThisPcModel::format()
{
    if (const KService::Ptr diskManager = KService::serviceByDesktopName(QStringLiteral("org.kde.partitionmanager"))) {
        auto *job = new KIO::ApplicationLauncherJob(diskManager);
        job->start();
    }
}

void Raye::Pages::ThisPcModel::copyAsPath(const QUrl &url)
{
    copyPathToClipboard(url);
}

void Raye::Pages::ThisPcModel::pinToQuickAccess(const QUrl &url)
{
    if (isPinnedToQuickAccess(url)) {
        unpinFromQuickAccess(url);
    } else {
        Raye::Pages::pinToQuickAccess(url);
    }
}

bool Raye::Pages::ThisPcModel::isPinned(const QUrl &url) const
{
    return isPinnedToQuickAccess(url);
}
