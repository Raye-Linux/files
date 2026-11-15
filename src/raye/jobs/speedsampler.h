/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <deque>
#include <functional>

class KJob;

namespace Raye::Jobs
{

// The speed of a copy, from the bytes it has done so far (explorer spec
// 3.13). Samples are cumulative byte counts with a time stamp; the speed is
// the slope over the last few seconds, so a job that reports in bursts
// still reads as a steady line. The graph gets one point per 250 ms.
class DOLPHIN_EXPORT SpeedSampler
{
public:
    static constexpr int bucketMs = 250;

    explicit SpeedSampler(int windowMs = 3000, int historySize = 160);

    void reset();
    // processed is cumulative; a smaller value than the last one means the
    // job started over, and the sampler with it.
    void addSample(qint64 timeMs, qulonglong processed);
    // Nothing happens to the bytes while paused: the speed reads 0 and the
    // time left is unknown. Resuming starts a fresh window, so the pause is
    // not averaged into the speed that follows.
    void setPaused(bool paused, qint64 timeMs);
    bool paused() const
    {
        return m_paused;
    }

    void setTotal(qulonglong total)
    {
        m_total = total;
    }
    qulonglong total() const
    {
        return m_total;
    }
    qulonglong processed() const
    {
        return m_processed;
    }

    // Bytes per second; 0 when nothing moved for a whole window.
    qreal speed() const;
    // The highest point of the graph.
    qreal peak() const;
    // Oldest first, one value per bucket, at most historySize.
    QList<qreal> history() const
    {
        return m_history;
    }
    // Milliseconds left at the current speed; -1 when it can't be told.
    qint64 remainingMs() const;

private:
    struct Sample {
        qint64 time;
        qulonglong processed;
    };
    void trim(qint64 now);
    void record(qint64 now);

    int m_window;
    int m_historySize;
    std::deque<Sample> m_samples;
    QList<qreal> m_history;
    qint64 m_lastBucket = -1;
    qint64 m_lastTime = 0;
    qulonglong m_processed = 0;
    qulonglong m_total = 0;
    bool m_paused = false;
};

// Feeds a SpeedSampler from a KJob's processed bytes, four times a second,
// and follows suspend/resume. The clock can be swapped for tests.
class DOLPHIN_EXPORT JobSpeedProbe : public QObject
{
    Q_OBJECT
public:
    using Clock = std::function<qint64()>;

    explicit JobSpeedProbe(KJob *job, QObject *parent = nullptr);

    void setClock(const Clock &clock);
    // Takes one sample now; the timer calls it on its own.
    void sample();

    const SpeedSampler &sampler() const
    {
        return m_sampler;
    }

Q_SIGNALS:
    void sampled();

private:
    qint64 now() const;

    QPointer<KJob> m_job;
    SpeedSampler m_sampler;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    Clock m_clock;
};

// "112 MB/s", "4.2 KB/s"; "About 2 minutes left"-style durations.
DOLPHIN_EXPORT QString speedText(qreal bytesPerSecond);
DOLPHIN_EXPORT QString remainingText(qint64 ms);

}
