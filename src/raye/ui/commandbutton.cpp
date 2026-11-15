/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "commandbutton.h"
#include "focusring.h"
#include "motion.h"

#include <KLocalizedString>

#include <QAction>
#include <QEnterEvent>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

namespace Raye::Ui
{

CommandButton::CommandButton(QWidget *parent)
    : QAbstractButton(parent)
{
    init();
}

CommandButton::CommandButton(const QIcon &icon, const QString &text, QWidget *parent)
    : QAbstractButton(parent)
{
    init();
    setIcon(icon);
    setText(text);
}

CommandButton::~CommandButton() = default;

void CommandButton::init()
{
    // Tab reaches it; a click does not steal the focus from the view.
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover);
    setFont(textFont(TextStyle::Body));
    setIconSize(QSize(Metrics::iconSize, Metrics::iconSize));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_fill = new ColorFade(this, [this]() {
        update();
    });
    FocusRing::attach(this);
    connect(Tokens::instance(), &Tokens::changed, this, [this]() {
        m_fill->jumpTo(QColor());
        update();
    });
    connect(this, &QAbstractButton::clicked, this, [this]() {
        if (m_menu) {
            showMenu();
        } else if (m_action && !m_action->isCheckable()) {
            m_action->trigger();
        }
    });
}

void CommandButton::setKind(Kind kind)
{
    if (m_kind == kind) {
        return;
    }
    m_kind = kind;
    // A dialog button takes the focus like any push button.
    setFocusPolicy(kind == Subtle ? Qt::TabFocus : Qt::StrongFocus);
    m_fill->jumpTo(QColor());
    updateGeometry();
    update();
}

void CommandButton::setIconOnly(bool iconOnly)
{
    if (m_iconOnly == iconOnly) {
        return;
    }
    m_iconOnly = iconOnly;
    updateGeometry();
    update();
}

void CommandButton::setCompact(bool compact)
{
    if (m_compact == compact) {
        return;
    }
    m_compact = compact;
    updateGeometry();
    update();
}

bool CommandButton::showsChevron() const
{
    return m_chevron || m_menu;
}

void CommandButton::setShowChevron(bool show)
{
    if (m_chevron == show) {
        return;
    }
    m_chevron = show;
    updateGeometry();
    update();
}

void CommandButton::setMenu(QMenu *menu)
{
    if (m_menu == menu) {
        return;
    }
    if (m_menu) {
        disconnect(m_menu, nullptr, this, nullptr);
    }
    m_menu = menu;
    if (m_menu) {
        connect(m_menu, &QMenu::aboutToHide, this, [this]() {
            m_menuOpen = false;
            setDown(false);
            update();
        });
    }
    updateGeometry();
    update();
}

void CommandButton::showMenu()
{
    if (!m_menu || m_menuOpen) {
        return;
    }
    Q_EMIT menuAboutToShow();
    m_menuOpen = true;
    update();
    // Under the button, 4 px away, aligned on its leading edge.
    const QSize menuSize = m_menu->sizeHint();
    QPoint at = mapToGlobal(QPoint(isRightToLeft() ? width() - menuSize.width() : 0, height() + Metrics::flyoutGap));
    m_menu->popup(at);
}

void CommandButton::setShortcutHint(const QKeySequence &shortcut)
{
    m_shortcutHint = shortcut;
    setAccessibleDescription(shortcut.isEmpty() ? QString() : shortcut.toString(QKeySequence::NativeText));
}

void CommandButton::setAction(QAction *action)
{
    if (m_action == action) {
        return;
    }
    if (m_action) {
        disconnect(m_action, nullptr, this, nullptr);
    }
    m_action = action;
    if (m_action) {
        connect(m_action, &QAction::changed, this, &CommandButton::syncFromAction);
        connect(m_action, &QAction::toggled, this, &CommandButton::syncFromAction);
        syncFromAction();
    }
}

void CommandButton::syncFromAction()
{
    if (!m_action) {
        return;
    }
    // No mnemonic: Alt+letter belongs to the window, not to the bar.
    setText(KLocalizedString::removeAcceleratorMarker(m_action->text()));
    setIcon(m_action->icon());
    setEnabled(m_action->isEnabled());
    setCheckable(m_action->isCheckable());
    setChecked(m_action->isChecked());
    setShortcutHint(m_action->shortcut());
    setVisible(m_action->isVisible());
    setMenu(m_action->menu<QMenu *>());
    updateGeometry();
}

void CommandButton::setContentAlignment(Qt::Alignment alignment)
{
    m_alignment = alignment & Qt::AlignHorizontal_Mask;
    update();
}

void CommandButton::setJoin(Join join)
{
    m_join = join;
    update();
}

QString CommandButton::plainText() const
{
    return KLocalizedString::removeAcceleratorMarker(text());
}

QString CommandButton::toolTipFor(const QString &name, const QKeySequence &shortcut)
{
    if (shortcut.isEmpty()) {
        return name;
    }
    return i18nc("@info:tooltip a command and its keyboard shortcut", "%1 (%2)", name, shortcut.toString(QKeySequence::NativeText));
}

QSize CommandButton::sizeHint() const
{
    const int height = m_compact ? Metrics::compactHeight : Metrics::controlHeight;
    const bool hasIcon = !icon().isNull();
    const bool labelShown = !m_iconOnly && !text().isEmpty();
    const bool chevron = showsChevron();

    if (!labelShown) {
        if (!hasIcon && chevron) {
            return QSize(Metrics::splitChevronWidth, height);
        }
        if (!chevron) {
            return QSize(m_compact ? Metrics::controlHeight : Metrics::iconButtonWidth, height);
        }
    }
    int width = Metrics::labelPadding * 2;
    if (hasIcon) {
        width += Metrics::iconSize;
    }
    if (labelShown) {
        if (hasIcon) {
            width += Metrics::iconTextGap;
        }
        width += QFontMetrics(font()).horizontalAdvance(plainText());
    }
    if (chevron) {
        width += Metrics::iconTextGap + Metrics::chevronSize;
    }
    return QSize(width, height);
}

QSize CommandButton::minimumSizeHint() const
{
    return sizeHint();
}

CommandButton::Look CommandButton::look(Kind kind, bool enabled, bool hovered, bool pressed, bool checked, const TokenSet &t)
{
    Look look;
    switch (kind) {
    case Subtle:
        look.ink = enabled ? t.color(TokenSet::textPrimary) : t.color(TokenSet::textDisabled);
        if (!enabled) {
            look.fill = checked ? t.color(TokenSet::fillSubtlePressed) : QColor(Qt::transparent);
        } else if (pressed) {
            look.fill = t.color(TokenSet::fillSubtlePressed);
        } else if (checked) {
            look.fill = t.color(TokenSet::fillSubtleSelected);
        } else if (hovered) {
            look.fill = t.color(TokenSet::fillSubtleHover);
        } else {
            look.fill = Qt::transparent;
        }
        break;
    case Standard:
        if (checked && enabled) {
            // A toggled standard button looks like the accent one.
            return Raye::Ui::CommandButton::look(Accent, enabled, hovered, pressed, false, t);
        }
        look.border = t.color(TokenSet::strokeControl);
        if (!enabled) {
            look.fill = t.color(TokenSet::fillControlDisabled);
            look.ink = t.color(TokenSet::textDisabled);
        } else if (pressed) {
            look.fill = t.color(TokenSet::fillControlPressed);
            look.ink = t.color(TokenSet::textSecondary);
        } else {
            look.fill = hovered ? t.color(TokenSet::fillControlHover) : t.color(TokenSet::fillControl);
            look.ink = t.color(TokenSet::textPrimary);
        }
        break;
    case Accent:
        if (!enabled) {
            look.fill = t.color(TokenSet::accentDisabled);
            look.ink = withAlpha(t.color(TokenSet::textOnAccent), 0.53);
        } else {
            look.fill = pressed ? t.color(TokenSet::accentPressed) : hovered ? t.color(TokenSet::accentHover) : t.color(TokenSet::accent);
            look.ink = t.color(TokenSet::textOnAccent);
        }
        break;
    }
    return look;
}

void paintChevron(QPainter *painter, const QRectF &box, const QColor &color)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 1.2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    const QPointF c = box.center();
    const QPointF points[3] = {c + QPointF(-4, -2), c + QPointF(0, 2), c + QPointF(4, -2)};
    painter->drawPolyline(points, 3);
    painter->restore();
}

void CommandButton::paintEvent(QPaintEvent *)
{
    const TokenSet &tokens = Tokens::current();
    const bool pressed = isDown() || m_menuOpen;
    const Look l = look(m_kind, isEnabled(), m_hovered && isEnabled(), pressed, isChecked(), tokens);
    m_fill->setTarget(l.fill);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF r = rect();
    const qreal radius = Metrics::radiusControl;

    QPainterPath shape;
    shape.addRoundedRect(r, radius, radius);
    if (m_join == JoinRight) {
        shape.addRect(QRectF(r.center().x(), r.top(), r.width() / 2, r.height()));
    } else if (m_join == JoinLeft) {
        shape.addRect(QRectF(r.left(), r.top(), r.width() / 2, r.height()));
    }
    shape.setFillRule(Qt::WindingFill);
    shape = shape.simplified();

    painter.fillPath(shape, m_fill->value());

    if (l.border.isValid()) {
        painter.save();
        if (m_join == JoinLeft) {
            // The neighbour draws the shared edge.
            painter.setClipRect(r.adjusted(1, 0, 0, 0));
        }
        QPainterPath outline;
        const QRectF inner = r.adjusted(0.5, 0.5, -0.5, -0.5);
        outline.addRoundedRect(inner, radius - 0.5, radius - 0.5);
        if (m_join == JoinRight) {
            outline.addRect(QRectF(inner.center().x(), inner.top(), inner.width() / 2, inner.height()));
        } else if (m_join == JoinLeft) {
            outline.addRect(QRectF(inner.left(), inner.top(), inner.width() / 2, inner.height()));
        }
        outline.setFillRule(Qt::WindingFill);
        painter.strokePath(outline.simplified(), QPen(l.border, 1));
        painter.restore();
        if (!pressed && isEnabled()) {
            // The bottom pixel row is a shade stronger (Windows' "elevation").
            const qreal left = m_join == JoinLeft ? r.left() : r.left() + radius;
            const qreal right = m_join == JoinRight ? r.right() : r.right() - radius;
            painter.setPen(QPen(tokens.color(TokenSet::strokeControlBottom), 1));
            painter.drawLine(QPointF(left, r.bottom() - 0.5), QPointF(right, r.bottom() - 0.5));
        }
    } else if (m_kind == Accent && isEnabled()) {
        // The Raye lit edge: a 1 px highlight inside the top of the fill.
        painter.setPen(QPen(tokens.color(TokenSet::edgeLit), 1));
        const qreal left = m_join == JoinLeft ? r.left() : r.left() + radius;
        const qreal right = m_join == JoinRight ? r.right() : r.right() - radius;
        painter.drawLine(QPointF(left, r.top() + 0.5), QPointF(right, r.top() + 0.5));
    }

    // Content: icon, label, chevron, centred as one block.
    const bool hasIcon = !icon().isNull();
    const bool labelShown = !m_iconOnly && !text().isEmpty();
    const bool chevron = showsChevron();
    const QFontMetrics metrics(font());
    const QString label = plainText();

    int contentWidth = 0;
    if (hasIcon) {
        contentWidth += Metrics::iconSize;
    }
    int labelWidth = 0;
    if (labelShown) {
        labelWidth = metrics.horizontalAdvance(label);
        contentWidth += (hasIcon ? Metrics::iconTextGap : 0) + labelWidth;
    }
    if (chevron) {
        contentWidth += (contentWidth > 0 ? Metrics::iconTextGap : 0) + Metrics::chevronSize;
    }
    const int room = width() - (labelShown ? 2 * Metrics::labelPadding : 0);
    if (labelShown && contentWidth > room) {
        // Too narrow: the label gives way, never the icon or the chevron.
        labelWidth = qMax(0, labelWidth - (contentWidth - room));
        contentWidth = room;
    }

    // Leading edge for list rows (mirrored right to left), centred otherwise.
    qreal x = (width() - contentWidth) / 2.0;
    if (m_alignment & Qt::AlignLeft) {
        x = isRightToLeft() ? width() - Metrics::labelPadding - contentWidth : Metrics::labelPadding;
    }
    const qreal midY = height() / 2.0;
    if (hasIcon) {
        QIcon::Mode mode = QIcon::Normal;
        if (!isEnabled()) {
            mode = QIcon::Disabled;
        } else if (m_kind == Accent || (m_kind == Standard && isChecked())) {
            mode = QIcon::Selected;
        }
        const QPixmap pixmap = icon().pixmap(iconSize(), devicePixelRatioF(), mode, isChecked() ? QIcon::On : QIcon::Off);
        painter.drawPixmap(QPointF(qRound(x), qRound(midY - Metrics::iconSize / 2.0)), pixmap);
        x += Metrics::iconSize;
    }
    if (labelShown) {
        if (hasIcon) {
            x += Metrics::iconTextGap;
        }
        painter.setPen(l.ink);
        painter.setFont(font());
        // Elide only what does not fit: elidedText() measures a little wider
        // than horizontalAdvance() with fractional advances, and would cut
        // a label that has all the room it asked for.
        const bool fits = labelWidth >= metrics.horizontalAdvance(label);
        const QString shown = fits ? label : metrics.elidedText(label, Qt::ElideRight, labelWidth);
        painter.drawText(QRectF(x, 0, labelWidth + (fits ? 2 : 0), height()), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, shown);
        x += labelWidth;
    }
    if (chevron) {
        if (hasIcon || labelShown) {
            x += Metrics::iconTextGap;
        }
        paintChevron(&painter, QRectF(x, midY - Metrics::chevronSize / 2.0, Metrics::chevronSize, Metrics::chevronSize), l.ink);
    }
}

void CommandButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void CommandButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void CommandButton::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange || event->type() == QEvent::FontChange) {
        updateGeometry();
        update();
    }
    QAbstractButton::changeEvent(event);
}

void CommandButton::keyPressEvent(QKeyEvent *event)
{
    const bool plain = event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::KeypadModifier;
    if (plain && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        animateClick();
        event->accept();
        return;
    }
    if (m_menu && (event->key() == Qt::Key_Down || event->key() == Qt::Key_F4)) {
        showMenu();
        if (m_menu && !m_menu->actions().isEmpty()) {
            m_menu->setActiveAction(m_menu->actions().constFirst());
        }
        event->accept();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

void CommandButton::checkStateSet()
{
    update();
}

void CommandButton::nextCheckState()
{
    if (m_action && m_action->isCheckable()) {
        m_action->trigger();
        setChecked(m_action->isChecked());
        return;
    }
    QAbstractButton::nextCheckState();
}

bool CommandButton::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip && toolTip().isEmpty()) {
        const QString name = plainText();
        if (name.isEmpty()) {
            event->ignore();
            return true;
        }
        auto *help = static_cast<QHelpEvent *>(event);
        QToolTip::showText(help->globalPos(), toolTipFor(name, m_shortcutHint), this);
        return true;
    }
    return QAbstractButton::event(event);
}

}
