/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tabbar.h"

#include "tabwidth.h"

#include "ui/motion.h"
#include "ui/tokens.h"

#include <KLocalizedString>

#include <QAbstractButton>
#include <QApplication>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QStyleOptionTab>
#include <QToolButton>

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace Raye::Title
{
using Ui::TokenSet;
using Ui::Tokens;

namespace
{
// A tab's words as shown: the tab widget doubles "&" so it is not read as
// a mnemonic; the label shows one.
QString shownText(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('&') && i + 1 < text.size()) {
            ++i;
        }
        out.append(text.at(i));
    }
    return out;
}

// The body of a tab: its slot less the feet on both sides.
QRectF bodyOf(const QRect &slot)
{
    return QRectF(slot).adjusted(Metrics::tabFoot, 0, -Metrics::tabFoot, 0);
}

// The selected tab: rounded top corners and two small curves at the bottom
// that flow into the address row, so tab and row read as one surface.
QPainterPath selectedShape(const QRect &slot)
{
    const QRectF r(slot);
    const qreal top = r.top();
    const qreal bottom = r.top() + r.height();
    const qreal left = r.left() + Metrics::tabFoot;
    const qreal right = r.left() + r.width() - Metrics::tabFoot;
    const qreal foot = Metrics::tabFoot;
    const qreal radius = Metrics::tabTopRadius;

    QPainterPath path;
    path.moveTo(left - foot, bottom);
    path.quadTo(left, bottom, left, bottom - foot);
    path.lineTo(left, top + radius);
    path.quadTo(left, top, left + radius, top);
    path.lineTo(right - radius, top);
    path.quadTo(right, top, right, top + radius);
    path.lineTo(right, bottom - foot);
    path.quadTo(right, bottom, right + foot, bottom);
    path.closeSubpath();
    return path;
}

void drawProgressRing(QPainter *p, const QPointF &centre, qreal progress, const TokenSet &t)
{
    const qreal d = Metrics::progressRing;
    const QRectF ring(centre.x() - d / 2 + 1, centre.y() - d / 2 + 1, d - 2, d - 2);
    p->save();
    p->setBrush(Qt::NoBrush);
    QPen track(t.color(TokenSet::strokeDivider), 2);
    p->setPen(track);
    p->drawEllipse(ring);
    QPen arc(t.color(TokenSet::accentText), 2);
    arc.setCapStyle(Qt::RoundCap);
    p->setPen(arc);
    const int span = -qRound(360 * 16 * std::clamp(progress, 0.0, 1.0));
    p->drawArc(ring, 90 * 16, span);
    p->restore();
}
}

// Draws the tabs: the bar paints through its style, and so does the tab
// Qt lifts out while it is dragged, so both look the same.
class TabStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override
    {
        const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
        if (!tab) {
            QProxyStyle::drawControl(element, option, painter, widget);
            return;
        }
        switch (element) {
        case CE_TabBarTab:
            drawShape(tab, painter, widget);
            drawLabel(tab, painter, widget);
            return;
        case CE_TabBarTabShape:
            drawShape(tab, painter, widget);
            return;
        case CE_TabBarTabLabel:
            drawLabel(tab, painter, widget);
            return;
        default:
            QProxyStyle::drawControl(element, option, painter, widget);
        }
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override
    {
        switch (element) {
        case PE_FrameTabBarBase:
        case PE_IndicatorTabTearLeft:
        case PE_IndicatorTabTearRight:
            return; // no base line, no tear: the row is one surface
        case PE_PanelButtonTool:
            if (isScrollButton(widget)) {
                drawScrollPanel(option, painter);
                return;
            }
            break;
        case PE_IndicatorArrowLeft:
        case PE_IndicatorArrowRight:
            if (isScrollButton(widget)) {
                drawChevron(element == PE_IndicatorArrowLeft, option, painter);
                return;
            }
            break;
        default:
            QProxyStyle::drawPrimitive(element, option, painter, widget);
        }
    }

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const override
    {
        // The scroll arrows, drawn whole: the base style would add its bevel.
        if (control == CC_ToolButton && isScrollButton(widget)) {
            if (const auto *button = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
                drawScrollPanel(option, painter);
                if (button->arrowType == Qt::LeftArrow || button->arrowType == Qt::RightArrow) {
                    drawChevron(button->arrowType == Qt::LeftArrow, option, painter);
                }
                return;
            }
        }
        QProxyStyle::drawComplexControl(control, option, painter, widget);
    }

    QRect subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const override
    {
        switch (element) {
        case SE_TabBarTabRightButton: {
            const QRect slot = option->rect;
            const int x = slot.right() + 1 - Metrics::tabFoot - Metrics::closeMargin - Metrics::closeHit;
            const int y = slot.top() + (slot.height() - Metrics::closeHit) / 2;
            return QRect(x, y, Metrics::closeHit, Metrics::closeHit);
        }
        case SE_TabBarTabLeftButton:
            return QRect();
        default:
            return QProxyStyle::subElementRect(element, option, widget);
        }
    }

    int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override
    {
        switch (metric) {
        case PM_TabBarTabShiftHorizontal:
        case PM_TabBarTabShiftVertical:
        case PM_TabBarBaseOverlap:
        case PM_TabBarTabOverlap:
        case PM_TabBarBaseHeight:
            return 0;
        case PM_TabBarIconSize:
            return Metrics::tabIconSize;
        case PM_TabBarScrollButtonWidth:
            return Metrics::closeHit;
        default:
            return QProxyStyle::pixelMetric(metric, option, widget);
        }
    }

    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const override
    {
        switch (hint) {
        case SH_TabBar_Alignment:
            return Qt::AlignLeft;
        case SH_TabBar_CloseButtonPosition:
            return QTabBar::RightSide;
        case SH_TabBar_ElideMode:
            return Qt::ElideRight;
        case SH_Widget_Animate:
            return Ui::Motion::enabled();
        case SH_Widget_Animation_Duration:
            return Ui::Motion::duration(Ui::Motion::Fast);
        default:
            return QProxyStyle::styleHint(hint, option, widget, returnData);
        }
    }

private:
    static const TabBar *barOf(const QWidget *widget)
    {
        return qobject_cast<const TabBar *>(widget);
    }

    // The arrows Qt adds when the tabs do not fit.
    static bool isScrollButton(const QWidget *widget)
    {
        return widget && qobject_cast<const QToolButton *>(widget) && barOf(widget->parentWidget());
    }

    static void drawScrollPanel(const QStyleOption *option, QPainter *p)
    {
        const TokenSet &t = Tokens::current();
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->fillRect(option->rect, t.color(TokenSet::bgBase)); // covers the tabs scrolled under it
        if (option->state & (State_MouseOver | State_Sunken)) {
            p->setPen(Qt::NoPen);
            p->setBrush(t.color(option->state & State_Sunken ? TokenSet::fillSubtlePressed : TokenSet::fillSubtleHover));
            p->drawRoundedRect(QRectF(option->rect).adjusted(1, 4, -1, -4), Ui::Metrics::radiusControl, Ui::Metrics::radiusControl);
        }
        p->restore();
    }

    static void drawChevron(bool left, const QStyleOption *option, QPainter *p)
    {
        const TokenSet &t = Tokens::current();
        const QPointF c = QRectF(option->rect).center();
        const qreal h = 3.5;
        QPainterPath path;
        if (left) {
            path.moveTo(c.x() + h / 2, c.y() - h);
            path.lineTo(c.x() - h / 2, c.y());
            path.lineTo(c.x() + h / 2, c.y() + h);
        } else {
            path.moveTo(c.x() - h / 2, c.y() - h);
            path.lineTo(c.x() + h / 2, c.y());
            path.lineTo(c.x() - h / 2, c.y() + h);
        }
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QPen pen(t.color(option->state & State_Enabled ? TokenSet::textPrimary : TokenSet::textDisabled), 1.2);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawPath(path);
        p->restore();
    }

    void drawShape(const QStyleOptionTab *tab, QPainter *p, const QWidget *widget) const
    {
        const TokenSet &t = Tokens::current();
        const TabBar *bar = barOf(widget);
        const bool selected = tab->state & State_Selected;
        const bool moving = tab->position == QStyleOptionTab::Moving;
        const int index = tab->tabIndex;
        const bool hovered = bar ? (bar->hoveredIndex() == index && index >= 0) : bool(tab->state & State_MouseOver);

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(Qt::NoPen);
        if (selected || moving) {
            p->setBrush(t.color(TokenSet::bgLayerSolid));
            p->drawPath(selectedShape(tab->rect));
        } else if (hovered) {
            p->setBrush(t.color(TokenSet::fillSubtleHover));
            p->drawRoundedRect(bodyOf(tab->rect).adjusted(0, 2, 0, -4), Ui::Metrics::radiusControl, Ui::Metrics::radiusControl);
        }

        // A thin divider between two resting tabs, none next to the selected
        // or the hovered one.
        if (bar && !selected && !moving && !hovered && index >= 0 && index + 1 < bar->count()) {
            const int next = index + 1;
            if (next != bar->currentIndex() && next != bar->hoveredIndex()) {
                const qreal x = tab->rect.right() + 0.5;
                const qreal y = tab->rect.top() + (tab->rect.height() - Metrics::dividerHeight - 4) / 2.0;
                p->setPen(QPen(t.color(TokenSet::strokeDivider), 1));
                p->drawLine(QPointF(x, y), QPointF(x, y + Metrics::dividerHeight));
            }
        }

        // A drop over this tab: an accent line under it, with a soft glow.
        if (bar && index >= 0 && bar->dropTargetIndex() == index) {
            const QRectF body = bodyOf(tab->rect);
            const qreal bottom = tab->rect.top() + tab->rect.height();
            const QRectF line(body.left() + 8, bottom - Metrics::dropLineHeight - 1, body.width() - 16, Metrics::dropLineHeight);
            p->setPen(Qt::NoPen);
            if (t.night()) {
                p->setBrush(t.color(TokenSet::glow));
                p->drawRoundedRect(line.adjusted(-3, -3, 3, 3), 4, 4);
            }
            p->setBrush(t.color(TokenSet::accent));
            p->drawRoundedRect(line, 1, 1);
        }
        p->restore();
    }

    void drawLabel(const QStyleOptionTab *tab, QPainter *p, const QWidget *widget) const
    {
        const TokenSet &t = Tokens::current();
        const TabBar *bar = barOf(widget);
        const int index = tab->tabIndex;
        const bool selected = tab->state & State_Selected;
        const bool hovered = bar ? (bar->hoveredIndex() == index && index >= 0) : bool(tab->state & State_MouseOver);
        const QRectF body = bodyOf(tab->rect);

        p->save();
        p->setClipRect(tab->rect);
        p->setRenderHint(QPainter::Antialiasing);
        const qreal midY = tab->rect.top() + (tab->rect.height() - 2) / 2.0;

        const QRectF iconRect(body.left() + Metrics::tabPadding, midY - Metrics::tabIconSize / 2.0, Metrics::tabIconSize, Metrics::tabIconSize);
        if (!tab->icon.isNull()) {
            tab->icon.paint(p, iconRect.toRect(), Qt::AlignCenter, QIcon::Normal, QIcon::On);
        }
        const qreal progress = bar && index >= 0 ? bar->tabProgress(index) : -1;
        if (progress >= 0) {
            drawProgressRing(p, iconRect.center(), progress, t);
        }

        const qreal textLeft = iconRect.right() + Metrics::tabIconGap;
        // The label runs under the close button's place while it is not shown,
        // so narrow tabs keep more of their name.
        const bool closeShown = bar && index >= 0 && bar->isCloseButtonShown(index) && tab->position != QStyleOptionTab::Moving;
        const qreal textRight = closeShown ? body.right() - Metrics::closeMargin - Metrics::closeHit - Metrics::tabIconGap : body.right() - Metrics::tabPadding;
        if (textRight > textLeft) {
            const QFont font = widget ? widget->font() : Ui::textFont(Ui::TextStyle::Body);
            p->setFont(font);
            p->setPen(t.color(selected || hovered || tab->position == QStyleOptionTab::Moving ? TokenSet::textPrimary : TokenSet::textSecondary));
            const QFontMetricsF fm(font);
            const QString text = fm.elidedText(shownText(tab->text), Qt::ElideRight, textRight - textLeft);
            p->drawText(QRectF(textLeft, tab->rect.top(), textRight - textLeft, tab->rect.height() - 2), Qt::AlignVCenter | Qt::AlignLeft, text);
        }
        p->restore();
    }
};

namespace
{
// The close button of a tab: a small cross in a 24 px target. It stays in
// place on every tab but only shows on the selected one and the one under
// the pointer (Qt places tab buttons itself, so it is never hidden).
class TabCloseButton : public QAbstractButton
{
public:
    explicit TabCloseButton(QWidget *parent)
        : QAbstractButton(parent)
        , m_fill(this, [this] {
            update();
        })
    {
        setFixedSize(Metrics::closeHit, Metrics::closeHit);
        setFocusPolicy(Qt::NoFocus);
        setCursor(Qt::ArrowCursor);
        setToolTip(i18nc("@info:tooltip", "Close tab (Ctrl+W)"));
        setAccessibleName(i18nc("@action:button", "Close tab"));
        setAttribute(Qt::WA_Hover);
        setShown(false);
    }

    void setShown(bool shown)
    {
        if (m_shown == shown && m_initialised) {
            return;
        }
        m_initialised = true;
        m_shown = shown;
        setAttribute(Qt::WA_TransparentForMouseEvents, !shown);
        update();
    }
    bool shown() const
    {
        return m_shown;
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (!m_shown) {
            return;
        }
        const TokenSet &t = Tokens::current();
        const QColor target = isDown() ? t.color(TokenSet::fillSubtlePressed) : (underMouse() ? t.color(TokenSet::fillSubtleHover) : QColor(Qt::transparent));
        if (m_fill.target() != target) {
            m_fill.setTarget(target);
        }
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(m_fill.value().isValid() ? m_fill.value() : target);
        p.drawRoundedRect(QRectF(rect()).adjusted(2, 2, -2, -2), Ui::Metrics::radiusControl, Ui::Metrics::radiusControl);

        const qreal g = Metrics::closeGlyph;
        const QRectF glyph(width() / 2.0 - g / 2, height() / 2.0 - g / 2, g, g);
        QPen pen(t.color(TokenSet::textPrimary), 1.2);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.drawLine(glyph.topLeft(), glyph.bottomRight());
        p.drawLine(glyph.topRight(), glyph.bottomLeft());
    }

    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave) {
            update();
        }
        return QAbstractButton::event(event);
    }

private:
    Ui::ColorFade m_fill;
    bool m_shown = false;
    bool m_initialised = false;
};

// How far below or above the strip a tab must be dropped to leave it.
constexpr int detachDistance = 48;
}

TabBar::TabBar(QWidget *parent)
    : QTabBar(parent)
    , m_style(new TabStyle)
{
    setStyle(m_style);
    setFont(Ui::textFont(Ui::TextStyle::Body));
    setDrawBase(false);
    setExpanding(false);
    setDocumentMode(true);
    setElideMode(Qt::ElideRight);
    setUsesScrollButtons(true);
    setMovable(true);
    setTabsClosable(false); // Raye's own close buttons, see tabInserted()
    setIconSize(QSize(Metrics::tabIconSize, Metrics::tabIconSize));
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
    setAutoFillBackground(false);
    for (QToolButton *arrow : findChildren<QToolButton *>(Qt::FindDirectChildrenOnly)) {
        // A style set on a widget does not reach its children.
        arrow->setStyle(m_style);
        arrow->setAutoRaise(false); // so the panel, opaque over the tabs, is always drawn
        arrow->setAttribute(Qt::WA_Hover);
    }

    connect(this, &QTabBar::currentChanged, this, &TabBar::updateCloseButtons);
    connect(this, &QTabBar::tabMoved, this, &TabBar::tabsMoved);
    connect(Tokens::instance(), &Tokens::changed, this, qOverload<>(&QWidget::update));
}

TabBar::~TabBar()
{
    // The style is not a child: hand the widget back to the application's
    // style before it goes, and the scroll arrows with it.
    for (QToolButton *arrow : findChildren<QToolButton *>(Qt::FindDirectChildrenOnly)) {
        arrow->setStyle(nullptr);
    }
    setStyle(nullptr);
    delete m_style;
}

void TabBar::setAvailableWidth(int width)
{
    if (m_available == width) {
        return;
    }
    m_available = width;
    relayout();
}

int TabBar::indexOfId(quint64 id) const
{
    for (qsizetype i = 0; i < m_states.size(); ++i) {
        if (m_states.at(i).id == id) {
            return int(i);
        }
    }
    return -1;
}

void TabBar::setTabProgress(int index, qreal progress)
{
    if (index < 0 || index >= m_states.size()) {
        return;
    }
    const qreal value = progress < 0 ? -1.0 : std::clamp(progress, 0.0, 1.0);
    if (qFuzzyCompare(m_states[index].progress + 2, value + 2)) {
        return;
    }
    m_states[index].progress = value;
    update(tabRect(index));
}

qreal TabBar::tabProgress(int index) const
{
    return index >= 0 && index < m_states.size() ? m_states.at(index).progress : -1.0;
}

void TabBar::setDropTargetIndex(int index)
{
    if (index >= count()) {
        index = -1;
    }
    if (m_dropIndex == index) {
        return;
    }
    m_dropIndex = index;
    update();
}

qreal TabBar::tabScale(int index) const
{
    return index >= 0 && index < m_states.size() ? m_states.at(index).scale : 1.0;
}

int TabBar::labelWidth(int index) const
{
    return qCeil(QFontMetricsF(font()).horizontalAdvance(shownText(tabText(index))));
}

QSize TabBar::tabSizeHint(int index) const
{
    QList<int> labels;
    labels.reserve(count());
    for (int i = 0; i < count(); ++i) {
        labels.append(labelWidth(i));
    }
    const TabStripLayout layout = layoutTabs(labels, m_available);
    const int width = index >= 0 && index < layout.widths.size() ? layout.widths.at(index) : Metrics::tabMinWidth;
    return QSize(qMax(1, qRound(width * tabScale(index))), Metrics::tabHeight);
}

QSize TabBar::minimumTabSizeHint(int index) const
{
    // Never smaller than the minimum: past it the strip scrolls instead.
    return QSize(qMax(1, qRound(Metrics::tabMinWidth * tabScale(index))), Metrics::tabHeight);
}

QSize TabBar::sizeHint() const
{
    int total = 0;
    for (int i = 0; i < count(); ++i) {
        total += tabSizeHint(i).width();
    }
    if (m_available >= 0) {
        total = qMin(total, m_available);
    }
    return QSize(total, Metrics::tabHeight);
}

QSize TabBar::minimumSizeHint() const
{
    return QSize(count() > 0 ? Metrics::tabMinWidth : 0, Metrics::tabHeight);
}

void TabBar::relayout()
{
    // Qt lays its tabs out again when the elide mode is set, even to the same
    // value; nothing else of the public API does it without side effects.
    setElideMode(elideMode());
    updateGeometry();
}

void TabBar::animateScale(quint64 id, qreal from, qreal to, const std::function<void()> &done)
{
    const int index = indexOfId(id);
    if (index < 0) {
        return;
    }
    m_states[index].scale = from;
    Ui::Motion::run(
        this,
        Ui::Motion::Fast,
        [this, id, from, to](qreal t) {
            const int i = indexOfId(id);
            if (i < 0) {
                return;
            }
            m_states[i].scale = from + (to - from) * t;
            relayout();
        },
        done);
}

void TabBar::tabInserted(int index)
{
    TabState state;
    state.id = m_nextId++;
    // Tabs restored before the window shows come in at full size.
    state.scale = isVisible() ? 0.0 : 1.0;
    m_states.insert(index, state);
    if (m_hoverIndex >= index) {
        ++m_hoverIndex;
    }
    if (m_dropIndex >= index) {
        ++m_dropIndex;
    }

    auto *close = new TabCloseButton(this);
    connect(close, &QAbstractButton::clicked, this, &TabBar::closeButtonClicked);
    setTabButton(index, QTabBar::RightSide, close);

    QTabBar::tabInserted(index);
    if (state.scale < 1.0) {
        animateScale(state.id, 0.0, 1.0);
    }
    updateCloseButtons();
}

void TabBar::tabRemoved(int index)
{
    if (index >= 0 && index < m_states.size()) {
        m_states.removeAt(index);
    }
    if (m_hoverIndex == index) {
        m_hoverIndex = -1;
    } else if (m_hoverIndex > index) {
        --m_hoverIndex;
    }
    if (m_dropIndex == index) {
        m_dropIndex = -1;
    } else if (m_dropIndex > index) {
        --m_dropIndex;
    }
    QTabBar::tabRemoved(index);
    updateCloseButtons();
}

void TabBar::tabsMoved(int from, int to)
{
    if (from >= 0 && from < m_states.size() && to >= 0 && to < m_states.size()) {
        m_states.move(from, to);
    }
    updateCloseButtons();
}

bool TabBar::isCloseButtonShown(int index) const
{
    if (index < 0 || index >= m_states.size() || m_states.at(index).closing) {
        return false;
    }
    return index == currentIndex() || index == m_hoverIndex;
}

void TabBar::updateCloseButtons()
{
    for (int i = 0; i < count(); ++i) {
        if (auto *close = dynamic_cast<TabCloseButton *>(tabButton(i, QTabBar::RightSide))) {
            close->setShown(isCloseButtonShown(i));
        }
    }
}

void TabBar::closeButtonClicked()
{
    for (int i = 0; i < count(); ++i) {
        if (tabButton(i, QTabBar::RightSide) == sender()) {
            requestClose(i);
            return;
        }
    }
}

void TabBar::requestClose(int index)
{
    if (index < 0 || index >= m_states.size() || m_states.at(index).closing) {
        return;
    }
    const quint64 id = m_states.at(index).id;
    m_states[index].closing = true;
    updateCloseButtons();
    animateScale(id, m_states.at(index).scale, 0.0, [this, id] {
        const int i = indexOfId(id);
        if (i < 0) {
            return;
        }
        Q_EMIT tabCloseRequested(i);
        // Still here: the window kept it (its last tab, or the user said no).
        const int still = indexOfId(id);
        if (still >= 0) {
            m_states[still].closing = false;
            m_states[still].scale = 1.0;
            relayout();
            updateCloseButtons();
        }
    });
}

void TabBar::setHoverIndex(int index)
{
    if (m_hoverIndex == index) {
        return;
    }
    m_hoverIndex = index;
    updateCloseButtons();
    update();
}

QMenu *TabBar::createTabMenu(int index, QWidget *parent)
{
    auto *menu = new QMenu(parent);
    const int tabs = count();
    const quint64 id = index >= 0 && index < m_states.size() ? m_states.at(index).id : 0;

    auto add = [&](const char *name, const QString &text, const QKeySequence &keys = {}) {
        QAction *action = menu->addAction(text);
        action->setObjectName(QString::fromLatin1(name));
        if (!keys.isEmpty()) {
            // Shown as a hint; the window's own action has the shortcut.
            action->setShortcut(keys);
            action->setShortcutContext(Qt::WidgetShortcut);
        }
        return action;
    };

    QAction *close = add("close_tab", i18nc("@action:inmenu", "Close tab"), QKeySequence(Qt::CTRL | Qt::Key_W));
    QAction *closeOthers = add("close_other_tabs", i18nc("@action:inmenu", "Close other tabs"));
    QAction *closeRight = add("close_tabs_right", i18nc("@action:inmenu", "Close tabs to the right"));
    menu->addSeparator();
    QAction *duplicate = add("duplicate_tab", i18nc("@action:inmenu", "Duplicate tab"));
    QAction *moveOut = add("move_tab_new_window", i18nc("@action:inmenu", "Move tab to new window"));

    closeOthers->setEnabled(tabs > 1);
    closeRight->setEnabled(index >= 0 && index < tabs - 1);
    moveOut->setEnabled(tabs > 1);

    // The menu may outlive a tab change: find the tab by its id again.
    connect(close, &QAction::triggered, this, [this, id] {
        requestClose(indexOfId(id));
    });
    connect(closeOthers, &QAction::triggered, this, [this, id] {
        for (int i = count() - 1; i >= 0; --i) {
            if (i < m_states.size() && m_states.at(i).id != id) {
                Q_EMIT tabCloseRequested(i);
            }
        }
    });
    connect(closeRight, &QAction::triggered, this, [this, id] {
        const int from = indexOfId(id);
        if (from < 0) {
            return;
        }
        for (int i = count() - 1; i > from; --i) {
            Q_EMIT tabCloseRequested(i);
        }
    });
    connect(duplicate, &QAction::triggered, this, [this, id] {
        const int i = indexOfId(id);
        if (i >= 0) {
            Q_EMIT duplicateTabRequested(i);
        }
    });
    connect(moveOut, &QAction::triggered, this, [this, id] {
        const int i = indexOfId(id);
        if (i >= 0 && count() > 1) {
            Q_EMIT moveTabToNewWindowRequested(i);
        }
    });
    return menu;
}

void TabBar::mousePressEvent(QMouseEvent *event)
{
    const int index = tabAt(event->position().toPoint());
    m_dragIndex = event->button() == Qt::LeftButton ? index : -1;
    QTabBar::mousePressEvent(event);
}

void TabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        setHoverIndex(tabAt(event->position().toPoint()));
    }
    QTabBar::mouseMoveEvent(event);
}

void TabBar::mouseReleaseEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const int index = tabAt(pos);
    if (event->button() == Qt::MiddleButton && index >= 0) {
        // Middle click closes, like a browser.
        requestClose(index);
        event->accept();
        return;
    }

    const bool dragged = m_dragIndex >= 0 && event->button() == Qt::LeftButton;
    const bool outside = pos.y() < -detachDistance || pos.y() > height() + detachDistance;
    m_dragIndex = -1;
    QTabBar::mouseReleaseEvent(event);

    // Dropped well away from the strip: the tab goes to a window of its own.
    if (dragged && outside && count() > 1 && currentIndex() >= 0) {
        Q_EMIT moveTabToNewWindowRequested(currentIndex());
    }
    setHoverIndex(tabAt(mapFromGlobal(QCursor::pos())));
}

void TabBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        Q_EMIT titleAreaDoubleClicked();
        event->accept();
        return;
    }
    QTabBar::mouseDoubleClickEvent(event);
}

void TabBar::leaveEvent(QEvent *event)
{
    setHoverIndex(-1);
    QTabBar::leaveEvent(event);
}

void TabBar::contextMenuEvent(QContextMenuEvent *event)
{
    const int index = tabAt(event->pos());
    if (index < 0) {
        QTabBar::contextMenuEvent(event);
        return;
    }
    QMenu *menu = createTabMenu(index, this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(event->globalPos());
}

void TabBar::changeEvent(QEvent *event)
{
    QTabBar::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::LayoutDirectionChange) {
        relayout();
    }
}

}

