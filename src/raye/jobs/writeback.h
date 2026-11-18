/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QByteArray>
#include <QString>

namespace Raye::Jobs
{

// A copy to a USB stick "ends" when the data sits in memory; the stick gets
// it later. These say how much is still on its way, so the progress card
// shows what is really written and stays open until the drive has it.

// Dirty + Writeback of /proc/meminfo, in bytes.
DOLPHIN_EXPORT qulonglong pendingWriteBytes(const QByteArray &meminfo);
DOLPHIN_EXPORT qulonglong pendingWriteBytes();

// Whether a block device ("/dev/sdb1") is a USB drive, a memory card or
// another removable or hot-plugged disk. sysRoot is /sys outside tests.
DOLPHIN_EXPORT bool isRemovableDevice(const QString &device, const QString &sysRoot = QStringLiteral("/sys"));
// The same for the drive holding a local path.
DOLPHIN_EXPORT bool isOnRemovableDrive(const QString &path);

// What part of a copy is on the drive: done minus what still waits in
// memory, never more than 99 % until the drive has everything.
DOLPHIN_EXPORT int writtenPercent(qulonglong total, qulonglong done, qulonglong pending);

// Waits until the file system holding path has written everything
// (syncfs). Blocks: run it off the GUI thread. False if it could not.
DOLPHIN_EXPORT bool flushFileSystem(const QString &path);

}
