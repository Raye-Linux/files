/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The Properties dialog's backend: sizes, who can open a file, hidden,
// read-only, drives, and the details of a picture.

#include "netdrive/networkdrives.h"
#include "properties/properties.h"

#include <KLocalizedString>

#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

using namespace Raye::Properties;

namespace
{
void write(const QString &path, int bytes)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QByteArray(bytes, 'x'));
}
}

class PropertiesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("raye-files");
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    void accessFromMode()
    {
        QCOMPARE(accessFor(0644, Who::Owner), Access::ReadWrite);
        QCOMPARE(accessFor(0644, Who::Group), Access::Read);
        QCOMPARE(accessFor(0640, Who::Others), Access::None);
        QCOMPARE(accessFor(0200, Who::Owner), Access::None);
    }

    void accessToMode()
    {
        QCOMPARE(withAccess(0644, Who::Others, Access::None, false), mode_t(0640));
        QCOMPARE(withAccess(0755, Who::Group, Access::ReadWrite, true), mode_t(0775));
        QCOMPARE(withAccess(0700, Who::Others, Access::Read, true), mode_t(0705));
        // a program keeps its x bits
        QCOMPARE(withAccess(0755, Who::Owner, Access::Read, false), mode_t(0555));
        QCOMPARE(withAccess(04755, Who::Others, Access::None, false), mode_t(04750));
    }

    void measuresATree()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("a/b")));
        write(dir.filePath(QStringLiteral("one")), 1000);
        write(dir.filePath(QStringLiteral("a/two")), 2000);
        write(dir.filePath(QStringLiteral("a/b/three")), 3000);
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("one")), dir.filePath(QStringLiteral("a/link"))));
        const Totals t = measure({dir.filePath(QStringLiteral("a")), dir.filePath(QStringLiteral("one"))});
        QCOMPARE(t.files, 4); // two, three, link, one
        QCOMPARE(t.folders, 2); // a, b
        QVERIFY(t.size >= 6000);
        QVERIFY(t.onDisk > 0);
        QCOMPARE(t.onDisk % 512, 0);
    }

    void texts()
    {
        QCOMPARE(sizeText(2516582), QStringLiteral("2.4 MB (2,516,582 bytes)"));
        QCOMPARE(sizeText(1), QStringLiteral("1 byte (1 byte)"));
        QCOMPARE(containsText(12, 3), QStringLiteral("12 files, 3 folders"));
        QCOMPARE(containsText(1, 0), QStringLiteral("1 file, 0 folders"));
        QCOMPARE(fileSystemName(QStringLiteral("btrfs")), QStringLiteral("Btrfs"));
        QCOMPARE(fileSystemName(QStringLiteral("vfat")), QStringLiteral("FAT32"));
        QCOMPARE(fileSystemName(QStringLiteral("ntfs3")), QStringLiteral("NTFS"));
        QCOMPARE(fileSystemName(QStringLiteral("ext4")), QStringLiteral("ext4"));
    }

    void hiddenFile()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(QStringLiteral("a.txt"));
        const QString b = dir.filePath(QStringLiteral("b.txt"));
        write(a, 1);
        write(b, 1);
        QVERIFY(!isHidden(a));
        QVERIFY(setHidden(a, true));
        QVERIFY(setHidden(b, true));
        QVERIFY(isHidden(a));
        QVERIFY(setHidden(a, false));
        QVERIFY(!isHidden(a));
        QVERIFY(isHidden(b));
        QFile list(dir.filePath(QStringLiteral(".hidden")));
        QVERIFY(list.open(QIODevice::ReadOnly));
        QCOMPARE(list.readAll().trimmed(), QByteArray("b.txt"));

        const QString dot = dir.filePath(QStringLiteral(".config"));
        write(dot, 1);
        QVERIFY(isHidden(dot));
        QVERIFY(!setHidden(dot, false));
    }

    void appliesChanges()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("notes.txt"));
        write(path, 10);
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther);

        PropertiesModel model({QUrl::fromLocalFile(path)});
        QCOMPARE(model.name(), QStringLiteral("notes.txt"));
        QVERIFY(model.canRename());
        QVERIFY(!model.readOnly());
        QCOMPARE(model.othersAccess(), int(Access::Read));
        QVERIFY(!model.dirty());

        model.setName(QStringLiteral("plan.txt"));
        model.setReadOnly(true);
        model.setOthersAccess(int(Access::None));
        model.setHidden(true);
        QVERIFY(model.dirty());
        QVERIFY2(model.apply(), qPrintable(model.error()));
        QVERIFY(!model.dirty());

        const QString renamed = dir.filePath(QStringLiteral("plan.txt"));
        QVERIFY(QFileInfo::exists(renamed));
        QVERIFY(!QFileInfo::exists(path));
        struct stat st;
        QCOMPARE(::stat(QFile::encodeName(renamed).constData(), &st), 0);
        QCOMPARE(mode_t(st.st_mode & 07777), mode_t(0440));
        QVERIFY(isHidden(renamed));
    }

    void refusesABadName()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("x"));
        write(path, 1);
        PropertiesModel model({QUrl::fromLocalFile(path)});
        model.setName(QStringLiteral("a/b"));
        QVERIFY(!model.apply());
        QVERIFY(!model.error().isEmpty());
        QVERIFY(QFileInfo::exists(path));
    }

    void folderIsMeasured()
    {
        QTemporaryDir dir;
        write(dir.filePath(QStringLiteral("one")), 100);
        write(dir.filePath(QStringLiteral("two")), 100);
        PropertiesModel model({QUrl::fromLocalFile(dir.path())});
        QVERIFY(model.folder());
        if (model.measuring()) {
            QSignalSpy spy(&model, &PropertiesModel::measured);
            QVERIFY(spy.wait(5000));
        }
        QCOMPARE(model.containsText(), QStringLiteral("2 files, 0 folders"));
        QCOMPARE(model.typeText(), QStringLiteral("File folder"));
    }

    void severalItems()
    {
        QTemporaryDir dir;
        write(dir.filePath(QStringLiteral("one")), 100);
        write(dir.filePath(QStringLiteral("two")), 300);
        PropertiesModel model({QUrl::fromLocalFile(dir.filePath(QStringLiteral("one"))), QUrl::fromLocalFile(dir.filePath(QStringLiteral("two")))});
        QCOMPARE(model.count(), 2);
        QVERIFY(!model.canRename());
        if (model.measuring()) {
            QSignalSpy spy(&model, &PropertiesModel::measured);
            QVERIFY(spy.wait(5000));
        }
        QCOMPARE(model.totals().size, qint64(400));
        QCOMPARE(model.name(), QStringLiteral("2 items"));
    }

    void driveVariant()
    {
        constexpr qint64 GiB = qint64(1024) * 1024 * 1024;
        PropertiesModel model({QUrl::fromLocalFile(QStringLiteral("/"))}, nullptr, QStringLiteral("Raye"), [](const QString &) {
            return PropertiesModel::Space{100 * GiB, 25 * GiB, QStringLiteral("btrfs")};
        });
        QVERIFY(model.drive());
        QCOMPARE(model.name(), QStringLiteral("Raye"));
        QCOMPARE(model.fileSystem(), QStringLiteral("Btrfs"));
        QCOMPARE(model.usage(), 0.75);
        QVERIFY(model.freeText().startsWith(QStringLiteral("25 GB")));
        QVERIFY(model.capacityText().startsWith(QStringLiteral("100 GB")));
        QVERIFY(!model.canRename());
    }

    void pictureDetails()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("p.png"));
        QImage image(40, 30, QImage::Format_RGB32);
        image.fill(Qt::red);
        QImage big(1600, 1200, QImage::Format_RGB32);
        big.fill(Qt::blue);
        QVERIFY(big.save(dir.filePath(QStringLiteral("big.png"))));
        QVERIFY(mediaDetails(dir.filePath(QStringLiteral("big.png"))).first().toMap().value(QStringLiteral("value")).toString() == QStringLiteral("1600 x 1200"));
        QVERIFY(image.save(path));
        const QVariantList details = mediaDetails(path);
        bool found = false;
        for (const QVariant &d : details) {
            const QVariantMap m = d.toMap();
            if (m.value(QStringLiteral("label")) == QLatin1String("Dimensions")) {
                QCOMPARE(m.value(QStringLiteral("value")).toString(), QStringLiteral("40 x 30"));
                found = true;
            }
        }
        QVERIFY(found);
        // a text file has nothing for the Details tab
        write(dir.filePath(QStringLiteral("t.txt")), 3);
        QVERIFY(mediaDetails(dir.filePath(QStringLiteral("t.txt"))).isEmpty());
    }

    void localFilesAndNetworkPlaces()
    {
        QVERIFY(PropertiesModel::handles({QUrl::fromLocalFile(QStringLiteral("/"))}));
        QVERIFY(PropertiesModel::handles({QUrl(QStringLiteral("smb://server/share"))}));
        QVERIFY(!PropertiesModel::handles({QUrl(QStringLiteral("smb://server/share")), QUrl(QStringLiteral("smb://server/other"))}));
        QVERIFY(!PropertiesModel::handles({QUrl::fromLocalFile(QStringLiteral("/no/such/file"))}));
        QVERIFY(!PropertiesModel::handles({}));
    }

    void networkDrive()
    {
        QTemporaryDir dir;
        const QString folder = dir.filePath(QStringLiteral("data"));
        QVERIFY(QDir().mkpath(folder));
        Raye::NetDrives::setMountReader([folder] {
            return QList<Raye::NetDrives::Mount>{{QStringLiteral("//192.168.50.222/data"), folder, QStringLiteral("cifs")}};
        });
        constexpr qint64 GiB = qint64(1024) * 1024 * 1024;
        PropertiesModel m({QUrl::fromLocalFile(folder)}, nullptr, QString(), [](const QString &) {
            return PropertiesModel::Space{2048 * GiB, 640 * GiB, QStringLiteral("cifs")};
        });
        Raye::NetDrives::setMountReader({});
        QVERIFY(m.drive());
        QVERIFY(m.network());
        QCOMPARE(m.typeText(), QStringLiteral("Network drive"));
        QCOMPARE(m.protocol(), QStringLiteral("SMB"));
        QCOMPARE(m.server(), QStringLiteral("\\\\192.168.50.222"));
        QCOMPARE(m.networkFolder(), QStringLiteral("\\\\192.168.50.222\\data"));
        QCOMPARE(m.mountedAt(), folder);
        QVERIFY(m.hasSpace());
        QVERIFY(m.fileSystem().isEmpty());
        QVERIFY(!m.typeText().contains(QStringLiteral("Local")));
    }

    void networkLocation()
    {
        const QUrl share(QStringLiteral("smb://nas/photos/2026"));
        QVERIFY(PropertiesModel::handles({share}));
        QVERIFY(!PropertiesModel::handles({QUrl(QStringLiteral("trash:/x"))}));
        QVERIFY(!PropertiesModel::handles({QUrl(QStringLiteral("smb:/"))}));
        PropertiesModel m({share});
        QVERIFY(m.network());
        QCOMPARE(m.name(), QStringLiteral("2026"));
        QCOMPARE(m.typeText(), QStringLiteral("Network location"));
        QCOMPARE(m.networkFolder(), QStringLiteral("\\\\nas\\photos\\2026"));
        QVERIFY(!m.hasSpace());
        QVERIFY(!m.canRename());
        QVERIFY(!m.dirty());
        PropertiesModel computer({QUrl(QStringLiteral("smb://nas"))});
        QCOMPARE(computer.typeText(), QStringLiteral("Computer on the network"));
        QCOMPARE(computer.iconName(), QStringLiteral("network-server"));
    }
};

QTEST_MAIN(PropertiesTest)

#include "test_properties.moc"
