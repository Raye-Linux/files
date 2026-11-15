/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The speed sampler of the progress window (explorer spec 3.13): steady,
// bursty, paused and stuck jobs, then the probe on a fake KJob.

#include "raye/jobs/speedsampler.h"

#include <KJob>

#include <QSignalSpy>
#include <QTest>

using Raye::Jobs::JobSpeedProbe;
using Raye::Jobs::SpeedSampler;

namespace
{
constexpr qulonglong MB = 1024 * 1024;

// A job that does nothing by itself: the test moves its counters.
class FakeJob : public KJob
{
    Q_OBJECT
public:
    FakeJob()
    {
        setCapabilities(KJob::Suspendable | KJob::Killable);
    }
    void start() override
    {
    }
    void setDone(qulonglong bytes)
    {
        setProcessedAmount(KJob::Bytes, bytes);
    }
    void setSize(qulonglong bytes)
    {
        setTotalAmount(KJob::Bytes, bytes);
    }

protected:
    bool doSuspend() override
    {
        return true;
    }
    bool doResume() override
    {
        return true;
    }
};
}

class TestSpeedSampler : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void steady();
    void bursty();
    void paused();
    void stuck();
    void startsOver();
    void historyIsBounded();
    void gapsHoldTheLine();
    void probeFollowsTheJob();
    void texts();
};

void TestSpeedSampler::steady()
{
    SpeedSampler s;
    s.setTotal(100 * MB);
    // 10 MB/s, a sample every 250 ms for 5 s
    for (qint64 t = 0; t <= 5000; t += 250) {
        s.addSample(t, qulonglong(t) * 10 * MB / 1000);
    }
    QVERIFY(qAbs(s.speed() - 10.0 * MB) < 0.01 * 10 * MB);
    // 50 MB done, 50 to go at 10 MB/s
    QVERIFY(qAbs(s.remainingMs() - 5000) < 250);
    QCOMPARE(s.history().size(), 21);
    QVERIFY(qAbs(s.history().last() - 10.0 * MB) < 0.01 * 10 * MB);
    QVERIFY(s.peak() >= s.history().last());
}

void TestSpeedSampler::bursty()
{
    SpeedSampler s;
    // 4 MB in one report each second, nothing in between
    qulonglong done = 0;
    for (qint64 t = 0; t <= 8000; t += 250) {
        if (t > 0 && t % 1000 == 0) {
            done += 4 * MB;
        }
        s.addSample(t, done);
    }
    QVERIFY2(qAbs(s.speed() - 4.0 * MB) < 0.25 * 4 * MB, qPrintable(QString::number(s.speed() / MB)));
    // the graph stays near the true rate once the window is full
    const QList<qreal> h = s.history();
    for (int i = 16; i < h.size(); ++i) {
        QVERIFY2(h.at(i) < 2 * 4.0 * MB, qPrintable(QString::number(h.at(i) / MB)));
        QVERIFY2(h.at(i) > 0.5 * 4.0 * MB, qPrintable(QString::number(h.at(i) / MB)));
    }
}

void TestSpeedSampler::paused()
{
    SpeedSampler s;
    s.setTotal(100 * MB);
    for (qint64 t = 0; t <= 3000; t += 250) {
        s.addSample(t, qulonglong(t) * 10 * MB / 1000);
    }
    QVERIFY(s.speed() > 9.0 * MB);
    s.setPaused(true, 3000);
    QVERIFY(s.paused());
    QCOMPARE(s.speed(), 0.0);
    QCOMPARE(s.remainingMs(), qint64(-1));
    // ten seconds of pause: the bytes stay where they are
    for (qint64 t = 3250; t <= 13000; t += 250) {
        s.addSample(t, 30 * MB);
    }
    QCOMPARE(s.history().last(), 0.0);
    s.setPaused(false, 13000);
    // half a second after resuming at 10 MB/s the speed is 10, not an
    // average dragged down by the pause
    s.addSample(13250, 30 * MB + 2621440);
    s.addSample(13500, 30 * MB + 5242880);
    QVERIFY2(qAbs(s.speed() - 10.0 * MB) < 0.05 * 10 * MB, qPrintable(QString::number(s.speed() / MB)));
    QVERIFY(s.remainingMs() > 0);
}

void TestSpeedSampler::stuck()
{
    SpeedSampler s;
    for (qint64 t = 0; t <= 2000; t += 250) {
        s.addSample(t, qulonglong(t) * MB / 1000);
    }
    QVERIFY(s.speed() > 0);
    // nothing moves for more than the window
    for (qint64 t = 2250; t <= 6000; t += 250) {
        s.addSample(t, 2 * MB);
    }
    QCOMPARE(s.speed(), 0.0);
    s.setTotal(10 * MB);
    QCOMPARE(s.remainingMs(), qint64(-1));
}

void TestSpeedSampler::startsOver()
{
    SpeedSampler s;
    s.addSample(0, 0);
    s.addSample(1000, 50 * MB);
    const int points = s.history().size();
    // a retried file: the count goes back
    s.addSample(1250, 1 * MB);
    QCOMPARE(s.processed(), 1 * MB);
    QCOMPARE(s.speed(), 0.0);
    QVERIFY(s.history().size() >= points);
    s.addSample(1500, 2 * MB);
    QVERIFY(s.speed() > 0);
}

void TestSpeedSampler::historyIsBounded()
{
    SpeedSampler s(3000, 40);
    for (qint64 t = 0; t <= 60000; t += 250) {
        s.addSample(t, qulonglong(t) * MB);
    }
    QCOMPARE(s.history().size(), 40);
}

void TestSpeedSampler::gapsHoldTheLine()
{
    SpeedSampler s;
    s.addSample(0, 0);
    s.addSample(250, MB);
    s.addSample(500, 2 * MB);
    const qreal before = s.history().last();
    // a second with no sample, then one more
    s.addSample(1500, 6 * MB);
    const QList<qreal> h = s.history();
    QCOMPARE(h.size(), 7);
    QCOMPARE(h.at(3), before);
    QCOMPARE(h.at(5), before);
}

void TestSpeedSampler::probeFollowsTheJob()
{
    FakeJob job;
    JobSpeedProbe probe(&job);
    qint64 clock = 0;
    probe.setClock([&clock] {
        return clock;
    });
    job.setSize(40 * MB);
    for (int i = 0; i <= 8; ++i) {
        clock = i * 250;
        job.setDone(qulonglong(i) * MB);
        probe.sample();
    }
    QCOMPARE(probe.sampler().total(), 40 * MB);
    QVERIFY(qAbs(probe.sampler().speed() - 4.0 * MB) < 0.01 * 4 * MB);

    QSignalSpy spy(&probe, &JobSpeedProbe::sampled);
    QVERIFY(job.suspend());
    QVERIFY(probe.sampler().paused());
    QCOMPARE(probe.sampler().speed(), 0.0);
    QVERIFY(job.resume());
    QVERIFY(!probe.sampler().paused());
    QCOMPARE(spy.count(), 2);

    // the timer samples by itself
    QSignalSpy timed(&probe, &JobSpeedProbe::sampled);
    QVERIFY(timed.wait(1000));
}

void TestSpeedSampler::texts()
{
    QVERIFY(Raye::Jobs::speedText(0).isEmpty());
    const QString fast = Raye::Jobs::speedText(112.0 * MB);
    QVERIFY2(fast.contains(QLatin1String("112")) && fast.endsWith(QLatin1String("/s")), qPrintable(fast));
    QVERIFY(fast.contains(QLatin1String("MB")));
    QCOMPARE(Raye::Jobs::remainingText(-1), QStringLiteral("Calculating…"));
    QCOMPARE(Raye::Jobs::remainingText(3000), QStringLiteral("A few seconds"));
    QCOMPARE(Raye::Jobs::remainingText(22000), QStringLiteral("About 25 seconds"));
    QCOMPARE(Raye::Jobs::remainingText(130000), QStringLiteral("About 2 minutes"));
    QCOMPARE(Raye::Jobs::remainingText(3600000), QStringLiteral("About 1 hour"));
    QCOMPARE(Raye::Jobs::remainingText(3600000 + 20 * 60000), QStringLiteral("About 1 hour and 20 minutes"));
}

QTEST_GUILESS_MAIN(TestSpeedSampler)

#include "test_speedsampler.moc"
