/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "addressbox.h"
#include "themeicon_p.h"
#include "addressbox_p.h"
#include "subfolders.h"

#include "ui/commandbutton.h"
#include "ui/focusring.h"
#include "ui/motion.h"
#include "ui/tokens.h"

#include <KCompletionBox>
#include <KLineEdit>
#include <KLocalizedString>
#include <KUrlCompletion>

#include <QDropEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QVariantAnimation>

namespace Raye::Address
{
using Ui::Metrics::radiusControl;
using Ui::TokenSet;
using Ui::Tokens;

namespace
{
constexpr int crumbHeight = 24;
constexpr int crumbPadding = 6; // left and right of a crumb's text
constexpr int innerPadding = 8; // inside the box, left and right
constexpr int iconGap = 6; // between the location icon and the first crumb
constexpr int overflowWidth = 24;
constexpr int loadingLine = 2;
constexpr int focusStroke = 2;

// A chevron of 12 px pointing right, or down when open.
void paintArrow(QPainter *painter, const QRectF &box, const QColor &color, bool down)
{
    const QPointF c = box.center();
    const qreal s = 3.0;
    QPainterPath path;
    if (down) {
        path.moveTo(c.x() - s, c.y() - s / 2);
        path.lineTo(c.x(), c.y() + s / 2);
        path.lineTo(c.x() + s, c.y() - s / 2);
    } else {
        path.moveTo(c.x() - s / 2, c.y() - s);
        path.lineTo(c.x() + s / 2, c.y());
        path.lineTo(c.x() - s / 2, c.y() + s);
    }
    QPen pen(color, 1.25);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
}

bool carriesFiles(const QMimeData *mime)
{
    return mime && mime->hasUrls() && !mime->urls().isEmpty();
}
}

// ---------------------------------------------------------------- crumbs

CrumbButton::CrumbButton(QWidget *parent)
    : QAbstractButton(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);
    setFont(Ui::textFont(Ui::TextStyle::Body));
    setCursor(Qt::ArrowCursor);
}

void CrumbButton::setCrumb(const Locations::Crumb &crumb)
{
    m_crumb = crumb;
    setText(crumb.text);
    setAccessibleName(crumb.text);
    setToolTip(crumb.url.toDisplayString(QUrl::PreferLocalFile));
    setAcceptDrops(crumb.isFolder);
    updateGeometry();
    update();
}

int CrumbButton::naturalWidth() const
{
    return fontMetrics().horizontalAdvance(text()) + 2 * crumbPadding + 1;
}

QSize CrumbButton::sizeHint() const
{
    return QSize(naturalWidth(), crumbHeight);
}

void CrumbButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const TokenSet &t = Tokens::current();
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    if (m_dropTarget) {
        p.setPen(QPen(t.color(TokenSet::accent), 1));
        p.setBrush(t.color(TokenSet::fillSubtleSelected));
        p.drawRoundedRect(r, radiusControl, radiusControl);
    } else if (isDown() || m_hovered) {
        p.setPen(Qt::NoPen);
        p.setBrush(t.color(isDown() ? TokenSet::fillSubtlePressed : TokenSet::fillSubtleHover));
        p.drawRoundedRect(r, radiusControl, radiusControl);
    }
    p.setPen(t.color(isEnabled() ? TokenSet::textPrimary : TokenSet::textDisabled));
    const QRect textRect = rect().adjusted(crumbPadding, 0, -crumbPadding, 0);
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, fontMetrics().elidedText(text(), Qt::ElideRight, textRect.width()));
}

void CrumbButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void CrumbButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void CrumbButton::dragEnterEvent(QDragEnterEvent *event)
{
    if (!m_crumb.isFolder || !carriesFiles(event->mimeData())) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    m_dropTarget = true;
    update();
}

void CrumbButton::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_dropTarget = false;
    update();
    QAbstractButton::dragLeaveEvent(event);
}

void CrumbButton::dropEvent(QDropEvent *event)
{
    m_dropTarget = false;
    update();
    if (!m_crumb.isFolder || !carriesFiles(event->mimeData())) {
        event->ignore();
        return;
    }
    Q_EMIT dropped(m_crumb.url, event);
}

ChevronButton::ChevronButton(QWidget *parent)
    : QAbstractButton(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);
    setAccessibleName(i18nc("@action:button opens the list of folders in a part of the address", "Show folders"));
}

void ChevronButton::setOpen(bool open)
{
    m_open = open;
    update();
}

QSize ChevronButton::sizeHint() const
{
    return QSize(width, crumbHeight);
}

void ChevronButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const TokenSet &t = Tokens::current();
    if (m_open || isDown() || m_hovered) {
        p.setPen(Qt::NoPen);
        p.setBrush(t.color(m_open || isDown() ? TokenSet::fillSubtlePressed : TokenSet::fillSubtleHover));
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radiusControl, radiusControl);
    }
    paintArrow(&p, QRectF(rect()), t.color(TokenSet::textSecondary), m_open);
}

void ChevronButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void ChevronButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

// ---------------------------------------------------------------- flyout

SubfolderFlyout::SubfolderFlyout(const Locations::LocationNames &names, QWidget *parent)
    : Ui::Flyout(parent)
    , m_lister(new Subfolders(this))
{
    m_lister->setLocationNames(names);
    connect(m_lister, &Subfolders::finished, this, &SubfolderFlyout::fill);
}

void SubfolderFlyout::list(const QUrl &url)
{
    m_lister->list(url);
}

void SubfolderFlyout::fill()
{
    auto *list = new QWidget;
    auto *layout = new QVBoxLayout(list);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    const QList<Locations::Crumb> folders = m_lister->folders();
    int widest = 200;
    for (const Locations::Crumb &folder : folders) {
        auto *row = new Ui::CommandButton(themeIcon(folder.iconName), folder.text, list);
        row->setContentAlignment(Qt::AlignLeft);
        row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->setFocusPolicy(Qt::StrongFocus);
        widest = std::max(widest, std::min(row->sizeHint().width() + 16, 360));
        const QUrl url = folder.url;
        connect(row, &QAbstractButton::clicked, this, [this, url]() {
            Q_EMIT folderChosen(url);
            dismiss();
        });
        layout->addWidget(row);
    }
    if (folders.isEmpty()) {
        auto *empty = new QLabel(m_lister->failed() ? i18nc("@info in the folder list of the address", "This folder can't be opened.")
                                                    : i18nc("@info in the folder list of the address", "No folders"),
                                 list);
        empty->setFont(Ui::textFont(Ui::TextStyle::Body));
        empty->setContentsMargins(Ui::Metrics::labelPadding, 0, Ui::Metrics::labelPadding, 0);
        empty->setFixedHeight(Ui::Metrics::controlHeight);
        empty->setEnabled(false);
        layout->addWidget(empty);
    }

    auto *scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidgetResizable(true);
    scroll->viewport()->setAutoFillBackground(false);
    list->setAutoFillBackground(false);
    scroll->setWidget(list);
    const int rows = std::clamp(int(folders.size()), 1, visibleRows);
    const int bar = folders.size() > visibleRows ? scroll->verticalScrollBar()->sizeHint().width() : 0;
    scroll->setFixedSize(widest + bar, rows * Ui::Metrics::controlHeight);
    setContentWidget(scroll);
    Q_EMIT ready();
}

// ---------------------------------------------------------------- box

AddressBox::AddressBox(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::TabFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(i18nc("@info accessible name of the address box", "Address"));
    setCursor(Qt::IBeamCursor);

    m_overflow = new CrumbButton(this);
    m_overflow->setCrumb({QUrl(), QStringLiteral("«"), QString(), Locations::Kind::Folder, false});
    m_overflow->setAccessibleName(i18nc("@action:button the hidden first parts of a long address", "Earlier folders"));
    m_overflow->setToolTip(m_overflow->accessibleName());
    m_overflow->hide();
    connect(m_overflow, &QAbstractButton::clicked, this, [this]() {
        QMenu *menu = overflowMenu();
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(m_overflow->mapToGlobal(QPoint(0, m_overflow->height() + Ui::Metrics::flyoutGap)));
    });

    m_edit = new KLineEdit(this);
    m_edit->setFrame(false);
    m_edit->setFont(Ui::textFont(Ui::TextStyle::Body));
    m_edit->setAccessibleName(accessibleName());
    auto *completion = new KUrlCompletion(KUrlCompletion::DirCompletion);
    m_edit->setCompletionObject(completion);
    m_edit->setAutoDeleteCompletionObject(true);
    m_edit->setCompletionMode(KCompletion::CompletionPopupAuto);
    m_edit->installEventFilter(this);
    m_edit->hide();
    connect(m_edit, &QLineEdit::returnPressed, this, &AddressBox::finishEditing);

    connect(Tokens::instance(), &Tokens::changed, this, [this]() {
        update();
    });
}

AddressBox::~AddressBox() = default;

void AddressBox::setLocationNames(const Locations::LocationNames &names)
{
    m_names = names;
    rebuild();
}

const Locations::LocationNames &AddressBox::locationNames() const
{
    if (!m_names) {
        const_cast<AddressBox *>(this)->m_names = Locations::LocationNames();
    }
    return *m_names;
}

void AddressBox::setUrl(const QUrl &url)
{
    if (m_editing) {
        cancelEditing();
    }
    if (m_flyout) {
        m_flyout->dismiss();
    }
    m_url = url;
    rebuild();
}

void AddressBox::rebuild()
{
    m_crumbs = m_url.isEmpty() ? QList<Locations::Crumb>() : locationNames().crumbs(m_url);
    m_iconName = m_crumbs.isEmpty() ? QString() : m_crumbs.last().iconName;

    // Later, not now: the crumb clicked may be one of those going away, and
    // it is still in its click handler.
    while (m_crumbButtons.size() > m_crumbs.size()) {
        for (QWidget *gone : {static_cast<QWidget *>(m_crumbButtons.takeLast()), static_cast<QWidget *>(m_chevrons.takeLast())}) {
            gone->hide();
            gone->deleteLater();
        }
    }
    while (m_crumbButtons.size() < m_crumbs.size()) {
        const int index = m_crumbButtons.size();
        auto *crumb = new CrumbButton(this);
        connect(crumb, &QAbstractButton::clicked, this, [this, crumb]() {
            Q_EMIT navigateRequested(crumb->crumb().url);
        });
        connect(crumb, &CrumbButton::dropped, this, &AddressBox::urlsDropped);
        auto *chevron = new ChevronButton(this);
        connect(chevron, &QAbstractButton::clicked, this, [this, index]() {
            showSubfolders(index);
        });
        m_crumbButtons.append(crumb);
        m_chevrons.append(chevron);
    }
    for (int i = 0; i < m_crumbs.size(); ++i) {
        m_crumbButtons.at(i)->setCrumb(m_crumbs.at(i));
    }
    relayout();
    update();
}

int AddressBox::contentLeft() const
{
    return boxRect().left() + innerPadding + Ui::Metrics::iconSize + iconGap;
}

QRect AddressBox::boxRect() const
{
    return QRect(glowMargin, (height() - boxHeight) / 2, width() - 2 * glowMargin, boxHeight);
}

void AddressBox::relayout()
{
    const QRect box = boxRect();
    const int left = contentLeft();
    const int right = box.right() - innerPadding;
    const int y = box.top() + (boxHeight - crumbHeight) / 2;

    if (m_editing) {
        for (int i = 0; i < m_crumbButtons.size(); ++i) {
            m_crumbButtons.at(i)->hide();
            m_chevrons.at(i)->hide();
        }
        m_overflow->hide();
        m_edit->setGeometry(left, box.top() + 1, std::max(0, right - left), boxHeight - 2);
        return;
    }

    // A chevron after every crumb that holds folders.
    QList<int> widths;
    QList<bool> chevron;
    int total = 0;
    for (int i = 0; i < m_crumbs.size(); ++i) {
        const Locations::Crumb &c = m_crumbs.at(i);
        const bool hasChevron = c.isFolder || c.kind == Locations::Kind::ThisPc || c.kind == Locations::Kind::Network;
        chevron.append(hasChevron);
        widths.append(m_crumbButtons.at(i)->naturalWidth() + (hasChevron ? ChevronButton::width : 0));
        total += widths.last();
    }

    // Fold from the left until the rest fits; the location itself stays.
    const int available = right - left;
    m_hidden = 0;
    if (total > available) {
        int used = total + overflowWidth;
        while (m_hidden < m_crumbs.size() - 1 && used > available) {
            used -= widths.at(m_hidden);
            ++m_hidden;
        }
    }

    int x = left;
    if (m_hidden > 0) {
        m_overflow->setGeometry(x, y, overflowWidth, crumbHeight);
        m_overflow->show();
        x += overflowWidth;
    } else {
        m_overflow->hide();
    }
    for (int i = 0; i < m_crumbs.size(); ++i) {
        CrumbButton *crumb = m_crumbButtons.at(i);
        ChevronButton *chev = m_chevrons.at(i);
        if (i < m_hidden) {
            crumb->hide();
            chev->hide();
            continue;
        }
        const int chevronWidth = chevron.at(i) ? ChevronButton::width : 0;
        const int crumbWidth = std::max(crumbPadding * 2, std::min(crumb->naturalWidth(), right - x - chevronWidth));
        crumb->setGeometry(x, y, crumbWidth, crumbHeight);
        crumb->show();
        x += crumbWidth;
        if (chevronWidth > 0) {
            chev->setGeometry(x, y, chevronWidth, crumbHeight);
            chev->show();
            x += chevronWidth;
        } else {
            chev->hide();
        }
    }
    m_edit->hide();
}

QStringList AddressBox::crumbTexts() const
{
    QStringList out;
    for (const Locations::Crumb &c : m_crumbs) {
        out << c.text;
    }
    return out;
}

QWidget *AddressBox::crumbWidget(int index) const
{
    return index >= 0 && index < m_crumbButtons.size() ? m_crumbButtons.at(index) : nullptr;
}

QWidget *AddressBox::overflowButton() const
{
    return m_overflow;
}

QMenu *AddressBox::overflowMenu()
{
    auto *menu = new QMenu(this);
    for (int i = m_hidden - 1; i >= 0; --i) {
        const Locations::Crumb crumb = m_crumbs.at(i);
        QAction *action = menu->addAction(themeIcon(crumb.iconName), crumb.text);
        connect(action, &QAction::triggered, this, [this, url = crumb.url]() {
            Q_EMIT navigateRequested(url);
        });
    }
    return menu;
}

void AddressBox::showSubfolders(int index)
{
    if (index < 0 || index >= m_crumbs.size()) {
        return;
    }
    if (m_flyout) {
        m_flyout->dismiss();
    }
    ChevronButton *chevron = m_chevrons.at(index);
    auto *flyout = new SubfolderFlyout(locationNames(), this);
    m_flyout = flyout;
    chevron->setOpen(true);
    connect(flyout, &SubfolderFlyout::folderChosen, this, &AddressBox::navigateRequested);
    connect(flyout, &Ui::Flyout::closed, chevron, [chevron, flyout]() {
        chevron->setOpen(false);
        flyout->deleteLater();
    });
    QPointer<ChevronButton> anchor(chevron);
    connect(flyout, &SubfolderFlyout::ready, flyout, [flyout, anchor]() {
        if (anchor) {
            flyout->showAt(anchor, Qt::BottomEdge, Qt::AlignLeft);
        }
    });
    flyout->list(m_crumbs.at(index).url);
}

void AddressBox::editLocation()
{
    if (!m_editing) {
        m_editing = true;
        m_edit->setText(locationNames().editText(m_url));
        if (auto *completion = qobject_cast<KUrlCompletion *>(m_edit->completionObject(false))) {
            completion->setDir(m_url.isLocalFile() ? m_url : QUrl());
        }
        relayout();
        m_edit->show();
    }
    m_edit->selectAll();
    m_edit->setFocus(Qt::ShortcutFocusReason);
    update();
}

void AddressBox::cancelEditing()
{
    if (!m_editing) {
        return;
    }
    m_editing = false;
    if (KCompletionBox *box = m_edit->completionBox(false)) {
        box->hide();
    }
    relayout();
    update();
}

void AddressBox::finishEditing()
{
    const QUrl target = locationNames().fromEditText(m_edit->text(), m_url);
    if (!target.isValid() || target.isEmpty()) {
        // Nowhere to go: the text stays for the user to fix.
        m_edit->selectAll();
        return;
    }
    cancelEditing();
    if (!target.matches(m_url, QUrl::StripTrailingSlash)) {
        Q_EMIT navigateRequested(target);
    }
}

void AddressBox::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    m_progress = -1;
    if (m_loadingAnimation) {
        m_loadingAnimation->stop();
        delete m_loadingAnimation;
    }
    if (loading && Ui::Motion::enabled()) {
        auto *animation = new QVariantAnimation(this);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        animation->setDuration(1400);
        animation->setLoopCount(-1);
        connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_phase = value.toReal();
            update();
        });
        animation->start();
        m_loadingAnimation = animation;
    }
    update();
}

void AddressBox::setLoadingProgress(int percent)
{
    if (!m_loading) {
        setLoading(true);
    }
    m_progress = percent < 0 ? -1 : std::min(percent, 100);
    if (m_progress >= 0 && m_loadingAnimation) {
        m_loadingAnimation->stop();
    }
    update();
}

QSize AddressBox::sizeHint() const
{
    return QSize(400, boxHeight + 2 * glowMargin);
}

QSize AddressBox::minimumSizeHint() const
{
    return QSize(160, boxHeight + 2 * glowMargin);
}

void AddressBox::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const TokenSet &t = Tokens::current();
    const QRectF box = QRectF(boxRect()).adjusted(0.5, 0.5, -0.5, -0.5);

    // The soft glow of the focused field, at night only.
    if (m_editing && t.night()) {
        QColor glow = t.color(TokenSet::glow);
        for (int k = glowMargin; k >= 1; --k) {
            glow.setAlphaF(t.color(TokenSet::glow).alphaF() * (1.0 - qreal(k) / (glowMargin + 1)));
            p.setPen(QPen(glow, 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(box.adjusted(-k, -k, k, k), radiusControl + k, radiusControl + k);
        }
    }

    QColor fill = t.color(m_editing ? TokenSet::fillInputActive : (m_hovered ? TokenSet::fillControlHover : TokenSet::fillControl));
    p.setPen(QPen(t.color(TokenSet::strokeControl), 1));
    p.setBrush(fill);
    p.drawRoundedRect(box, radiusControl, radiusControl);

    QPainterPath clip;
    clip.addRoundedRect(box, radiusControl, radiusControl);
    p.save();
    p.setClipPath(clip);
    if (m_editing) {
        p.fillRect(QRectF(box.left(), box.bottom() - focusStroke + 0.5, box.width(), focusStroke), t.color(TokenSet::accent));
    } else {
        p.fillRect(QRectF(box.left(), box.bottom(), box.width(), 1), t.color(TokenSet::strokeControlBottom));
    }

    if (m_loading) {
        const QRectF line(box.left(), box.bottom() - loadingLine + 0.5, box.width(), loadingLine);
        QColor accent = t.color(TokenSet::accent);
        if (m_progress >= 0) {
            p.fillRect(QRectF(line.left(), line.top(), line.width() * m_progress / 100.0, line.height()), accent);
        } else if (!m_loadingAnimation) {
            // Motion off: a still line says "busy" without moving.
            accent.setAlphaF(0.6);
            p.fillRect(line, accent);
        } else {
            const qreal segment = line.width() * 0.3;
            const qreal x = line.left() - segment + (line.width() + segment) * m_phase;
            QLinearGradient g(x, 0, x + segment, 0);
            QColor edge = t.color(TokenSet::glow);
            edge.setAlpha(0);
            g.setColorAt(0, edge);
            g.setColorAt(0.5, accent);
            g.setColorAt(1, edge);
            p.fillRect(QRectF(x, line.top(), segment, line.height()), g);
        }
    }
    p.restore();

    if (!m_iconName.isEmpty()) {
        const int s = Ui::Metrics::iconSize;
        const QRect iconRect(boxRect().left() + innerPadding, boxRect().center().y() - s / 2 + 1, s, s);
        themeIcon(m_iconName, QStringLiteral("folder")).paint(&p, iconRect);
    }
}

void AddressBox::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayout();
}

void AddressBox::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && boxRect().contains(event->position().toPoint())) {
        editLocation();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void AddressBox::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    if (Ui::FocusRing::isKeyboardReason(event->reason())) {
        editLocation();
    }
}

void AddressBox::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        QPalette pal = m_edit->palette();
        pal.setColor(QPalette::Base, Qt::transparent);
        pal.setColor(QPalette::Text, Tokens::current().color(TokenSet::textPrimary));
        m_edit->setPalette(pal);
    }
    QWidget::changeEvent(event);
}

void AddressBox::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void AddressBox::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

bool AddressBox::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit) {
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            KCompletionBox *box = m_edit->completionBox(false);
            if (!box || !box->isVisible()) {
                cancelEditing();
                return true;
            }
        } else if (event->type() == QEvent::FocusOut) {
            const auto reason = static_cast<QFocusEvent *>(event)->reason();
            KCompletionBox *box = m_edit->completionBox(false);
            if (reason != Qt::PopupFocusReason && (!box || !box->isVisible())) {
                cancelEditing();
            }
        } else if (event->type() == QEvent::Show) {
            QPalette pal = m_edit->palette();
            pal.setColor(QPalette::Base, Qt::transparent);
            pal.setColor(QPalette::Text, Tokens::current().color(TokenSet::textPrimary));
            m_edit->setPalette(pal);
        }
    }
    return QWidget::eventFilter(watched, event);
}

}
