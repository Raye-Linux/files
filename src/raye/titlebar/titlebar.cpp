/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "titlebar.h"

#include "tabbar.h"
#include "tabwidth.h"

#include "ui/motion.h"
#include "ui/tokens.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QAbstractButton>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QTabBar>
#include <QTimer>
#include <QWindow>

#include <cmath>

namespace Raye::Title
{
using Ui::TokenSet;
using Ui::Tokens;

namespace
{
TitleBar::SnapHandler &snapHandler()
{
    static TitleBar::SnapHandler handler;
    return handler;
}

void sendSnapLayouts()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                          QStringLiteral("/component/kwin"),
                                                          QStringLiteral("org.kde.kglobalaccel.Component"),
                                                          QStringLiteral("invokeShortcut"));
    message.setArguments({QStringLiteral("Raye Snap Layouts")});
    message.setAutoStartService(false);
    QDBusConnection::sessionBus().send(message);
}

// The "+" after the last tab and the three caption buttons. They share the
// hover fade and differ in size, colour and glyph.
class RowButton : public QAbstractButton
{
public:
    enum Kind {
        NewTab,
        Minimize,
        Maximize,
        Close,
    };

    RowButton(Kind kind, QWidget *parent)
        : QAbstractButton(parent)
        , m_kind(kind)
        , m_fill(this, [this] {
            update();
        })
    {
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
        if (kind == NewTab) {
            setFixedSize(Metrics::newTabSize, Metrics::newTabSize);
        } else {
            setFixedSize(Metrics::captionWidth, Metrics::rowHeight);
        }
        updateTexts();
    }

    void setMaximized(bool maximized)
    {
        if (m_maximized != maximized) {
            m_maximized = maximized;
            updateTexts();
            update();
        }
    }

protected:
    bool event(QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::EnabledChange:
            update();
            break;
        default:
            break;
        }
        return QAbstractButton::event(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        const TokenSet &t = Tokens::current();
        const bool hovered = underMouse() && isEnabled();
        const bool pressed = isDown();

        QColor fill(Qt::transparent);
        if (m_kind == Close && (hovered || pressed)) {
            fill = t.color(pressed ? TokenSet::captionClosePressed : TokenSet::captionClose);
        } else if (pressed) {
            fill = t.color(TokenSet::fillSubtlePressed);
        } else if (hovered) {
            fill = t.color(TokenSet::fillSubtleHover);
        }
        if (m_fill.target() != fill) {
            // The close red comes in at once, like the decoration's.
            m_fill.setTarget(fill, m_kind == Close ? Ui::Motion::ExitShort : Ui::Motion::Faster);
        }
        const QColor shown = m_fill.value().isValid() ? m_fill.value() : fill;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(shown);
        if (m_kind == NewTab) {
            p.drawRoundedRect(QRectF(rect()), Ui::Metrics::radiusControl, Ui::Metrics::radiusControl);
        } else {
            p.drawRect(rect());
        }

        QColor ink = t.color(TokenSet::textPrimary);
        if (!isEnabled()) {
            ink = t.color(TokenSet::textDisabled);
        } else if (m_kind == Close && (hovered || pressed)) {
            ink = Qt::white;
        } else if (m_kind != NewTab && window() && !window()->isActiveWindow()) {
            ink = t.color(TokenSet::textSecondary);
        }
        drawGlyph(&p, ink);
    }

private:
    void updateTexts()
    {
        QString text;
        switch (m_kind) {
        case NewTab:
            text = i18nc("@action:button", "New tab");
            setToolTip(i18nc("@info:tooltip", "New tab (Ctrl+T)"));
            break;
        case Minimize:
            text = i18nc("@action:button", "Minimize");
            break;
        case Maximize:
            text = m_maximized ? i18nc("@action:button", "Restore down") : i18nc("@action:button", "Maximize");
            break;
        case Close:
            text = i18nc("@action:button", "Close");
            break;
        }
        setAccessibleName(text);
        if (m_kind != NewTab) {
            setToolTip(text);
        }
    }

    // The glyphs of the Raye decoration: 10 px monoline, 1 px strokes on the
    // pixel grid; "+" is 12 px.
    void drawGlyph(QPainter *p, const QColor &ink) const
    {
        const qreal dpr = devicePixelRatioF();
        const qreal stroke = qMax<qreal>(1.0, qRound(dpr)) / qMax<qreal>(dpr, 0.1);
        const qreal half = stroke / 2;
        const qreal g = m_kind == NewTab ? 12 : Metrics::captionGlyph;
        auto snap = [dpr](qreal v) {
            return std::round(v * dpr) / dpr;
        };
        const QRectF glyph(snap(width() / 2.0 - g / 2), snap(height() / 2.0 - g / 2), g, g);

        QPen pen(ink, stroke);
        pen.setCapStyle(Qt::FlatCap);
        pen.setJoinStyle(Qt::MiterJoin);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);

        switch (m_kind) {
        case NewTab: {
            pen.setCapStyle(Qt::RoundCap);
            p->setPen(pen);
            const qreal cx = glyph.center().x() + half;
            const qreal cy = glyph.center().y() + half;
            p->drawLine(QPointF(glyph.left(), cy), QPointF(glyph.right(), cy));
            p->drawLine(QPointF(cx, glyph.top()), QPointF(cx, glyph.bottom()));
            break;
        }
        case Minimize: {
            const qreal y = snap(glyph.center().y()) + half;
            p->drawLine(QPointF(glyph.left(), y), QPointF(glyph.right(), y));
            break;
        }
        case Maximize: {
            const qreal radius = 1.5;
            if (m_maximized) {
                // restore: a square with a second one behind it, up and right
                const QRectF front(glyph.left() + half, glyph.top() + 2 + half, 8 - stroke, 8 - stroke);
                p->drawRoundedRect(front, radius, radius);
                QPainterPath back;
                back.moveTo(glyph.left() + 2 + half, glyph.top() + 2);
                back.lineTo(glyph.left() + 2 + half, glyph.top() + half + radius);
                back.arcTo(QRectF(glyph.left() + 2 + half, glyph.top() + half, 2 * radius, 2 * radius), 180, -90);
                back.lineTo(glyph.right() - half - radius, glyph.top() + half);
                back.arcTo(QRectF(glyph.right() - half - 2 * radius, glyph.top() + half, 2 * radius, 2 * radius), 90, -90);
                back.lineTo(glyph.right() - half, glyph.bottom() - 2 - half);
                back.lineTo(glyph.right() - 2, glyph.bottom() - 2 - half);
                p->drawPath(back);
            } else {
                p->drawRoundedRect(glyph.adjusted(half, half, -half, -half), radius, radius);
            }
            break;
        }
        case Close:
            pen.setCapStyle(Qt::RoundCap);
            p->setPen(pen);
            p->drawLine(glyph.topLeft() + QPointF(half, half), glyph.bottomRight() - QPointF(half, half));
            p->drawLine(glyph.topRight() + QPointF(-half, half), glyph.bottomLeft() + QPointF(half, -half));
            break;
        }
    }

    Kind m_kind;
    Ui::ColorFade m_fill;
    bool m_maximized = false;
};
}

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
    , m_newTab(new RowButton(RowButton::NewTab, this))
    , m_minimize(new RowButton(RowButton::Minimize, this))
    , m_maximize(new RowButton(RowButton::Maximize, this))
    , m_close(new RowButton(RowButton::Close, this))
    , m_snapTimer(new QTimer(this))
{
    setFixedHeight(Metrics::rowHeight);
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_newTab->setObjectName(QStringLiteral("newtab"));
    m_minimize->setObjectName(QStringLiteral("minimize"));
    m_maximize->setObjectName(QStringLiteral("maximize"));
    m_close->setObjectName(QStringLiteral("close"));

    connect(m_newTab, &QAbstractButton::clicked, this, &TitleBar::newTabRequested);
    connect(m_minimize, &QAbstractButton::clicked, this, [this] {
        window()->showMinimized();
    });
    connect(m_maximize, &QAbstractButton::clicked, this, &TitleBar::toggleMaximized);
    connect(m_close, &QAbstractButton::clicked, this, [this] {
        window()->close();
    });

    // Resting on Maximise opens Snap layouts; leaving or pressing cancels.
    m_snapTimer->setSingleShot(true);
    m_snapTimer->setInterval(Metrics::snapLayoutsDelayMs);
    connect(m_snapTimer, &QTimer::timeout, this, [this] {
        if (m_own && m_maximize->underMouse() && !m_maximize->isDown()) {
            requestSnapLayouts();
        }
    });
    m_maximize->installEventFilter(this);
    connect(m_maximize, &QAbstractButton::pressed, m_snapTimer, &QTimer::stop);

    connect(Tokens::instance(), &Tokens::changed, this, qOverload<>(&QWidget::update));
    setOwnTitleBar(readOwnTitleBar());
}

TitleBar::~TitleBar() = default;

bool TitleBar::readOwnTitleBar()
{
    const KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("dolphinrc")), QStringLiteral("Raye"));
    return ownTitleBarFor(group.readEntry("OwnTitleBar", QString()), decorationHidesTitleBar());
}

bool TitleBar::ownTitleBarFor(const QString &setting, bool decorationHides)
{
    if (setting.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (setting.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    return decorationHides;
}

bool TitleBar::decorationHidesTitleBar(const QString &windowClass)
{
    // Only the Raye decoration reads the exceptions of rayerc; with any other
    // one Files would draw its buttons under a second title bar.
    const KConfigGroup kwin(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("org.kde.kdecoration2"));
    if (kwin.readEntry("library", QString()) != QLatin1String("org.raye.decoration")) {
        return false;
    }
    const KSharedConfig::Ptr rayerc = KSharedConfig::openConfig(QStringLiteral("rayerc"));
    for (int i = 0;; ++i) {
        const QString name = QStringLiteral("Windeco Exception %1").arg(i);
        if (!rayerc->hasGroup(name)) {
            return false;
        }
        const KConfigGroup g = rayerc->group(name);
        // The decoration stops at the first exception that matches.
        const QString pattern = g.readEntry("ExceptionPattern", QString());
        if (!g.readEntry("Enabled", true) || pattern.isEmpty() || g.readEntry("ExceptionType", 0) != 0) {
            continue;
        }
        if (QRegularExpression(pattern).match(windowClass).hasMatch()) {
            return g.readEntry("HideTitleBar", false);
        }
    }
}

void TitleBar::setOwnTitleBar(bool own)
{
    m_own = own;
    m_minimize->setVisible(own);
    m_maximize->setVisible(own);
    m_close->setVisible(own);
    if (!own) {
        m_snapTimer->stop();
    }
    relayout();
}

void TitleBar::setTabBar(QTabBar *tabBar)
{
    if (m_tabs == tabBar) {
        return;
    }
    if (m_tabs) {
        m_tabs->removeEventFilter(this);
        if (auto *raye = qobject_cast<TabBar *>(m_tabs)) {
            disconnect(raye, nullptr, this, nullptr);
        }
    }
    m_tabs = tabBar;
    if (m_tabs) {
        m_tabs->setParent(this);
        m_tabs->installEventFilter(this);
        m_tabs->show();
        if (auto *raye = qobject_cast<TabBar *>(m_tabs)) {
            connect(raye, &TabBar::titleAreaDoubleClicked, this, [this] {
                if (m_own) {
                    toggleMaximized();
                }
            });
        }
    }
    relayout();
}

QAbstractButton *TitleBar::button(const QString &name) const
{
    for (QAbstractButton *b : {m_newTab, m_minimize, m_maximize, m_close}) {
        if (b->objectName() == name) {
            return b;
        }
    }
    return nullptr;
}

void TitleBar::setSnapLayoutsHandler(const SnapHandler &handler)
{
    snapHandler() = handler;
}

void TitleBar::requestSnapLayouts()
{
    if (snapHandler()) {
        snapHandler()();
    } else {
        sendSnapLayouts();
    }
}

void TitleBar::toggleMaximized()
{
    QWidget *w = window();
    if (w->isMaximized()) {
        w->showNormal();
    } else {
        w->showMaximized();
    }
}

QSize TitleBar::sizeHint() const
{
    return QSize(Metrics::leftMargin + Metrics::tabMaxWidth + Metrics::newTabGap + Metrics::newTabSize + Metrics::minDragSpace + 3 * Metrics::captionWidth,
                 Metrics::rowHeight);
}

QSize TitleBar::minimumSizeHint() const
{
    return QSize(Metrics::leftMargin + Metrics::tabMinWidth + Metrics::newTabGap + Metrics::newTabSize + (m_own ? 3 * Metrics::captionWidth : 0),
                 Metrics::rowHeight);
}

void TitleBar::relayout()
{
    const int captions = m_own ? 3 * Metrics::captionWidth : 0;
    if (m_own) {
        int x = width() - captions;
        for (QAbstractButton *b : {m_minimize, m_maximize, m_close}) {
            b->setGeometry(x, 0, Metrics::captionWidth, Metrics::rowHeight);
            x += Metrics::captionWidth;
        }
    }

    const int tabsTop = Metrics::rowHeight - Metrics::tabHeight;
    int x = Metrics::leftMargin;
    const int room = qMax(0, width() - Metrics::leftMargin - Metrics::newTabGap - Metrics::newTabSize - Metrics::minDragSpace - captions);
    if (m_tabs) {
        if (auto *raye = qobject_cast<TabBar *>(m_tabs)) {
            raye->setAvailableWidth(room);
        }
        const int w = qMin(m_tabs->sizeHint().width(), room);
        m_tabs->setGeometry(x, tabsTop, w, Metrics::tabHeight);
        x += w;
    }
    m_newTab->move(x + Metrics::newTabGap, tabsTop + (Metrics::tabHeight - Metrics::newTabSize) / 2 - 2);
}

bool TitleBar::event(QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest) {
        // The strip changed its size (a tab came, went or grew).
        relayout();
    } else if (event->type() == QEvent::Show) {
        watchWindow();
    }
    return QWidget::event(event);
}

bool TitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_maximize) {
        if (event->type() == QEvent::Enter && m_own) {
            m_snapTimer->start();
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::Hide) {
            m_snapTimer->stop();
        }
    } else if (watched == m_window) {
        if (event->type() == QEvent::WindowStateChange) {
            updateCaptionStates();
        }
    } else if (watched == m_tabs && event->type() == QEvent::LayoutRequest) {
        relayout();
    } else if (watched == m_tabs && (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
        // QTabWidget still places the bar it owns when its pages change
        // size; put it back in the row.
        QTimer::singleShot(0, this, &TitleBar::relayout);
    }
    return QWidget::eventFilter(watched, event);
}

void TitleBar::watchWindow()
{
    QWidget *w = window();
    if (w == m_window || w == this) {
        return;
    }
    if (m_window) {
        m_window->removeEventFilter(this);
    }
    m_window = w;
    m_window->installEventFilter(this);
    updateCaptionStates();
}

void TitleBar::updateCaptionStates()
{
    const bool maximized = window()->isMaximized();
    static_cast<RowButton *>(m_maximize)->setMaximized(maximized);
}

void TitleBar::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.fillRect(event->rect(), Tokens::current().color(TokenSet::bgBase));
}

void TitleBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayout();
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    // Empty space of the row: it moves the window once the pointer travels.
    if (m_own && event->button() == Qt::LeftButton) {
        m_pressed = true;
        m_pressPos = event->position().toPoint();
        event->accept();
        return;
    }
    // A right click would open the window menu on Windows; KWin offers no
    // way for an application to open it, so nothing happens.
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_pressed && (event->buttons() & Qt::LeftButton)
        && (event->position().toPoint() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
        m_pressed = false;
        if (QWindow *handle = window()->windowHandle()) {
            handle->startSystemMove();
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_pressed = false;
    QWidget::mouseReleaseEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_own && event->button() == Qt::LeftButton) {
        m_pressed = false;
        toggleMaximized();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TitleBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::ParentChange:
        watchWindow();
        break;
    case QEvent::ActivationChange:
        // Caption glyphs dim while the window is in the background.
        update();
        for (QAbstractButton *b : {m_minimize, m_maximize, m_close}) {
            b->update();
        }
        break;
    default:
        break;
    }
}

}

