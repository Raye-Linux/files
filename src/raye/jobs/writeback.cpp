/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "writeback.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>

#include <fcntl.h>
#include <unistd.h>

namespace Raye::Jobs
{

qulonglong pendingWriteBytes(const QByteArray &meminfo)
{
    qulonglong kib = 0;
    for (const QByteArray &line : meminfo.split('\n')) {
        if (!line.startsWith("Dirty:") && !line.startsWith("Writeback:")) {
            continue;
        }
        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() >= 2) {
            kib += parts.at(1).toULongLong();
        }
    }
    return kib * 1024;
}

qulonglong pendingWriteBytes()
{
    QFile f(QStringLiteral("/proc/meminfo"));
    if (!f.open(QIODevice::ReadOnly)) {
        return 0;
    }
    return pendingWriteBytes(f.readAll());
}

bool isRemovableDevice(const QString &device, const QString &sysRoot)
{
    if (!device.startsWith(QLatin1String("/dev/"))) {
        return false;
    }
    const QString name = QFileInfo(device).fileName();
    const QString entry = sysRoot + QStringLiteral("/class/block/") + name;
    const QString real = QFileInfo(entry).canonicalFilePath();
    if (real.isEmpty()) {
        return false;
    }
    // a partition sits in the folder of its disk
    const QString disk = QFile::exists(entry + QStringLiteral("/partition")) ? QFileInfo(real).dir().dirName() : name;
    QFile removable(sysRoot + QStringLiteral("/block/") + disk + QStringLiteral("/removable"));
    if (removable.open(QIODevice::ReadOnly) && removable.readAll().trimmed() == "1") {
        return true;
    }
    // USB hard disks and card readers say 0 there; their place on the bus tells
    return real.contains(QLatin1String("/usb")) || real.contains(QLatin1String("/mmc_host/"));
}

bool isOnRemovableDrive(const QString &path)
{
    const QStorageInfo storage(path);
    return storage.isValid() && isRemovableDevice(QString::fromLocal8Bit(storage.device()));
}

int writtenPercent(qulonglong total, qulonglong done, qulonglong pending)
{
    if (total == 0) {
        return 0;
    }
    done = qMin(done, total);
    const qulonglong written = done > pending ? done - pending : 0;
    return int(qMin<qulonglong>(99, written * 100 / total));
}

bool flushFileSystem(const QString &path)
{
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    const bool ok = ::syncfs(fd) == 0;
    ::close(fd);
    return ok;
}

}
