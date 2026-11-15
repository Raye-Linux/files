/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "usagebar.h"
#include "motion.h"
#include "tokens.h"

#include <KLocalizedString>

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace Raye::Ui
{

UsageBar::UsageBar(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::NoFocus);
    connect(Tokens::instance(), &Tokens::changed, this, qOverload<>(&QWidget::update));
}

void UsageBar::setValue(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(value + 1, m_value + 1)) {
        return;
    }
    m_value = value;
    if (m_grown || !m_animate) {
        m_shown = value;
    }
    setAccessibleName(i18nc("@info accessible text of a drive usage bar", "%1 % used", qRound(value * 100)));
    update();
}

void UsageBar::setUsage(qint64 usedBytes, qint64 totalBytes)
{
    setValue(totalBytes > 0 ? qreal(qBound<qint64>(0, usedBytes, totalBytes)) / totalBytes : 0.0);
}

bool UsageBar::caution() const
{
    return m_value > Metrics::usageCautionAbove;
}

QSize UsageBar::sizeHint() const
{
    return QSize(240, Metrics::usageBarHeight);
}

QSize UsageBar::minimumSizeHint() const
{
    return QSize(Metrics::usageBarHeight * 4, Metrics::usageBarHeight);
}

void UsageBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_grown) {
        grow();
    }
}

void UsageBar::grow()
{
    m_grown = true;
    if (!m_animate) {
        m_shown = m_value;
        return;
    }
    // 400 ms at normal speed, a little longer than a page enter.
    auto *animation = Motion::run(this, Motion::Normal, [this](qreal t) {
        m_shown = m_value * t;
        update();
    });
    if (animation) {
        animation->setDuration(qRound(400 * Motion::speedFactor()));
    }
}

void UsageBar::paintEvent(QPaintEvent *)
{
    const TokenSet &t = Tokens::current();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal h = qMin<qreal>(height(), Metrics::usageBarHeight);
    const QRectF track(0, (height() - h) / 2.0, width(), h);
    const qreal radius = Metrics::radiusSmall;

    painter.setPen(Qt::NoPen);
    painter.setBrush(withAlpha(t.color(TokenSet::strokeStrong), 0.35));
    painter.drawRoundedRect(track, radius, radius);

    const qreal w = qRound(track.width() * m_shown);
    if (w <= 0) {
        return;
    }
    const QRectF used(track.left(), track.top(), qMax<qreal>(w, h), h);
    if (caution()) {
        painter.setBrush(t.color(TokenSet::statusCaution));
    } else {
        QLinearGradient gradient(track.topLeft(), track.topRight());
        gradient.setColorAt(0, t.color(TokenSet::accent));
        gradient.setColorAt(1, QColor(t.color(TokenSet::glow).rgb()));
        painter.setBrush(gradient);
    }
    painter.drawRoundedRect(used, radius, radius);
}

}
