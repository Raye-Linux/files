/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "motion.h"

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QVariantAnimation>

namespace Raye::Ui
{

namespace
{
qreal s_pinnedFactor = -1;

qreal userFactor()
{
    const KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kdeglobals"), KConfig::FullConfig), QStringLiteral("KDE"));
    return qMax(0.0, group.readEntry("AnimationDurationFactor", 1.0));
}

// Follows the user's setting while Files runs.
struct Watch {
    Watch()
    {
        watcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kdeglobals"), KConfig::FullConfig));
        QObject::connect(watcher.data(), &KConfigWatcher::configChanged, Motion::notifier(), [](const KConfigGroup &group, const QByteArrayList &names) {
            if (group.name() == QLatin1String("KDE") && names.contains("AnimationDurationFactor") && s_pinnedFactor < 0) {
                Q_EMIT Motion::notifier()->speedChanged();
            }
        });
    }
    KConfigWatcher::Ptr watcher;
};
}

namespace Motion
{

Notifier *notifier()
{
    // Lives as long as the process: widgets connect to it at any time.
    static Notifier *n = new Notifier;
    return n;
}

qreal speedFactor()
{
    if (s_pinnedFactor >= 0) {
        return s_pinnedFactor;
    }
    static Watch watch;
    return userFactor();
}

void setSpeedFactor(qreal factor)
{
    s_pinnedFactor = factor;
    Q_EMIT notifier()->speedChanged();
}

bool enabled()
{
    return speedFactor() > 0;
}

int baseDuration(Speed speed)
{
    switch (speed) {
    case Faster:
    case ExitShort:
        return 83;
    case Fast:
    case Exit:
        return 167;
    case Normal:
        return 250;
    case Slow:
        return 333;
    }
    return 0;
}

int duration(Speed speed)
{
    const qreal factor = speedFactor();
    if (factor <= 0) {
        return 0;
    }
    return qRound(baseDuration(speed) * factor);
}

QEasingCurve easing(Speed speed)
{
    switch (speed) {
    case Faster:
    case ExitShort:
        return QEasingCurve(QEasingCurve::Linear);
    case Fast:
        return QEasingCurve(QEasingCurve::OutCubic);
    case Normal:
    case Slow:
        return QEasingCurve(QEasingCurve::OutQuint);
    case Exit:
        return QEasingCurve(QEasingCurve::InCubic);
    }
    return QEasingCurve(QEasingCurve::Linear);
}

QVariantAnimation *run(QObject *owner, Speed speed, const std::function<void(qreal)> &step, const std::function<void()> &done)
{
    const int ms = duration(speed);
    if (ms <= 0 || !owner) {
        step(1.0);
        if (done) {
            done();
        }
        return nullptr;
    }
    auto *animation = new QVariantAnimation(owner);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setDuration(ms);
    animation->setEasingCurve(easing(speed));
    QObject::connect(animation, &QVariantAnimation::valueChanged, owner, [step](const QVariant &value) {
        step(value.toReal());
    });
    QObject::connect(animation, &QVariantAnimation::finished, owner, [done]() {
        if (done) {
            done();
        }
    });
    step(0.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    return animation;
}

}

ColorFade::ColorFade(QObject *parent, std::function<void()> update)
    : QObject(parent)
    , m_update(std::move(update))
{
}

void ColorFade::jumpTo(const QColor &color)
{
    if (m_animation) {
        m_animation->stop();
    }
    m_from = m_to = m_value = color;
    if (m_update) {
        m_update();
    }
}

void ColorFade::setTarget(const QColor &color, Motion::Speed speed)
{
    if (color == m_to && m_value.isValid()) {
        return;
    }
    if (!m_value.isValid() || Motion::duration(speed) <= 0) {
        jumpTo(color);
        return;
    }
    if (m_animation) {
        m_animation->stop();
    }
    m_from = m_value;
    m_to = color;
    m_animation = Motion::run(this, speed, [this](qreal t) {
        // Premultiplied steps: a fade from transparent does not go grey.
        const QColor a = m_from.toRgb();
        const QColor b = m_to.toRgb();
        const qreal alpha = a.alphaF() + (b.alphaF() - a.alphaF()) * t;
        auto ch = [&](qreal x, qreal xa, qreal y, qreal ya) {
            const qreal pre = x * xa + (y * ya - x * xa) * t;
            return alpha > 0 ? qBound(0.0, pre / alpha, 1.0) : y;
        };
        m_value = QColor::fromRgbF(ch(a.redF(), a.alphaF(), b.redF(), b.alphaF()),
                                   ch(a.greenF(), a.alphaF(), b.greenF(), b.alphaF()),
                                   ch(a.blueF(), a.alphaF(), b.blueF(), b.alphaF()),
                                   alpha);
        if (m_update) {
            m_update();
        }
    });
}

}
