/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "uiexport.h"

#include <QAbstractButton>
#include <QList>
#include <QPointer>
#include <QWidget>

class QHBoxLayout;
class QVariantAnimation;

namespace Raye::Ui
{

class PivotBar;

// One view of a PivotBar. A button, so screen readers and Tab treat it like
// one; the bar draws the underline.
class RAYE_UI_EXPORT PivotItem : public QAbstractButton
{
    Q_OBJECT
public:
    explicit PivotItem(const QString &text, PivotBar *bar);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    PivotBar *m_bar;
    bool m_hovered = false;
};

// A row of views to pick from (explorer spec 3.9, 3.15: Home's
// "Recent | Favorites", the Properties tabs): Body strong labels 40 tall,
// the selected one in primary text over a 3 px accent underline that slides
// to it in 167 ms. Left, Right, Home and End move the selection.
class RAYE_UI_EXPORT PivotBar : public QWidget
{
    Q_OBJECT
public:
    explicit PivotBar(QWidget *parent = nullptr);

    int addItem(const QString &text);
    int count() const
    {
        return m_items.size();
    }
    PivotItem *item(int index) const;
    QString itemText(int index) const;

    int currentIndex() const
    {
        return m_current;
    }
    void setCurrentIndex(int index);

    // Where the underline is drawn now (moves while it slides).
    QRectF pillRect() const;

Q_SIGNALS:
    void currentChanged(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;

private:
    friend class PivotItem;
    QRectF pillTarget(int index) const;
    void slidePill();

    QHBoxLayout *m_layout;
    QList<PivotItem *> m_items;
    int m_current = -1;
    QRectF m_pill;
    QPointer<QVariantAnimation> m_slide;
};

}
