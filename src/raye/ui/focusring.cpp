/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "focusring.h"

#include <QEvent>
#include <QFocusEvent>
#include <QPainter>
#include <QPainterPath>

#include <utility>

namespace Raye::Ui
{

namespace
{
const char s_property[] = "_raye_focus_ring";
}

FocusRing::FocusRing(QWidget *control, qreal controlRadius)
    : QWidget(control->parentWidget())
    , m_control(control)
    , m_radius(controlRadius)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    hide();
    control->installEventFilter(this);
    // Not owned by the control (it is the control's sibling): leave with it.
    connect(control, &QObject::destroyed, this, &QObject::deleteLater);
    connect(Tokens::instance(), &Tokens::changed, this, qOverload<>(&QWidget::update));
}

FocusRing *FocusRing::attach(QWidget *control, qreal controlRadius)
{
    if (!control) {
        return nullptr;
    }
    if (FocusRing *existing = of(control)) {
        existing->setControlRadius(controlRadius);
        return existing;
    }
    // A control with no parent yet gets its ring now; the ring moves into
    // the parent when the control is put somewhere (ParentChange below).
    auto *ring = new FocusRing(control, controlRadius);
    control->setProperty(s_property, QVariant::fromValue(static_cast<QObject *>(ring)));
    return ring;
}

FocusRing *FocusRing::of(const QWidget *control)
{
    return control ? qobject_cast<FocusRing *>(control->property(s_property).value<QObject *>()) : nullptr;
}

void FocusRing::detach(QWidget *control)
{
    FocusRing *ring = of(control);
    if (!ring) {
        return;
    }
    control->removeEventFilter(ring);
    control->setProperty(s_property, QVariant());
    delete ring;
}

int FocusRing::reach()
{
    return Metrics::focusMargin + Metrics::focusGlow;
}

bool FocusRing::isKeyboardReason(Qt::FocusReason reason)
{
    return reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason || reason == Qt::ShortcutFocusReason;
}

void FocusRing::setControlRadius(qreal radius)
{
    m_radius = radius;
    update();
}

void FocusRing::showRing()
{
    m_wanted = true;
    follow();
}

void FocusRing::hideRing()
{
    m_wanted = false;
    hide();
}

void FocusRing::follow()
{
    if (!m_control || !m_wanted || !m_control->isVisible() || !parentWidget() || m_control->parentWidget() != parentWidget()) {
        hide();
        return;
    }
    const int r = reach();
    setGeometry(m_control->geometry().adjusted(-r, -r, r, r));
    raise();
    show();
    update();
}

bool FocusRing::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_control) {
        switch (event->type()) {
        case QEvent::FocusIn:
            if (isKeyboardReason(static_cast<QFocusEvent *>(event)->reason())) {
                showRing();
            }
            break;
        case QEvent::FocusOut:
            hideRing();
            break;
        case QEvent::ParentChange:
            if (m_control->parentWidget() != parentWidget()) {
                // A top-level control has no room around it to draw in.
                setParent(m_control->parentWidget());
                hide();
            }
            if (m_wanted) {
                follow();
            }
            break;
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::Hide:
            if (m_wanted) {
                follow();
            }
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FocusRing::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const int r = reach();
    paint(&painter, QRectF(rect()).adjusted(r, r, -r, -r), m_radius, Tokens::current());
}

void FocusRing::paint(QPainter *painter, const QRectF &controlRect, qreal controlRadius, const TokenSet &tokens)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);

    // From the control's edge outwards: 1 px inner ring, 2 px outer ring
    // (3 px in all), then the glow. Each stroke is centred on its band.
    const auto band = [&](qreal centre) {
        return std::pair{controlRect.adjusted(-centre, -centre, centre, centre), controlRadius + centre};
    };
    if (tokens.night()) {
        const auto [rect, radius] = band(Metrics::focusMargin + Metrics::focusGlow / 2.0);
        painter->setPen(QPen(tokens.color(TokenSet::glow), Metrics::focusGlow));
        painter->drawRoundedRect(rect, radius, radius);
    }
    {
        const auto [rect, radius] = band(Metrics::focusInner + Metrics::focusOuter / 2.0);
        painter->setPen(QPen(tokens.color(TokenSet::focusOuter), Metrics::focusOuter));
        painter->drawRoundedRect(rect, radius, radius);
    }
    {
        const auto [rect, radius] = band(Metrics::focusInner / 2.0);
        painter->setPen(QPen(tokens.color(TokenSet::focusInner), Metrics::focusInner));
        painter->drawRoundedRect(rect, radius, radius);
    }
    painter->restore();
}

}
