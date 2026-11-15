/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// This PC: the drive tiles, how full each drive is and its free space.

#include "pages/thispcmodel.h"

#include <KLocalizedString>

#include <QSignalSpy>
#include <QTest>

using Raye::Pages::ThisPcModel;

namespace
{
constexpr qint64 GiB = qint64(1024) * 1024 * 1024;
}

class ThisPcModelTest : public QObject
{
    Q_OBJECT

private:
    static ThisPcModel *model(QObject *parent)
    {
        const auto drives = [] {
            return QList<Raye::Locations::Drive>{
                {QStringLiteral("/"), QStringLiteral("Raye"), QStringLiteral("drive-harddisk-system"), false},
                {QStringLiteral("/media/ann/USB"), QStringLiteral("USB Drive"), QStringLiteral("drive-removable-media-usb"), true},
            };
        };
        const auto space = [](const QString &mount) {
            if (mount == QLatin1String("/")) {
                return ThisPcModel::Space{476 * GiB, 100 * GiB};
            }
            return ThisPcModel::Space{8 * GiB, qint64(5.5 * GiB)};
        };
        return new ThisPcModel(drives, space, parent);
    }

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setLanguages({QStringLiteral("en")});
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    void spaceText()
    {
        QCOMPARE(Raye::Pages::spaceText(100 * GiB, 476 * GiB), QStringLiteral("100 GB free of 476 GB"));
        QCOMPARE(Raye::Pages::spaceText(qint64(5.5 * GiB), 8 * GiB), QStringLiteral("5.5 GB free of 8.0 GB"));
        QCOMPARE(Raye::Pages::spaceText(0, 0), QString());
    }

    void rows()
    {
        QObject parent;
        ThisPcModel *m = model(&parent);
        QCOMPARE(m->rowCount(), 2);
        const QModelIndex first = m->index(0);
        QCOMPARE(first.data(ThisPcModel::NameRole).toString(), QStringLiteral("Raye"));
        QCOMPARE(first.data(ThisPcModel::UrlRole).toUrl(), QUrl::fromLocalFile(QStringLiteral("/")));
        QCOMPARE(first.data(ThisPcModel::UsageRole).toReal(), qreal(376) / qreal(476));
        QCOMPARE(first.data(ThisPcModel::SpaceRole).toString(), QStringLiteral("100 GB free of 476 GB"));
        QVERIFY(m->index(1).data(ThisPcModel::RemovableRole).toBool());
    }

    void phonesAndNetworkLocations()
    {
        QObject parent;
        ThisPcModel *m = model(&parent);
        m->setPlaceListers(
            [] {
                return QList<ThisPcModel::Place>{{QUrl(QStringLiteral("mtp:udi=/org/kde/solid/udev/phone")), QStringLiteral("Pixel 8"), QStringLiteral("phone")}};
            },
            [] {
                return QList<ThisPcModel::Place>{{QUrl(QStringLiteral("smb://nas/photos")), QStringLiteral("photos on nas"), QStringLiteral("folder-network")}};
            });
        QCOMPARE(m->rowCount(), 4);
        QCOMPARE(m->index(0).data(ThisPcModel::SectionRole).toString(), QStringLiteral("drives"));
        QCOMPARE(m->index(2).data(ThisPcModel::SectionRole).toString(), QStringLiteral("phones"));
        QCOMPARE(m->index(2).data(ThisPcModel::NameRole).toString(), QStringLiteral("Pixel 8"));
        QCOMPARE(m->index(2).data(ThisPcModel::SpaceRole).toString(), QString());
        QCOMPARE(m->index(3).data(ThisPcModel::SectionRole).toString(), QStringLiteral("network"));
        QCOMPARE(m->index(3).data(ThisPcModel::UrlRole).toUrl(), QUrl(QStringLiteral("smb://nas/photos")));
        QCOMPARE(m->sections(), (QStringList{QStringLiteral("drives"), QStringLiteral("phones"), QStringLiteral("network")}));
    }

    void emptySectionsAreLeftOut()
    {
        QObject parent;
        ThisPcModel *m = model(&parent);
        m->setPlaceListers({}, {});
        QCOMPARE(m->sections(), QStringList{QStringLiteral("drives")});
    }

    void networkMountsAreNotDrives()
    {
        QVERIFY(ThisPcModel::isNetworkFileSystem("cifs"));
        QVERIFY(ThisPcModel::isNetworkFileSystem("nfs4"));
        QVERIFY(ThisPcModel::isNetworkFileSystem("fuse.sshfs"));
        QVERIFY(!ThisPcModel::isNetworkFileSystem("btrfs"));
        QVERIFY(ThisPcModel::isRemotePlace(QUrl(QStringLiteral("sftp://ann@host/home"))));
        QVERIFY(!ThisPcModel::isRemotePlace(QUrl::fromLocalFile(QStringLiteral("/home/ann"))));
        QVERIFY(!ThisPcModel::isRemotePlace(QUrl(QStringLiteral("trash:/"))));
    }

    void computersAndMappedDrives()
    {
        QObject parent;
        ThisPcModel *m = model(&parent);
        ThisPcModel::Place mapped{QUrl::fromLocalFile(QStringLiteral("/home/ann/Network/photos")), QStringLiteral("photos"), QStringLiteral("folder-network")};
        mapped.mapped = true;
        ThisPcModel::Place away{QUrl::fromLocalFile(QStringLiteral("/home/ann/Network/music")), QStringLiteral("music"), QStringLiteral("network-disconnect")};
        away.mapped = true;
        away.connected = false;
        m->setPlaceListers({}, [mapped, away] {
            return QList<ThisPcModel::Place>{mapped, away};
        });
        m->setComputerFinder([](QObject *, const std::function<void(const QList<ThisPcModel::Place> &)> &found) {
            found({{QUrl(QStringLiteral("smb://nas")), QStringLiteral("NAS"), QStringLiteral("network-server")}});
        });
        QCOMPARE(m->sections(), (QStringList{QStringLiteral("drives"), QStringLiteral("network"), QStringLiteral("computers")}));
        const int last = m->rowCount() - 1;
        QCOMPARE(m->index(last).data(ThisPcModel::NameRole).toString(), QStringLiteral("NAS"));
        QCOMPARE(m->index(last).data(ThisPcModel::UrlRole).toUrl(), QUrl(QStringLiteral("smb://nas")));
        QVERIFY(m->index(2).data(ThisPcModel::MappedRole).toBool());
        QVERIFY(m->index(2).data(ThisPcModel::ConnectedRole).toBool());
        // A connected network drive shows its space like a disk.
        QCOMPARE(m->index(2).data(ThisPcModel::SpaceRole).toString(), QStringLiteral("5.5 GB free of 8.0 GB"));
        QVERIFY(!m->index(3).data(ThisPcModel::ConnectedRole).toBool());
        QCOMPARE(m->index(3).data(ThisPcModel::SpaceRole).toString(), QStringLiteral("Disconnected"));
        QCOMPARE(m->sectionTitle(QStringLiteral("computers")), QStringLiteral("Computers on the network"));
        QCOMPARE(ThisPcModel::shareName(QStringLiteral("data"), QStringLiteral("192.168.50.222")), QStringLiteral("data (\\\\192.168.50.222)"));
    }

    void tileActionsGoThroughTheHost()
    {
        QObject parent;
        ThisPcModel *m = model(&parent);
        QSignalSpy tab(m, &ThisPcModel::openInNewTabRequested);
        QSignalSpy window(m, &ThisPcModel::openInNewWindowRequested);
        QSignalSpy map(m, &ThisPcModel::mapNetworkDriveRequested);
        m->openInNewTab(QUrl::fromLocalFile(QStringLiteral("/")));
        m->openInNewWindow(QUrl::fromLocalFile(QStringLiteral("/")));
        m->mapNetworkDrive(QUrl(QStringLiteral("smb://nas")));
        QCOMPARE(tab.count(), 1);
        QCOMPARE(window.count(), 1);
        QCOMPARE(map.count(), 1);
        QCOMPARE(map.first().first().toUrl(), QUrl(QStringLiteral("smb://nas")));
    }

    void openGoesThroughTheHost()
    {
        QObject parent;
        ThisPcModel *m = model(&parent);
        QSignalSpy spy(m, &ThisPcModel::openRequested);
        m->open(QUrl::fromLocalFile(QStringLiteral("/")));
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_GUILESS_MAIN(ThisPcModelTest)

#include "test_thispcmodel.moc"
