/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "uiexport.h"

#include "tokens.h"

#include <QPointer>
#include <QWidget>

class QPainter;

namespace Raye::Ui
{

// The Raye keyboard focus ring (theming spec 3.12): 2 px focus.outer and
// 1 px focus.inner, 3 px outside the control, radius = control radius + 3,
// and a 2 px glow outside it at night. Only for keyboard focus, never on a
// click, like Windows.
//
// A widget cannot paint outside itself, so the ring is a small transparent
// sibling laid over the control's edge. attach() is all a control does.
class RAYE_UI_EXPORT FocusRing : public QWidget
{
    Q_OBJECT
public:
    // Shows a ring around control whenever it gets keyboard focus.
    static FocusRing *attach(QWidget *control, qreal controlRadius = Metrics::radiusControl);
    // The ring of a control, if attach() was called on it.
    static FocusRing *of(const QWidget *control);
    // Removes it again (a control drawn inside a bigger one that carries
    // the ring, like the main part of a SplitButton).
    static void detach(QWidget *control);

    // Paints a ring around controlRect (in painter coordinates).
    static void paint(QPainter *painter, const QRectF &controlRect, qreal controlRadius, const TokenSet &tokens);
    // How far the ring and its glow reach outside the control.
    static int reach();
    static bool isKeyboardReason(Qt::FocusReason reason);

    QWidget *control() const
    {
        return m_control;
    }
    void setControlRadius(qreal radius);
    // Shows the ring now (a control can also have focus from code that
    // should still show it, e.g. a flyout opened from the keyboard).
    void showRing();
    void hideRing();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    FocusRing(QWidget *control, qreal controlRadius);
    void follow();

    QPointer<QWidget> m_control;
    qreal m_radius;
    bool m_wanted = false;
};

}
