/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "uiexport.h"

#include <QColor>
#include <QEasingCurve>
#include <QObject>
#include <QPointer>

#include <functional>

class QVariantAnimation;

namespace Raye::Ui
{

// Raye motion (theming spec 3.5): durations at normal speed, scaled by the
// user's animation speed (kdeglobals [KDE] AnimationDurationFactor), all
// zero when animations are off.
namespace Motion
{
enum Speed {
    Faster, // 83 ms linear: hover and press colours
    Fast, // 167 ms OutCubic: toggles, small fades, pivot underline
    Normal, // 250 ms OutQuint: flyouts, page enters
    Slow, // 333 ms OutQuint: large panels
    Exit, // 167 ms InCubic: anything leaving
    ExitShort, // 83 ms linear: popups closing
};

RAYE_UI_EXPORT int baseDuration(Speed speed);
RAYE_UI_EXPORT int duration(Speed speed);
RAYE_UI_EXPORT QEasingCurve easing(Speed speed);

// The factor in use. setSpeedFactor() pins it (tests, the render
// harness); a negative value goes back to the user's setting.
RAYE_UI_EXPORT qreal speedFactor();
RAYE_UI_EXPORT void setSpeedFactor(qreal factor);
RAYE_UI_EXPORT bool enabled();

class RAYE_UI_EXPORT Notifier : public QObject
{
    Q_OBJECT
Q_SIGNALS:
    void speedChanged();
};
RAYE_UI_EXPORT Notifier *notifier();

// Runs step(0..1) over the duration on owner, then done(). With animations
// off it calls step(1) and done() at once and returns nullptr. The
// animation dies with owner.
RAYE_UI_EXPORT QVariantAnimation *run(QObject *owner, Speed speed, const std::function<void(qreal)> &step, const std::function<void()> &done = {});
}

// A colour that moves to its target over a Raye duration: the fill of a
// button going to its hover colour, back to rest, and so on. update() is
// called on every step (usually QWidget::update).
class RAYE_UI_EXPORT ColorFade : public QObject
{
    Q_OBJECT
public:
    explicit ColorFade(QObject *parent, std::function<void()> update);

    QColor value() const
    {
        return m_value;
    }
    QColor target() const
    {
        return m_to;
    }
    // Jumps when the colour is not valid yet or speed is instant.
    void setTarget(const QColor &color, Motion::Speed speed = Motion::Faster);
    void jumpTo(const QColor &color);

private:
    std::function<void()> m_update;
    QPointer<QVariantAnimation> m_animation;
    QColor m_from;
    QColor m_to;
    QColor m_value;
};

}
