/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "flyout.h"
#include "motion.h"
#include "shadow.h"
#include "tokens.h"

#include <KWindowEffects>

#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWindow>

namespace Raye::Ui
{

Flyout::Flyout(QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_layout(new QVBoxLayout(this))
    , m_padding(Metrics::flyoutPadding)
    , m_translucent(compositorActive())
{
    setAttribute(Qt::WA_TranslucentBackground, m_translucent);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setObjectName(QStringLiteral("RayeFlyout"));
    m_layout->setContentsMargins(m_padding, m_padding, m_padding, m_padding);
    m_layout->setSpacing(0);
    // A popup is exactly as big as its content asks; nothing stretches it.
    m_layout->setSizeConstraint(QLayout::SetFixedSize);
    connect(Tokens::instance(), &Tokens::changed, this, [this]() {
        // Night and Day have different shadows.
        if (isVisible()) {
            applyWindowEffects();
        }
        update();
    });
}

Flyout::~Flyout() = default;

void Flyout::setContentWidget(QWidget *content)
{
    if (m_content == content) {
        return;
    }
    if (m_content) {
        m_layout->removeWidget(m_content);
        m_content->deleteLater();
    }
    m_content = content;
    if (content) {
        content->setParent(this);
        m_layout->addWidget(content);
        setFocusProxy(nullptr);
    }
}

void Flyout::setPadding(int padding)
{
    m_padding = qMax(0, padding);
    m_layout->setContentsMargins(m_padding, m_padding, m_padding, m_padding);
}

void Flyout::setTranslucent(bool translucent)
{
    m_translucent = translucent;
    setAttribute(Qt::WA_TranslucentBackground, translucent);
    update();
}

QSize Flyout::sizeHint() const
{
    return QWidget::sizeHint().expandedTo(QSize(2 * Metrics::radiusOverlay, 2 * Metrics::radiusOverlay));
}

Flyout::Placement Flyout::place(const QRect &anchor, const QSize &size, const QRect &screen, Qt::Edge edge, Qt::Alignment alignment)
{
    const int gap = Metrics::flyoutGap;
    const bool vertical = edge == Qt::BottomEdge || edge == Qt::TopEdge;
    QRect r(QPoint(), size);
    Qt::Edge from = edge;

    if (vertical) {
        const int below = anchor.bottom() + 1 + gap;
        const int above = anchor.top() - gap - size.height();
        const bool fitsBelow = below + size.height() <= screen.bottom() + 1;
        const bool fitsAbove = above >= screen.top();
        bool wantBelow = edge == Qt::BottomEdge;
        if (wantBelow && !fitsBelow && fitsAbove) {
            wantBelow = false;
        } else if (!wantBelow && !fitsAbove && fitsBelow) {
            wantBelow = true;
        }
        from = wantBelow ? Qt::BottomEdge : Qt::TopEdge;
        r.moveTop(wantBelow ? below : above);

        if (alignment & Qt::AlignRight) {
            r.moveRight(anchor.right());
        } else if (alignment & Qt::AlignHCenter) {
            r.moveLeft(anchor.center().x() - size.width() / 2);
        } else {
            r.moveLeft(anchor.left());
        }
    } else {
        const int right = anchor.right() + 1 + gap;
        const int left = anchor.left() - gap - size.width();
        bool wantRight = edge == Qt::RightEdge;
        if (wantRight && right + size.width() > screen.right() + 1 && left >= screen.left()) {
            wantRight = false;
        } else if (!wantRight && left < screen.left() && right + size.width() <= screen.right() + 1) {
            wantRight = true;
        }
        from = wantRight ? Qt::RightEdge : Qt::LeftEdge;
        r.moveLeft(wantRight ? right : left);
        if (alignment & Qt::AlignBottom) {
            r.moveBottom(anchor.bottom());
        } else if (alignment & Qt::AlignVCenter) {
            r.moveTop(anchor.center().y() - size.height() / 2);
        } else {
            r.moveTop(anchor.top());
        }
    }

    // Never off screen: slide back in, the left/top edge winning.
    if (r.right() > screen.right()) {
        r.moveRight(screen.right());
    }
    if (r.left() < screen.left()) {
        r.moveLeft(screen.left());
    }
    if (r.bottom() > screen.bottom()) {
        r.moveBottom(screen.bottom());
    }
    if (r.top() < screen.top()) {
        r.moveTop(screen.top());
    }
    return {r, from};
}

void Flyout::showAt(QWidget *anchor, Qt::Edge edge, Qt::Alignment alignment)
{
    if (!anchor) {
        return;
    }
    // A flyout opened with the keyboard puts the focus ring on its first
    // control; one opened with the mouse does not.
    m_keyboardOpened = anchor->hasFocus() && QGuiApplication::mouseButtons() == Qt::NoButton;
    const QRect global(anchor->mapToGlobal(QPoint(0, 0)), anchor->size());
    showAt(global, edge, alignment);
}

void Flyout::showAt(const QRect &anchorGlobal, Qt::Edge edge, Qt::Alignment alignment)
{
    Q_EMIT aboutToShow();
    ensurePolished();
    m_layout->activate();
    resize(sizeHint());
    QScreen *screen = QGuiApplication::screenAt(anchorGlobal.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect area = screen ? screen->availableGeometry() : QRect(anchorGlobal.topLeft() - QPoint(4096, 4096), QSize(8192, 8192));
    const Placement placement = place(anchorGlobal, size(), area, edge, alignment);
    m_openedFrom = placement.openedFrom;
    m_closing = false;
    setGeometry(placement.geometry);
    show();
    animateIn();
}

void Flyout::animateIn()
{
    if (m_animation) {
        m_animation->stop();
    }
    if (m_content && Motion::duration(Motion::Fast) > 0) {
        m_fade = new QGraphicsOpacityEffect(m_content);
        m_content->setGraphicsEffect(m_fade);
    }
    m_animation = Motion::run(
        this,
        Motion::Fast,
        [this](qreal t) {
            m_progress = t;
            // The content comes in from the anchor's side, 6 px.
            const int slide = qRound((1 - t) * Metrics::flyoutSlide);
            int top = m_padding;
            int bottom = m_padding;
            if (m_openedFrom == Qt::BottomEdge) {
                top -= slide;
                bottom += slide;
            } else if (m_openedFrom == Qt::TopEdge) {
                top += slide;
                bottom -= slide;
            }
            m_layout->setContentsMargins(m_padding, top, m_padding, bottom);
            if (m_fade) {
                m_fade->setOpacity(t);
            }
            update();
        },
        [this]() {
            m_progress = 1;
            m_layout->setContentsMargins(m_padding, m_padding, m_padding, m_padding);
            if (m_content && m_fade) {
                // Drawing through an effect costs: only while it moves.
                m_content->setGraphicsEffect(nullptr);
            }
            update();
        });
}

void Flyout::dismiss()
{
    if (!isVisible() || m_closing) {
        return;
    }
    m_closing = true;
    if (m_animation) {
        m_animation->stop();
    }
    const qreal start = m_progress;
    setWindowOpacity(1.0);
    m_animation = Motion::run(
        this,
        Motion::ExitShort,
        [this, start](qreal t) {
            m_progress = start * (1 - t);
            setWindowOpacity(m_progress);
        },
        [this]() {
            hide();
            setWindowOpacity(1.0);
            m_progress = 1;
            m_closing = false;
        });
}

void Flyout::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        dismiss();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void Flyout::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    applyWindowEffects();
    if (m_content) {
        QWidget *first = m_content->focusPolicy() != Qt::NoFocus ? m_content.data() : m_content->nextInFocusChain();
        while (first && first != m_content && !m_content->isAncestorOf(first)) {
            first = first->nextInFocusChain();
        }
        if (first && first->focusPolicy() != Qt::NoFocus) {
            first->setFocus(m_keyboardOpened ? Qt::TabFocusReason : Qt::PopupFocusReason);
        }
    }
}

void Flyout::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_keyboardOpened = false;
    Q_EMIT closed();
}

void Flyout::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (isVisible()) {
        applyWindowEffects();
    }
}

void Flyout::applyWindowEffects()
{
    QWindow *window = windowHandle();
    if (!window || !m_translucent) {
        return;
    }
    // Blur behind the rounded surface only.
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), Metrics::radiusOverlay, Metrics::radiusOverlay);
    KWindowEffects::enableBlurBehind(window, true, QRegion(path.toFillPolygon().toPolygon()));

    // The compositor draws the shadow outside the window.
    m_shadow = Shadow::apply(this, Metrics::radiusOverlay, Elevation::Flyout, m_shadow);
}

QImage Flyout::shadowImage(const QSize &surface, bool night, int *margin)
{
    if (margin) {
        *margin = Shadow::margin(Elevation::Flyout, night);
    }
    return Shadow::image(surface, Metrics::radiusOverlay, Elevation::Flyout, night);
}

void Flyout::paintEvent(QPaintEvent *)
{
    const TokenSet &t = Tokens::current();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!m_translucent) {
        // No compositor: a solid square surface with its outline.
        painter.fillRect(rect(), t.color(TokenSet::acrylicTintSolid));
        painter.setPen(QPen(t.color(TokenSet::strokeSurfaceSolid), 1));
        painter.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
        return;
    }
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    const QRectF surface = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = Metrics::radiusOverlay;
    painter.setPen(QPen(t.color(TokenSet::strokeSurface), 1));
    painter.setBrush(t.color(TokenSet::acrylicTint));
    painter.drawRoundedRect(surface, radius - 0.5, radius - 0.5);
}

}
