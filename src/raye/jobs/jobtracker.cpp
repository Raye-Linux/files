/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "jobtracker.h"

#include "jobsmodel.h"
#include "progresswindow.h"

#include <KJob>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
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
    showSoon(job);
}

void JobTracker::unregisterJob(KJob *job)
{
    KJobTrackerInterface::unregisterJob(job);
    const int i = m_model->rowOf(job);
    if (i >= 0) {
        m_model->removeRow(i);
    }
    maybeClose();
}

void JobTracker::showSoon(KJob *job)
{
    QPointer<KJob> guard(job);
    QTimer::singleShot(m_showDelay, this, [this, guard] {
        if (!guard) {
            return;
        }
        const int i = m_model->rowOf(guard);
        if (i >= 0 && !m_model->row(i)->done) {
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
    JobsModel::Row *r = m_model->row(i);
    r->done = true;
    r->percent = 100;
    r->job = nullptr;
    m_model->touch(i);
    // The success check plays for 250 ms, then the card goes.
    const quint64 id = r->id;
    QTimer::singleShot(m_window && m_window->isVisible() ? 400 : 0, this, [this, id] {
        m_model->removeRow(m_model->rowOfId(id));
        maybeClose();
    });
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
