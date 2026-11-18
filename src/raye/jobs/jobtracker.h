/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <KJobTrackerInterface>

#include <QPointer>
#include <QTimer>

#include <functional>

namespace Raye::Jobs
{

class JobsModel;
class ProgressWindow;

// Files' own job tracker (explorer spec 3.13): KIO jobs of this process go
// to the Raye progress window instead of the notification area. A job
// shows only once it has run for a second; the window closes when the
// last one is done. The taskbar button shows the overall progress through
// the Unity launcher entry the Raye taskbar reads.
class DOLPHIN_EXPORT JobTracker : public KJobTrackerInterface
{
    Q_OBJECT
public:
    static JobTracker *instance();

    explicit JobTracker(QObject *parent = nullptr);
    ~JobTracker() override;

    JobsModel *model() const
    {
        return m_model;
    }
    // The window, made on first use.
    ProgressWindow *window();
    // How long a job runs before the window opens (1 s; 0 in tests).
    void setShowDelay(int ms)
    {
        m_showDelay = ms;
    }
    // Whether a local folder is on a USB drive or memory card: copies there
    // wait for the drive before they end. isOnRemovableDrive by default.
    void setRemovableCheck(const std::function<bool(const QString &)> &check)
    {
        m_removable = check;
    }

public Q_SLOTS:
    void registerJob(KJob *job) override;
    void unregisterJob(KJob *job) override;

protected Q_SLOTS:
    void finished(KJob *job) override;
    void suspended(KJob *job) override;
    void resumed(KJob *job) override;
    void description(KJob *job, const QString &title, const QPair<QString, QString> &field1, const QPair<QString, QString> &field2) override;
    void totalAmount(KJob *job, KJob::Unit unit, qulonglong amount) override;
    void processedAmount(KJob *job, KJob::Unit unit, qulonglong amount) override;
    void percent(KJob *job, unsigned long percent) override;

private:
    void showSoon(KJob *job);
    void updateLauncher();
    void maybeClose();
    void watchWriteback(int row);
    bool startFinishing(KJob *job);
    void updateWriteback();
    void flushThenFinish(quint64 id, const QString &path);
    void removeSoon(quint64 id);

    JobsModel *m_model;
    QPointer<ProgressWindow> m_window;
    int m_showDelay = 1000;
    QTimer m_launcherTimer;
    bool m_launcherVisible = false;
    std::function<bool(const QString &)> m_removable;
    QTimer m_writebackTimer;
};

// The Unity launcher entry signal for org.kde.dolphin.desktop.
DOLPHIN_EXPORT void sendLauncherProgress(qreal progress, bool visible);

}
