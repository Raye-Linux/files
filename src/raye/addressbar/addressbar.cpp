/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "addressbar.h"
#include "themeicon_p.h"
#include "addressbox.h"

#include "ui/commandbutton.h"
#include "ui/tokens.h"

#include <KLocalizedString>
#include <KUrlNavigator>

#include <QAction>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QMouseEvent>
#include <QTimer>

#include <functional>

namespace Raye::Address
{
namespace
{
constexpr int margin = 8;
constexpr int buttonSize = 32;
constexpr int buttonGap = 4;
constexpr int splitterWidth = 4;
constexpr int holdDelay = 500; // a long press on Back or Forward opens its history
}

// The 4 px handle between the address box and the end of the row: dragging
// it left widens the search box.
class Splitter : public QWidget
{
public:
    explicit Splitter(std::function<int()> width, std::function<void(int)> resize, QWidget *parent)
        : QWidget(parent)
        , m_width(std::move(width))
        , m_resize(std::move(resize))
    {
        setCursor(Qt::SplitHCursor);
        setFixedWidth(splitterWidth);
        setAccessibleName(i18nc("@info accessible name of the handle that resizes the search box", "Resize the search box"));
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressX = event->globalPosition().x();
            m_startWidth = m_width();
            m_dragging = true;
            event->accept();
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragging) {
            m_resize(m_startWidth - int(event->globalPosition().x() - m_pressX));
        }
    }
    void mouseReleaseEvent(QMouseEvent *) override
    {
        m_dragging = false;
    }

private:
    std::function<int()> m_width;
    std::function<void(int)> m_resize;
    qreal m_pressX = 0;
    int m_startWidth = 0;
    bool m_dragging = false;
};

AddressBar::AddressBar(QWidget *parent)
    : QWidget(parent)
    , m_box(new AddressBox(this))
    , m_editAction(new QAction(this))
    , m_holdTimer(new QTimer(this))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto button = [this](const char *icon, const QString &name, const QKeySequence &shortcut) {
        auto *b = new Ui::CommandButton(themeIcon(QString::fromLatin1(icon)), name, this);
        b->setIconOnly(true);
        b->setFixedSize(buttonSize, buttonSize);
        b->setShortcutHint(shortcut);
        b->setToolTip(Ui::CommandButton::toolTipFor(name, shortcut));
        b->setAccessibleName(name);
        return b;
    };
    m_back = button("go-previous", i18nc("@action:button go to the previous location", "Back"), QKeySequence(Qt::ALT | Qt::Key_Left));
    m_forward = button("go-next", i18nc("@action:button go to the next location", "Forward"), QKeySequence(Qt::ALT | Qt::Key_Right));
    m_up = button("go-up", i18nc("@action:button go to the folder that holds this one", "Up"), QKeySequence(Qt::ALT | Qt::Key_Up));
    m_refresh = button("view-refresh", i18nc("@action:button load the folder again", "Refresh"), QKeySequence(Qt::Key_F5));

    m_splitter = new Splitter(
        [this]() {
            return m_trailingWidth;
        },
        [this](int width) {
            setTrailingWidth(width);
        },
        this);
    m_splitter->hide();

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(margin, 0, margin, 0);
    m_layout->setSpacing(buttonGap);
    m_layout->addWidget(m_back);
    m_layout->addWidget(m_forward);
    m_layout->addWidget(m_up);
    m_layout->addWidget(m_refresh);
    // The box keeps room around it for its glow: one gap less.
    m_layout->addSpacing(buttonGap - AddressBox::glowMargin);
    m_layout->addWidget(m_box, 1);
    m_layout->addWidget(m_splitter);

    connect(m_back, &QAbstractButton::clicked, this, [this]() {
        goSteps(Back, 1);
    });
    connect(m_forward, &QAbstractButton::clicked, this, [this]() {
        goSteps(Forward, 1);
    });
    connect(m_up, &QAbstractButton::clicked, this, [this]() {
        const QUrl up = m_box->locationNames().upUrl(url());
        if (up.isValid() && !up.isEmpty()) {
            setUrl(up);
        }
    });
    connect(m_refresh, &QAbstractButton::clicked, this, &AddressBar::refreshRequested);
    connect(m_box, &AddressBox::navigateRequested, this, &AddressBar::setUrl);
    connect(m_box, &AddressBox::urlsDropped, this, &AddressBar::urlsDropped);

    // Back and Forward: right-click or a long press shows their history.
    for (Ui::CommandButton *b : {m_back, m_forward}) {
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        b->installEventFilter(this);
        const Direction direction = b == m_back ? Back : Forward;
        connect(b, &QWidget::customContextMenuRequested, this, [this, direction]() {
            showHistoryMenu(direction);
        });
    }
    m_holdTimer->setSingleShot(true);
    m_holdTimer->setInterval(holdDelay);
    connect(m_holdTimer, &QTimer::timeout, this, [this]() {
        Ui::CommandButton *b = m_holdDirection == Back ? m_back : m_forward;
        b->setDown(false);
        showHistoryMenu(m_holdDirection);
    });

    m_editAction->setText(i18nc("@action edit the address as text", "Edit address"));
    m_editAction->setObjectName(QStringLiteral("raye_edit_address"));
    m_editAction->setShortcuts({QKeySequence(Qt::ALT | Qt::Key_D), QKeySequence(Qt::CTRL | Qt::Key_L), QKeySequence(Qt::Key_F4)});
    m_editAction->setShortcutContext(Qt::WindowShortcut);
    connect(m_editAction, &QAction::triggered, this, &AddressBar::editLocation);
    addAction(m_editAction);
    updateFromNavigator();
}

AddressBar::~AddressBar()
{
    // No reaction to the navigators going away with the row.
    if (m_navigator) {
        m_navigator->disconnect(this);
    }
    if (m_ownNavigator) {
        m_ownNavigator->disconnect(this);
    }
}

void AddressBar::setNavigator(KUrlNavigator *navigator)
{
    if (m_navigator) {
        m_navigator->disconnect(this);
    }
    if (!navigator) {
        // Made when first needed, at the first location: no empty step in
        // its history.
        m_navigator.clear();
        updateFromNavigator();
        return;
    }
    m_navigator = navigator;
    connect(navigator, &KUrlNavigator::urlChanged, this, &AddressBar::updateFromNavigator);
    connect(navigator, &KUrlNavigator::historyChanged, this, &AddressBar::updateFromNavigator);
    connect(navigator, &QObject::destroyed, this, [this](QObject *gone) {
        if (gone == m_ownNavigator) {
            m_ownNavigator = nullptr;
        }
        // The view container went first: keep a history of our own.
        setNavigator(nullptr);
    });
    updateFromNavigator();
}

KUrlNavigator *AddressBar::navigator() const
{
    if (!m_navigator) {
        const_cast<AddressBar *>(this)->useOwnNavigator(Locations::homePageUrl());
    }
    return m_navigator;
}

void AddressBar::useOwnNavigator(const QUrl &url)
{
    if (!m_ownNavigator) {
        m_ownNavigator = new KUrlNavigator(nullptr, url, this);
        m_ownNavigator->hide();
    }
    setNavigator(m_ownNavigator);
}

void AddressBar::setLocationNames(const Locations::LocationNames &names)
{
    m_box->setLocationNames(names);
    updateFromNavigator();
}

QUrl AddressBar::url() const
{
    return m_navigator ? m_navigator->locationUrl() : QUrl();
}

void AddressBar::setUrl(const QUrl &url)
{
    if (!url.isValid()) {
        return;
    }
    if (!m_navigator) {
        useOwnNavigator(url);
        return;
    }
    m_navigator->setLocationUrl(url);
}

void AddressBar::updateFromNavigator()
{
    if (!m_navigator) {
        m_back->setEnabled(false);
        m_forward->setEnabled(false);
        m_up->setEnabled(false);
        return;
    }
    const QUrl current = m_navigator->locationUrl();
    if (current != m_box->url()) {
        m_box->setUrl(current);
    }
    const int index = m_navigator->historyIndex();
    m_back->setEnabled(index < m_navigator->historySize() - 1);
    m_forward->setEnabled(index > 0);
    const QUrl up = m_box->locationNames().upUrl(current);
    m_up->setEnabled(up.isValid() && !up.isEmpty());
}

void AddressBar::goSteps(Direction direction, int steps)
{
    // The way Dolphin's own history menu does it: one step at a time.
    for (int i = 0; i < steps && m_navigator; ++i) {
        const bool moved = direction == Back ? m_navigator->goBack() : m_navigator->goForward();
        if (!moved) {
            break;
        }
    }
}

QMenu *AddressBar::historyMenu(Direction direction)
{
    auto *menu = new QMenu(this);
    if (!m_navigator) {
        return menu;
    }
    const int index = m_navigator->historyIndex();
    const int size = m_navigator->historySize();
    QList<int> entries;
    if (direction == Back) {
        for (int i = index + 1; i < size && entries.size() < historyEntries; ++i) {
            entries << i;
        }
    } else {
        for (int i = index - 1; i >= 0 && entries.size() < historyEntries; --i) {
            entries << i;
        }
    }
    const Locations::LocationNames &names = m_box->locationNames();
    for (int entry : std::as_const(entries)) {
        const QUrl u = m_navigator->locationUrl(entry);
        QAction *action = menu->addAction(themeIcon(names.iconName(u)), names.displayName(u));
        connect(action, &QAction::triggered, this, [this, entry]() {
            if (!m_navigator) {
                return;
            }
            const int now = m_navigator->historyIndex();
            if (entry > now) {
                goSteps(Back, entry - now);
            } else if (entry < now) {
                goSteps(Forward, now - entry);
            }
        });
    }
    return menu;
}

void AddressBar::showHistoryMenu(Direction direction)
{
    Ui::CommandButton *b = direction == Back ? m_back : m_forward;
    if (!b->isEnabled()) {
        return;
    }
    QMenu *menu = historyMenu(direction);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(b->mapToGlobal(QPoint(0, b->height() + 4)));
}

void AddressBar::editLocation()
{
    m_box->editLocation();
}

void AddressBar::setLoading(bool loading)
{
    m_box->setLoading(loading);
}

void AddressBar::setLoadingProgress(int percent)
{
    m_box->setLoadingProgress(percent);
}

void AddressBar::setTrailingWidget(QWidget *widget)
{
    if (m_trailing == widget) {
        return;
    }
    if (m_trailing) {
        m_layout->removeWidget(m_trailing);
        delete m_trailing;
    }
    m_trailing = widget;
    m_splitter->setVisible(widget != nullptr);
    if (widget) {
        widget->setParent(this);
        widget->setFixedWidth(m_trailingWidth);
        m_layout->addWidget(widget);
        widget->show();
    }
}

void AddressBar::setTrailingWidth(int width)
{
    width = std::clamp(width, int(trailingMinimum), int(trailingMaximum));
    if (width == m_trailingWidth) {
        return;
    }
    m_trailingWidth = width;
    if (m_trailing) {
        m_trailing->setFixedWidth(width);
    }
    Q_EMIT trailingWidthChanged(width);
}

QSize AddressBar::sizeHint() const
{
    return QSize(QWidget::sizeHint().width(), rowHeight);
}

QSize AddressBar::minimumSizeHint() const
{
    return QSize(QWidget::minimumSizeHint().width(), rowHeight);
}

bool AddressBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_back || watched == m_forward) {
        if (event->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
            m_holdDirection = watched == m_back ? Back : Forward;
            m_holdTimer->start();
        } else if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::Leave) {
            m_holdTimer->stop();
        }
    }
    return QWidget::eventFilter(watched, event);
}

}
