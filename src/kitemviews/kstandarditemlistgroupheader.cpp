/*
 * SPDX-FileCopyrightText: 2011 Peter Penz <peter.penz19@gmail.com>
 *
 * Based on the Itemviews NG project from Trolltech Labs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kstandarditemlistgroupheader.h"

#include "ui/tokens.h" // Raye

#include <KRatingPainter>
#include <QPainter>

KStandardItemListGroupHeader::KStandardItemListGroupHeader(QGraphicsWidget *parent)
    : KItemListGroupHeader(parent)
    , m_dirtyCache(true)
    , m_text()
    , m_pixmap()
{
}

KStandardItemListGroupHeader::~KStandardItemListGroupHeader()
{
}

void KStandardItemListGroupHeader::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (m_dirtyCache) {
        updateCache();
    }
    if (role() == "rating" || scrollOrientation() == Qt::Horizontal) {
        KItemListGroupHeader::paint(painter, option, widget);
        return;
    }
    // Raye: Windows' group headers: the name in Body strong, then a divider
    // running to the right edge at the middle of the text (explorer spec 3.6).
    const Raye::Ui::TokenSet &tokens = Raye::Ui::Tokens::current();
    const QFont font = Raye::Ui::textFont(Raye::Ui::TextStyle::BodyStrong);
    const QFontMetricsF metrics(font);
    const qreal margin = qMax(2, styleOption().horizontalMargin) + qMax(1, styleOption().padding) + 4;
    const qreal maxWidth = size().width() - 2 * margin;
    const QString text = metrics.elidedText(data().toString(), Qt::ElideRight, maxWidth);
    const qreal textWidth = metrics.horizontalAdvance(text);
    const QRectF textRect(layoutDirection() == Qt::LeftToRight ? margin : size().width() - margin - textWidth, 0, textWidth, size().height());

    painter->save();
    painter->setFont(font);
    painter->setPen(tokens.color(Raye::Ui::TokenSet::textPrimary));
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
    const qreal y = qRound(size().height() / 2.0) + 0.5;
    painter->setPen(QPen(tokens.color(Raye::Ui::TokenSet::strokeDivider), 1));
    if (layoutDirection() == Qt::LeftToRight) {
        if (textRect.right() + 12 < size().width() - margin) {
            painter->drawLine(QPointF(textRect.right() + 12, y), QPointF(size().width() - margin, y));
        }
    } else if (textRect.left() - 12 > margin) {
        painter->drawLine(QPointF(margin, y), QPointF(textRect.left() - 12, y));
    }
    painter->restore();
}

void KStandardItemListGroupHeader::paintRole(QPainter *painter, const QRectF &roleBounds, const QColor &color)
{
    if (m_pixmap.isNull()) {
        painter->setPen(color);
        painter->drawText(roleBounds, 0, m_text);
    } else {
        painter->drawPixmap(roleBounds.topLeft(), m_pixmap);
    }
}

void KStandardItemListGroupHeader::paintSeparator(QPainter *painter, const QColor &color)
{
    if (itemIndex() == 0) {
        // No top- or left-line should be drawn for the first group-header
        return;
    }

    painter->setPen(color);

    if (scrollOrientation() == Qt::Horizontal) {
        const qreal x = layoutDirection() == Qt::RightToLeft ? size().width() - 1 : 0;
        painter->drawLine(x, 0, x, size().height() - 1);
    } else {
        if (layoutDirection() == Qt::LeftToRight) {
            painter->drawLine(0, 0, size().width() - 1, 0);
        } else {
            painter->drawLine(1, 0, size().width(), 0);
        }
    }
}

void KStandardItemListGroupHeader::roleChanged(const QByteArray &current, const QByteArray &previous)
{
    Q_UNUSED(current)
    Q_UNUSED(previous)
    m_dirtyCache = true;
}

void KStandardItemListGroupHeader::dataChanged(const QVariant &current, const QVariant &previous)
{
    Q_UNUSED(current)
    Q_UNUSED(previous)
    m_dirtyCache = true;
}

void KStandardItemListGroupHeader::resizeEvent(QGraphicsSceneResizeEvent *event)
{
    KItemListGroupHeader::resizeEvent(event);
    m_dirtyCache = true;
}

void KStandardItemListGroupHeader::updateCache()
{
    Q_ASSERT(m_dirtyCache);
    m_dirtyCache = false;

    const qreal maxWidth = size().width() - 4 * styleOption().padding;

    if (role() == "rating") {
        m_text = QString();

        const qreal height = styleOption().fontMetrics.ascent();
        const QSizeF pixmapSize(qMin(height * 5, maxWidth), height);

        m_pixmap = QPixmap(pixmapSize.toSize());
        m_pixmap.fill(Qt::transparent);

        QPainter painter(&m_pixmap);
        const QRect rect(0, 0, m_pixmap.width(), m_pixmap.height());
        const int rating = data().toInt();
        KRatingPainter::paintRating(&painter, rect, Qt::AlignJustify | Qt::AlignVCenter, rating);
    } else {
        m_pixmap = QPixmap();

        QFontMetricsF fontMetrics(font());
        const QString text = fontMetrics.elidedText(data().toString(), Qt::ElideRight, maxWidth);
        m_text = text;
    }
}

#include "moc_kstandarditemlistgroupheader.cpp"
