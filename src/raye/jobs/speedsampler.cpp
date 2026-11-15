/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "speedsampler.h"

#include <KFormat>
#include <KJob>
#include <KLocalizedString>

#include <algorithm>
#include <cmath>

namespace Raye::Jobs
{

SpeedSampler::SpeedSampler(int windowMs, int historySize)
    : m_window(qMax(bucketMs * 2, windowMs))
    , m_historySize(qMax(2, historySize))
{
}

void SpeedSampler::reset()
{
    m_samples.clear();
    m_history.clear();
    m_lastBucket = -1;
    m_lastTime = 0;
    m_processed = 0;
    m_paused = false;
}

void SpeedSampler::addSample(qint64 timeMs, qulonglong processed)
{
    if (!m_samples.empty() && (processed < m_samples.back().processed || timeMs < m_samples.back().time)) {
        // The job started over (a retried file) or the clock went back.
        const QList<qreal> keep = m_history;
        reset();
        m_history = keep;
    }
    m_processed = processed;
    m_lastTime = timeMs;
    if (m_paused) {
        record(timeMs);
        return;
    }
    m_samples.push_back({timeMs, processed});
    trim(timeMs);
    record(timeMs);
}

void SpeedSampler::setPaused(bool paused, qint64 timeMs)
{
    if (paused == m_paused) {
        return;
    }
    m_paused = paused;
    m_samples.clear();
    if (!paused) {
        m_samples.push_back({timeMs, m_processed});
    }
    m_lastTime = timeMs;
    record(timeMs);
}

void SpeedSampler::trim(qint64 now)
{
    // Keep one sample at or before the start of the window, so the slope
    // always spans the whole window once there is that much history.
    while (m_samples.size() > 2 && m_samples[1].time <= now - m_window) {
        m_samples.pop_front();
    }
}

qreal SpeedSampler::speed() const
{
    if (m_paused || m_samples.size() < 2) {
        return 0;
    }
    // The window starts at the last sample at or before its edge, so a job
    // that stopped moving a whole window ago reads 0, not slow.
    const Sample &last = m_samples.back();
    const Sample &first = m_samples.front();
    const qint64 span = qMax<qint64>(last.time - first.time, bucketMs);
    return qreal(last.processed - first.processed) * 1000.0 / qreal(span);
}

qreal SpeedSampler::peak() const
{
    qreal top = 0;
    for (qreal v : m_history) {
        top = std::max(top, v);
    }
    return top;
}

qint64 SpeedSampler::remainingMs() const
{
    const qreal s = speed();
    if (m_paused || m_total == 0 || s <= 0) {
        return -1;
    }
    if (m_processed >= m_total) {
        return 0;
    }
    return qint64(std::llround(qreal(m_total - m_processed) * 1000.0 / s));
}

void SpeedSampler::record(qint64 now)
{
    const qint64 bucket = now / bucketMs;
    const qreal value = speed();
    if (m_lastBucket < 0) {
        m_history.append(value);
    } else if (bucket > m_lastBucket) {
        // A gap between samples: the graph holds the last value over it.
        const qint64 missing = std::min<qint64>(bucket - m_lastBucket - 1, m_historySize);
        const qreal held = m_history.isEmpty() ? value : m_history.last();
        for (qint64 i = 0; i < missing; ++i) {
            m_history.append(m_paused ? 0 : held);
        }
        m_history.append(value);
    } else if (!m_history.isEmpty()) {
        m_history.last() = value;
    }
    m_lastBucket = bucket;
    while (m_history.size() > m_historySize) {
        m_history.removeFirst();
    }
}

JobSpeedProbe::JobSpeedProbe(KJob *job, QObject *parent)
    : QObject(parent)
    , m_job(job)
{
    m_elapsed.start();
    m_timer.setInterval(SpeedSampler::bucketMs);
    connect(&m_timer, &QTimer::timeout, this, &JobSpeedProbe::sample);
    if (job) {
        connect(job, &KJob::suspended, this, [this] {
            m_sampler.setPaused(true, now());
            Q_EMIT sampled();
        });
        connect(job, &KJob::resumed, this, [this] {
            m_sampler.setPaused(false, now());
            Q_EMIT sampled();
        });
        connect(job, &KJob::finished, &m_timer, &QTimer::stop);
        m_timer.start();
    }
}

void JobSpeedProbe::setClock(const Clock &clock)
{
    m_clock = clock;
}

qint64 JobSpeedProbe::now() const
{
    return m_clock ? m_clock() : m_elapsed.elapsed();
}

void JobSpeedProbe::sample()
{
    if (!m_job) {
        m_timer.stop();
        return;
    }
    m_sampler.setTotal(m_job->totalAmount(KJob::Bytes));
    m_sampler.addSample(now(), m_job->processedAmount(KJob::Bytes));
    Q_EMIT sampled();
}

QString speedText(qreal bytesPerSecond)
{
    if (bytesPerSecond <= 0) {
        return QString();
    }
    // KB, MB, GB on base 1024 with three significant digits, the way
    // Windows writes them: 4.21 MB, 42.1 MB, 112 MB.
    qreal scaled = bytesPerSecond;
    while (scaled >= 1024) {
        scaled /= 1024;
    }
    const int precision = scaled < 10 ? 2 : scaled < 100 ? 1 : 0;
    const QString size = KFormat().formatByteSize(bytesPerSecond, precision, KFormat::JEDECBinaryDialect);
    return i18nc("@info speed of a copy, %1 is like 112 MB", "%1/s", size);
}

QString remainingText(qint64 ms)
{
    if (ms < 0) {
        return i18nc("@info time left", "Calculating…");
    }
    const qint64 s = (ms + 999) / 1000;
    if (s <= 5) {
        return i18nc("@info time left", "A few seconds");
    }
    if (s < 60) {
        // Windows rounds to 5 seconds while it is under a minute.
        const qint64 r = ((s + 4) / 5) * 5;
        return i18ncp("@info time left", "About %1 second", "About %1 seconds", r);
    }
    const qint64 m = (s + 30) / 60;
    if (m < 60) {
        return i18ncp("@info time left", "About %1 minute", "About %1 minutes", m);
    }
    const qint64 h = m / 60;
    const qint64 rest = m % 60;
    if (rest == 0) {
        return i18ncp("@info time left", "About %1 hour", "About %1 hours", h);
    }
    return i18nc("@info time left, %1 hours and %2 minutes", "About %1 and %2", i18ncp("@info", "%1 hour", "%1 hours", h), i18ncp("@info", "%1 minute", "%1 minutes", rest));
}

}
