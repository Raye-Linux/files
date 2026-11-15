/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "separator.h"
#include "tokens.h"

#include <QPainter>

namespace Raye::Ui
{

Separator::Separator(Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent)
    , m_orientation(Qt::Vertical)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setOrientation(orientation);
    connect(Tokens::instance(), &Tokens::changed, this, qOverload<>(&QWidget::update));
}

void Separator::setOrientation(Qt::Orientation orientation)
{
    m_orientation = orientation;
    if (orientation == Qt::Vertical) {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    } else {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    updateGeometry();
    update();
}

QSize Separator::sizeHint() const
{
    return m_orientation == Qt::Vertical ? QSize(1, Metrics::separatorLength) : QSize(Metrics::separatorLength, 1);
}

QSize Separator::minimumSizeHint() const
{
    return m_orientation == Qt::Vertical ? QSize(1, 1) : QSize(1, 1);
}

void Separator::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QColor color = Tokens::current().color(TokenSet::strokeDivider);
    if (m_orientation == Qt::Vertical) {
        const int length = qMin(height(), Metrics::separatorLength);
        painter.fillRect(QRect(0, (height() - length) / 2, 1, length), color);
    } else {
        painter.fillRect(QRect(0, (height() - 1) / 2, width(), 1), color);
    }
}

}
