/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"
#include "locations/locationnames.h"

#include <QAbstractListModel>
#include <QSet>
#include <QUrl>

#include <functional>

namespace Raye::Pages
{
// "123 GB free of 476 GB", as This PC writes it under a drive.
DOLPHIN_EXPORT QString spaceText(qint64 freeBytes, qint64 totalBytes);

// What This PC shows (explorer spec 1.10, 3.10), in three sections:
// "drives" (name, icon, how full), "phones" (phones and cameras) and
// "network" (shares the person added or mounted). An empty section is not
// shown.
class DOLPHIN_EXPORT ThisPcModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QStringList sections READ sections NOTIFY countChanged)

public:
    struct Space {
        qint64 total = 0;
        qint64 free = 0;
    };
    using SpaceReader = std::function<Space(const QString &mountPoint)>;
    using DriveLister = std::function<QList<Locations::Drive>()>;
    struct Place {
        QUrl url;
        QString name;
        QString iconName;
        // A network drive the person mapped (Disconnect), and whether it is
        // connected right now.
        bool mapped = false;
        bool connected = true;
    };
    using PlaceLister = std::function<QList<Place>()>;
    // Lists the computers seen on the network, then calls back (later).
    using ComputerFinder = std::function<void(QObject *context, const std::function<void(const QList<Place> &)> &found)>;

    enum Roles {
        UrlRole = Qt::UserRole + 1,
        NameRole,
        IconRole,
        UsageRole,
        SpaceRole,
        RemovableRole,
        SectionRole,
        MappedRole,
        ConnectedRole,
    };

    // Empty functions read the system (Locations::listDrives, QStorageInfo).
    explicit ThisPcModel(const DriveLister &drives = {}, const SpaceReader &space = {}, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Empty functions: nothing in that section. The system ones are set
    // when the model reads the system.
    void setPlaceListers(const PlaceLister &phones, const PlaceLister &network);
    void setComputerFinder(const ComputerFinder &finder);
    QStringList sections() const;
    // A section's title, in the user's language.
    Q_INVOKABLE QString sectionTitle(const QString &section) const;

    static bool isNetworkFileSystem(const QByteArray &type);
    static bool isRemotePlace(const QUrl &url);
    static QList<Place> systemPhones();
    static QList<Place> systemNetworkLocations();
    static void findSystemComputers(QObject *context, const std::function<void(const QList<Place> &)> &found);
    // "data (\\\\nas)": a share mounted without a name of its own.
    static QString shareName(const QString &share, const QString &server);

    Q_INVOKABLE void open(const QUrl &url);
    Q_INVOKABLE void openInNewTab(const QUrl &url);
    Q_INVOKABLE void openInNewWindow(const QUrl &url);
    Q_INVOKABLE void properties(const QUrl &url);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void mapNetworkDrive(const QUrl &share = QUrl());
    Q_INVOKABLE void disconnectDrive(const QUrl &url);
    // A mapped drive that is not connected: connect it again, asking for
    // its password when the wallet does not have it.
    Q_INVOKABLE void reconnect(const QUrl &url);
    Q_INVOKABLE void eject(const QUrl &url);
    Q_INVOKABLE void format();
    Q_INVOKABLE bool canFormat() const;
    Q_INVOKABLE void pinToQuickAccess(const QUrl &url);
    Q_INVOKABLE bool isPinned(const QUrl &url) const;
    Q_INVOKABLE void copyAsPath(const QUrl &url);

Q_SIGNALS:
    void countChanged();
    void openRequested(const QUrl &url);
    void openInNewTabRequested(const QUrl &url);
    void openInNewWindowRequested(const QUrl &url);
    void propertiesRequested(const QUrl &url);
    void mapNetworkDriveRequested(const QUrl &share);
    // A mapped drive whose password has to be typed again.
    void reconnectRequested(const QString &mountPoint);
    void errorOccurred(const QString &text);

private:
    void findComputers();

    struct Row {
        Locations::Drive drive;
        Space space;
        QString section;
        bool mapped = false;
        bool connected = true;
    };
    DriveLister m_lister;
    PlaceLister m_phones;
    PlaceLister m_network;
    ComputerFinder m_computers;
    QList<Place> m_foundComputers;
    SpaceReader m_space;
    QList<Row> m_rows;
    QHash<int, QUrl> m_urls;
    bool m_watchMounts = false;
    QSet<QString> m_watched;
    void addUnmountedDrives();
};
}
