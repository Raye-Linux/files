/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

#include <raye/archive/options.h>

#include <atomic>

namespace Raye::Archive
{

// Lives in the main thread; the job's thread emits on it (queued to the
// job) and reads the kill flag. Shared between the job and the thread, so
// a job deleted while its thread still finishes leaves nothing dangling.
class JobRelay : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    std::atomic_bool killed{false};
    // Pause and cancel reach the engine through this.
    raye::archive::Control control;

Q_SIGNALS:
    void totalKnown(qint64 total);
    void progressed(qint64 done);
    void done();
};

}
