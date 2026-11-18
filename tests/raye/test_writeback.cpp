/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Copies to a USB drive: what is still on its way to the drive, which
// drives count as removable, the honest percentage, and the card that
// stays until the drive has everything.

#include "jobs/jobsmodel.h"
#include "jobs/jobtracker.h"
#include "jobs/writeback.h"

#include <KIO/CopyJob>

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Raye::Jobs;

namespace
{
void touch(const QString &path, const QByteArray &data = {})
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

// /sys/class/block/<name> links to the device in /sys/devices, like the kernel
void link(const QString &root, const QString &name, const QString &devicePath)
{
    QDir().mkpath(root + QStringLiteral("/class/block"));
    QVERIFY(QFile::link(root + devicePath, root + QStringLiteral("/class/block/") + name));
}
}

class WritebackTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void meminfo()
    {
        const QByteArray text =
            "MemTotal:       16303436 kB\n"
            "Dirty:             20480 kB\n"
            "Writeback:          4096 kB\n"
            "WritebackTmp:          8 kB\n";
        QCOMPARE(pendingWriteBytes(text), qulonglong(24576) * 1024);
        QCOMPARE(pendingWriteBytes(QByteArray()), qulonglong(0));
    }

    void removable()
    {
        QTemporaryDir sys;
        QVERIFY(sys.isValid());
        const QString root = sys.path();
        const QString usb = QStringLiteral("/devices/pci0000:00/0000:00:14.0/usb2/2-1/2-1:1.0/host6/target6:0:0/6:0:0:0/block/sdb");
        const QString sata = QStringLiteral("/devices/pci0000:00/0000:00:17.0/ata1/host0/target0:0:0/0:0:0:0/block/sda");
        const QString nvme = QStringLiteral("/devices/pci0000:00/0000:00:1d.0/0000:3d:00.0/nvme/nvme0/nvme0n1");
        const QString card = QStringLiteral("/devices/pci0000:00/0000:00:1c.0/0000:02:00.0/mmc_host/mmc0/mmc0:aaaa/block/mmcblk0");
        const QString stick = QStringLiteral("/devices/pci0000:00/0000:00:14.0/0000:01:00.0/host7/target7:0:0/7:0:0:0/block/sdc");
        touch(root + usb + QStringLiteral("/removable"), "0\n");
        touch(root + usb + QStringLiteral("/sdb1/partition"), "1\n");
        touch(root + sata + QStringLiteral("/removable"), "0\n");
        touch(root + sata + QStringLiteral("/sda2/partition"), "2\n");
        touch(root + nvme + QStringLiteral("/removable"), "0\n");
        touch(root + nvme + QStringLiteral("/nvme0n1p1/partition"), "1\n");
        touch(root + card + QStringLiteral("/removable"), "0\n");
        touch(root + card + QStringLiteral("/mmcblk0p1/partition"), "1\n");
        touch(root + stick + QStringLiteral("/removable"), "1\n");
        touch(root + stick + QStringLiteral("/sdc1/partition"), "1\n");
        link(root, QStringLiteral("sdb1"), usb + QStringLiteral("/sdb1"));
        link(root, QStringLiteral("sda2"), sata + QStringLiteral("/sda2"));
        link(root, QStringLiteral("nvme0n1p1"), nvme + QStringLiteral("/nvme0n1p1"));
        link(root, QStringLiteral("mmcblk0p1"), card + QStringLiteral("/mmcblk0p1"));
        link(root, QStringLiteral("sdc1"), stick + QStringLiteral("/sdc1"));
        link(root, QStringLiteral("sdc"), stick);
        QDir().mkpath(root + QStringLiteral("/block"));
        for (const auto &[disk, path] : {std::pair{"sdb", usb}, std::pair{"sda", sata}, std::pair{"nvme0n1", nvme}, std::pair{"mmcblk0", card}, std::pair{"sdc", stick}}) {
            QVERIFY(QFile::link(root + path, root + QStringLiteral("/block/") + QLatin1String(disk)));
        }

        QVERIFY(isRemovableDevice(QStringLiteral("/dev/sdb1"), root)); // USB disk, removable 0
        QVERIFY(isRemovableDevice(QStringLiteral("/dev/mmcblk0p1"), root)); // memory card
        QVERIFY(isRemovableDevice(QStringLiteral("/dev/sdc1"), root)); // removable 1
        QVERIFY(isRemovableDevice(QStringLiteral("/dev/sdc"), root)); // a stick with no partition table
        QVERIFY(!isRemovableDevice(QStringLiteral("/dev/sda2"), root));
        QVERIFY(!isRemovableDevice(QStringLiteral("/dev/nvme0n1p1"), root));
        QVERIFY(!isRemovableDevice(QStringLiteral("/dev/sdz1"), root));
        QVERIFY(!isRemovableDevice(QStringLiteral("server:/share"), root));
        QVERIFY(!isRemovableDevice(QStringLiteral("tmpfs"), root));
    }

    void percent()
    {
        QCOMPARE(writtenPercent(0, 0, 0), 0);
        QCOMPARE(writtenPercent(1000, 500, 0), 50);
        // KIO says 100 %, 300 bytes still in memory
        QCOMPARE(writtenPercent(1000, 1000, 300), 70);
        // everything copied and nothing pending is still not "done": the
        // card finishes when the flush returns
        QCOMPARE(writtenPercent(1000, 1000, 0), 99);
        QCOMPARE(writtenPercent(1000, 200, 900), 0);
        QCOMPARE(writtenPercent(1000, 2000, 0), 99);
    }

    void flush()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("a")), QByteArray(4096, 'a'));
        QVERIFY(flushFileSystem(dir.path()));
        QVERIFY(!flushFileSystem(dir.filePath(QStringLiteral("missing"))));
    }

    // A copy whose destination is treated as a USB drive: the card stays,
    // says it is finishing the writing, then goes once the flush is done.
    void cardWaitsForTheDrive()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("src/one.bin")), QByteArray(1 << 20, 'x'));
        QDir().mkpath(dir.filePath(QStringLiteral("dst")));

        JobTracker tracker;
        tracker.setShowDelay(60000);
        bool asked = false;
        tracker.setRemovableCheck([&asked](const QString &) {
            asked = true;
            return true;
        });
        bool finishingSeen = false;
        QString finishingText;
        int finishingPercent = -1;
        JobsModel *model = tracker.model();
        connect(model, &QAbstractItemModel::dataChanged, this, [&] {
            if (model->rowCount() > 0 && model->data(model->index(0), JobsModel::FinishingRole).toBool() && !finishingSeen) {
                finishingSeen = true;
                finishingText = model->data(model->index(0), JobsModel::PercentTextRole).toString();
                finishingPercent = model->data(model->index(0), JobsModel::PercentRole).toInt();
            }
        });

        auto *job = KIO::copy(QUrl::fromLocalFile(dir.filePath(QStringLiteral("src/one.bin"))),
                              QUrl::fromLocalFile(dir.filePath(QStringLiteral("dst"))),
                              KIO::HideProgressInfo);
        tracker.registerJob(job);
        QCOMPARE(model->rowCount(), 1);
        QVERIFY(asked);
        QVERIFY(job->exec());
        // the job is over: the card said it was finishing the writing,
        // below 100 %, and goes once the drive has everything
        QVERIFY(finishingSeen);
        QVERIFY2(finishingText.startsWith(QLatin1String("Finishing writing")), qPrintable(finishingText));
        QVERIFY(finishingPercent < 100);
        QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(), 0, 10000);
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("dst/one.bin"))));
    }

    // Anything else ends as before: no finishing step.
    void otherCopiesEndAtOnce()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("src/one.bin")), QByteArray(1024, 'x'));
        QDir().mkpath(dir.filePath(QStringLiteral("dst")));
        JobTracker tracker;
        tracker.setShowDelay(60000);
        tracker.setRemovableCheck([](const QString &) {
            return false;
        });
        auto *job = KIO::copy(QUrl::fromLocalFile(dir.filePath(QStringLiteral("src/one.bin"))),
                              QUrl::fromLocalFile(dir.filePath(QStringLiteral("dst"))),
                              KIO::HideProgressInfo);
        tracker.registerJob(job);
        QVERIFY(job->exec());
        QVERIFY(!tracker.model()->rowCount() || !tracker.model()->data(tracker.model()->index(0), JobsModel::FinishingRole).toBool());
        QTRY_COMPARE(tracker.model()->rowCount(), 0);
    }
};

QTEST_MAIN(WritebackTest)
#include "test_writeback.moc"
