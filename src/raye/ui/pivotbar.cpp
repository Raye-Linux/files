/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "pivotbar.h"
#include "focusring.h"
#include "motion.h"
#include "tokens.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QVariantAnimation>

namespace Raye::Ui
{

PivotItem::PivotItem(const QString &text, PivotBar *bar)
    : QAbstractButton(bar)
    , m_bar(bar)
{
    setText(text);
    setCheckable(true);
    setAutoExclusive(true);
    setFont(textFont(TextStyle::BodyStrong));
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    FocusRing::attach(this);
}

QSize PivotItem::sizeHint() const
{
    return QSize(QFontMetrics(font()).horizontalAdvance(text()) + 2 * Metrics::pivotPadding, Metrics::pivotHeight);
}

QSize PivotItem::minimumSizeHint() const
{
    return sizeHint();
}

void PivotItem::paintEvent(QPaintEvent *)
{
    const TokenSet &t = Tokens::current();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // Hover and press fill inside the item, 4 px above and below.
    const QRectF fill = QRectF(rect()).adjusted(0, 4, 0, -4);
    if (isEnabled() && (isDown() || m_hovered)) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(isDown() ? t.color(TokenSet::fillSubtlePressed) : t.color(TokenSet::fillSubtleHover));
        painter.drawRoundedRect(fill, Metrics::radiusControl, Metrics::radiusControl);
    }
    QColor ink = t.color(TokenSet::textSecondary);
    if (!isEnabled()) {
        ink = t.color(TokenSet::textDisabled);
    } else if (isChecked() || m_hovered) {
        ink = t.color(TokenSet::textPrimary);
    }
    painter.setPen(ink);
    painter.setFont(font());
    painter.drawText(rect(), Qt::AlignCenter | Qt::TextSingleLine, text());
}

void PivotItem::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void PivotItem::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void PivotItem::keyPressEvent(QKeyEvent *event)
{
    const int here = m_bar->m_items.indexOf(this);
    int next = -1;
    const bool rtl = isRightToLeft();
    switch (event->key()) {
    case Qt::Key_Left:
        next = rtl ? here + 1 : here - 1;
        break;
    case Qt::Key_Right:
        next = rtl ? here - 1 : here + 1;
        break;
    case Qt::Key_Home:
        next = 0;
        break;
    case Qt::Key_End:
        next = m_bar->count() - 1;
        break;
    default:
        QAbstractButton::keyPressEvent(event);
        return;
    }
    event->accept();
    if (next < 0 || next >= m_bar->count()) {
        return;
    }
    m_bar->setCurrentIndex(next);
    m_bar->item(next)->setFocus(Qt::TabFocusReason);
}

PivotBar::PivotBar(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QHBoxLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(4);
    m_layout->addStretch();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFixedHeight(Metrics::pivotHeight);
    connect(Tokens::instance(), &Tokens::changed, this, qOverload<>(&QWidget::update));
}

int PivotBar::addItem(const QString &text)
{
    auto *item = new PivotItem(text, this);
    const int index = m_items.size();
    m_items.append(item);
    m_layout->insertWidget(index, item);
    connect(item, &QAbstractButton::clicked, this, [this, item]() {
        setCurrentIndex(m_items.indexOf(item));
    });
    item->setAccessibleName(text);
    if (m_current < 0) {
        setCurrentIndex(0);
    } else {
        // Only the selected item is a Tab stop; arrows move between them.
        item->setFocusPolicy(Qt::NoFocus);
    }
    return index;
}

PivotItem *PivotBar::item(int index) const
{
    return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
}

QString PivotBar::itemText(int index) const
{
    const PivotItem *i = item(index);
    return i ? i->text() : QString();
}

void PivotBar::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_items.size() || index == m_current) {
        return;
    }
    const bool first = m_current < 0;
    m_current = index;
    for (int i = 0; i < m_items.size(); ++i) {
        m_items.at(i)->setChecked(i == index);
        m_items.at(i)->setFocusPolicy(i == index ? Qt::TabFocus : Qt::NoFocus);
    }
    if (first || !isVisible()) {
        m_pill = pillTarget(index);
        update();
    } else {
        slidePill();
    }
    Q_EMIT currentChanged(index);
}

QRectF PivotBar::pillTarget(int index) const
{
    const PivotItem *i = item(index);
    if (!i) {
        return {};
    }
    const QRect g = i->geometry();
    return QRectF(g.center().x() + 0.5 - Metrics::pillWidth / 2.0, g.bottom() + 1 - 4 - Metrics::pillHeight, Metrics::pillWidth, Metrics::pillHeight);
}

QRectF PivotBar::pillRect() const
{
    return m_pill;
}

void PivotBar::slidePill()
{
    if (m_slide) {
        m_slide->stop();
    }
    const QRectF from = m_pill;
    const int index = m_current;
    m_slide = Motion::run(this, Motion::Fast, [this, from, index](qreal t) {
        const QRectF to = pillTarget(index);
        m_pill = QRectF(from.x() + (to.x() - from.x()) * t, to.y(), to.width(), to.height());
        update();
    });
}

void PivotBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_slide) {
        m_pill = pillTarget(m_current);
    }
}

bool PivotBar::event(QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Show) {
        const bool handled = QWidget::event(event);
        if (!m_slide) {
            m_pill = pillTarget(m_current);
            update();
        }
        return handled;
    }
    return QWidget::event(event);
}

void PivotBar::paintEvent(QPaintEvent *)
{
    if (m_pill.isEmpty()) {
        return;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(isEnabled() ? Tokens::current().color(TokenSet::accent) : Tokens::current().color(TokenSet::accentDisabled));
    const qreal radius = Metrics::pillHeight / 2.0;
    painter.drawRoundedRect(m_pill, radius, radius);
}

}
