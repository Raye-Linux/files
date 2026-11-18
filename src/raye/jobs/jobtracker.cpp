/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "jobtracker.h"

#include "jobsmodel.h"
#include "progresswindow.h"
#include "writeback.h"

#include <KJob>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <QUrl>

namespace Raye::Jobs
{

JobTracker *JobTracker::instance()
{
    static QPointer<JobTracker> tracker;
    if (!tracker) {
        tracker = new JobTracker(qApp);
    }
    return tracker;
}

JobTracker::JobTracker(QObject *parent)
    : KJobTrackerInterface(parent)
    , m_model(new JobsModel(this))
{
    // Four times a second at most: the taskbar does not need more.
    m_launcherTimer.setInterval(250);
    m_launcherTimer.setSingleShot(true);
    connect(&m_launcherTimer, &QTimer::timeout, this, &JobTracker::updateLauncher);
    connect(m_model, &JobsModel::overallChanged, this, [this] {
        if (!m_launcherTimer.isActive()) {
            m_launcherTimer.start();
        }
    });
    m_removable = isOnRemovableDrive;
    m_writebackTimer.setInterval(250);
    connect(&m_writebackTimer, &QTimer::timeout, this, &JobTracker::updateWriteback);
}

JobTracker::~JobTracker()
{
    if (m_launcherVisible) {
        sendLauncherProgress(0, false);
    }
}

ProgressWindow *JobTracker::window()
{
    if (!m_window) {
        m_window = new ProgressWindow(m_model);
        m_window->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    return m_window;
}

void JobTracker::registerJob(KJob *job)
{
    if (!job || m_model->rowOf(job) >= 0) {
        return;
    }
    KJobTrackerInterface::registerJob(job);
    m_model->addJob(job);
    watchWriteback(m_model->rowOf(job));
    showSoon(job);
}

void JobTracker::watchWriteback(int i)
{
    JobsModel::Row *r = m_model->row(i);
    if (!r || (r->op != Operation::Copy && r->op != Operation::Move) || !r->to.isLocalFile() || !m_removable) {
        return;
    }
    // the destination may be a name that does not exist yet
    QString path = r->to.toLocalFile();
    while (!path.isEmpty() && !QFileInfo::exists(path) && path != QLatin1String("/")) {
        path = QFileInfo(path).path();
    }
    if (path.isEmpty() || !m_removable(path)) {
        return;
    }
    r->flushes = true;
    r->flushPath = path;
    r->pendingBase = pendingWriteBytes();
    if (!m_writebackTimer.isActive()) {
        m_writebackTimer.start();
    }
}

void JobTracker::updateWriteback()
{
    const qulonglong now = pendingWriteBytes();
    bool any = false;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        JobsModel::Row *r = m_model->row(i);
        if (!r->flushes || r->done) {
            continue;
        }
        any = true;
        r->pending = now > r->pendingBase ? now - r->pendingBase : 0;
        if (r->bytesTotal > 0) {
            const qulonglong done = r->finishing ? r->bytesTotal : r->bytesDone;
            // never backwards: other programs write too
            r->percent = qMax(r->percent, writtenPercent(r->bytesTotal, done, r->pending));
        }
        m_model->touch(i);
    }
    if (!any) {
        m_writebackTimer.stop();
    }
}

void JobTracker::flushThenFinish(quint64 id, const QString &path)
{
    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, id] {
        watcher->deleteLater();
        JobsModel::Row *r = m_model->row(m_model->rowOfId(id));
        if (!r) {
            return;
        }
        r->finishing = false;
        r->done = true;
        r->percent = 100;
        m_model->touch(m_model->rowOfId(id));
        removeSoon(id);
    });
    watcher->setFuture(QtConcurrent::run(flushFileSystem, path));
}

void JobTracker::removeSoon(quint64 id)
{
    // The success check plays for 250 ms, then the card goes.
    QTimer::singleShot(m_window && m_window->isVisible() ? 400 : 0, this, [this, id] {
        m_model->removeRow(m_model->rowOfId(id));
        maybeClose();
    });
}

void JobTracker::unregisterJob(KJob *job)
{
    KJobTrackerInterface::unregisterJob(job);
    // KJob::finished reaches this slot before finished(): a copy to a USB
    // drive keeps its card here
    if (startFinishing(job)) {
        return;
    }
    const int i = m_model->rowOf(job);
    if (i >= 0) {
        m_model->removeRow(i);
    }
    maybeClose();
}

bool JobTracker::startFinishing(KJob *job)
{
    const int i = m_model->rowOf(job);
    JobsModel::Row *r = m_model->row(i);
    if (!r || !r->flushes || job->error() != 0) {
        return false;
    }
    // KIO is done, the drive is not: say so until it has everything.
    r->job = nullptr;
    r->finishing = true;
    r->bytesDone = r->bytesTotal;
    r->itemsDone = r->itemsTotal;
    r->percent = qMin(r->percent, 99);
    m_model->touch(i);
    updateWriteback();
    flushThenFinish(r->id, r->flushPath);
    return true;
}

void JobTracker::showSoon(KJob *job)
{
    // by row: a copy to a USB drive outlives its job while the drive writes
    const int at = m_model->rowOf(job);
    if (at < 0) {
        return;
    }
    const quint64 id = m_model->row(at)->id;
    QTimer::singleShot(m_showDelay, this, [this, id] {
        const JobsModel::Row *r = m_model->row(m_model->rowOfId(id));
        if (r && !r->done) {
            window()->showOperations();
        }
    });
}

void JobTracker::finished(KJob *job)
{
    const int i = m_model->rowOf(job);
    if (i < 0) {
        return;
    }
    if (startFinishing(job)) {
        return;
    }
    JobsModel::Row *r = m_model->row(i);
    r->job = nullptr;
    r->done = true;
    r->percent = 100;
    m_model->touch(i);
    removeSoon(r->id);
}

void JobTracker::maybeClose()
{
    if (m_model->rowCount() == 0 && m_window) {
        m_window->hide();
    }
}

void JobTracker::suspended(KJob *job)
{
    const int i = m_model->rowOf(job);
    if (i >= 0) {
        m_model->row(i)->paused = true;
        m_model->touch(i);
    }
}

void JobTracker::resumed(KJob *job)
{
    const int i = m_model->rowOf(job);
    if (i >= 0) {
        m_model->row(i)->paused = false;
        m_model->touch(i);
    }
}

void JobTracker::description(KJob *job, const QString &title, const QPair<QString, QString> &field1, const QPair<QString, QString> &field2)
{
    Q_UNUSED(field2)
    const int i = m_model->rowOf(job);
    if (i < 0) {
        return;
    }
    JobsModel::Row *r = m_model->row(i);
    r->otherTitle = title;
    // KIO puts the file it is working on in the first field.
    const QUrl current = QUrl::fromUserInput(field1.second);
    const QString name = current.isValid() ? current.fileName() : QString();
    if (!name.isEmpty()) {
        r->currentName = name;
    }
    m_model->touch(i);
}

void JobTracker::totalAmount(KJob *job, KJob::Unit unit, qulonglong amount)
{
    const int i = m_model->rowOf(job);
    if (i < 0) {
        return;
    }
    JobsModel::Row *r = m_model->row(i);
    if (unit == KJob::Bytes) {
        r->bytesTotal = amount;
    } else if (unit == KJob::Files || unit == KJob::Directories || unit == KJob::Items) {
        r->itemsTotal = job->totalAmount(KJob::Files) + job->totalAmount(KJob::Directories);
        if (r->itemsTotal == 0) {
            r->itemsTotal = job->totalAmount(KJob::Items);
        }
    }
    m_model->touch(i);
}

void JobTracker::processedAmount(KJob *job, KJob::Unit unit, qulonglong amount)
{
    const int i = m_model->rowOf(job);
    if (i < 0) {
        return;
    }
    JobsModel::Row *r = m_model->row(i);
    if (unit == KJob::Bytes) {
        r->bytesDone = amount;
    } else {
        r->itemsDone = job->processedAmount(KJob::Files) + job->processedAmount(KJob::Directories);
        if (r->itemsDone == 0) {
            r->itemsDone = job->processedAmount(KJob::Items);
        }
    }
    m_model->touch(i);
}

void JobTracker::percent(KJob *job, unsigned long value)
{
    const int i = m_model->rowOf(job);
    // a copy to a USB drive counts what is on the drive (updateWriteback)
    if (i >= 0 && m_model->row(i)->flushes && m_model->row(i)->bytesTotal > 0) {
        return;
    }
    if (i >= 0) {
        m_model->row(i)->percent = int(qMin<unsigned long>(value, 100));
        m_model->touch(i);
    }
}

void JobTracker::updateLauncher()
{
    const bool visible = m_model->rowCount() > 0;
    if (!visible && !m_launcherVisible) {
        return;
    }
    m_launcherVisible = visible;
    sendLauncherProgress(m_model->overallPercent() / 100.0, visible);
}

void sendLauncherProgress(qreal progress, bool visible)
{
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/org/kde/dolphin/UnityLauncher"),
                                                      QStringLiteral("com.canonical.Unity.LauncherEntry"),
                                                      QStringLiteral("Update"));
    QVariantMap properties;
    properties.insert(QStringLiteral("progress"), qBound(0.0, progress, 1.0));
    properties.insert(QStringLiteral("progress-visible"), visible);
    message << QStringLiteral("application://org.kde.dolphin.desktop") << properties;
    QDBusConnection::sessionBus().send(message);
}

}
