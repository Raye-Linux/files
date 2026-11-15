/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "uiexport.h"

#include <QImage>
#include <QPointer>
#include <QWidget>

class QGraphicsOpacityEffect;
class QVBoxLayout;
class QVariantAnimation;
class KWindowShadow;

namespace Raye::Ui
{

// The base of every Raye popup in Files that is not a plain menu: the Share
// flyout, AutoPlay, the address bar drop-downs, search options. Acrylic
// (acrylic.tint over a blur where the compositor offers one, solid
// otherwise), radius 8, 1 px stroke.surface, the flyout shadow, 4 px of
// padding. It opens 4 px from what it belongs to, below it when there is
// room, and comes in with the Raye popup motion: a 6 px slide and fade,
// 167 ms; it leaves in 83 ms. Esc and a click outside close it.
class RAYE_UI_EXPORT Flyout : public QWidget
{
    Q_OBJECT
public:
    explicit Flyout(QWidget *parent = nullptr);
    ~Flyout() override;

    QWidget *contentWidget() const
    {
        return m_content;
    }
    // The flyout takes ownership.
    void setContentWidget(QWidget *content);

    int padding() const
    {
        return m_padding;
    }
    void setPadding(int padding);

    // Translucent acrylic needs a compositor; without one the flyout is
    // solid with square corners. The render harness forces it on to draw
    // what a Raye session shows.
    bool translucent() const
    {
        return m_translucent;
    }
    void setTranslucent(bool translucent);

    // Opens next to anchor: under it (Qt::BottomEdge) or above it
    // (Qt::TopEdge), flipping when the screen has no room, left or right
    // aligned (Qt::AlignLeft, Qt::AlignRight, Qt::AlignHCenter).
    void showAt(QWidget *anchor, Qt::Edge edge = Qt::BottomEdge, Qt::Alignment alignment = Qt::AlignLeft);
    void showAt(const QRect &anchorGlobal, Qt::Edge edge = Qt::BottomEdge, Qt::Alignment alignment = Qt::AlignLeft);
    // Closes with the exit motion.
    void dismiss();

    // Where the flyout goes for an anchor, its size and the screen's
    // available area; also says from which edge it opened.
    struct Placement {
        QRect geometry;
        Qt::Edge openedFrom;
    };
    static Placement place(const QRect &anchor, const QSize &size, const QRect &screen, Qt::Edge edge, Qt::Alignment alignment);

    // The flyout shadow (elevation tokens) for a surface of this size, with
    // the surface itself cut out. Margin: how far it reaches out.
    static QImage shadowImage(const QSize &surface, bool night, int *margin = nullptr);

    QSize sizeHint() const override;

Q_SIGNALS:
    void aboutToShow();
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyWindowEffects();
    void animateIn();

    QVBoxLayout *m_layout;
    QPointer<QWidget> m_content;
    int m_padding;
    bool m_translucent;
    bool m_closing = false;
    bool m_keyboardOpened = false;
    Qt::Edge m_openedFrom = Qt::BottomEdge;
    qreal m_progress = 1.0;
    QPointer<QVariantAnimation> m_animation;
    QPointer<QGraphicsOpacityEffect> m_fade;
    KWindowShadow *m_shadow = nullptr;
};

}
