/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "carddialog.h"
#include "commandbutton.h"
#include "focusring.h"
#include "shadow.h"
#include "tokens.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWindow>

namespace Raye::Ui
{

namespace
{
constexpr int s_titleGap = 12; // between the title and the content
}

CardDialog::CardDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_bodyLayout(new QVBoxLayout)
    , m_buttonLayout(new QHBoxLayout)
    , m_buttonRow(new QWidget(this))
    , m_title(new QLabel(this))
    , m_translucent(compositorActive())
{
    setAttribute(Qt::WA_TranslucentBackground, m_translucent);
    setObjectName(QStringLiteral("RayeCardDialog"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->setSizeConstraint(QLayout::SetMinAndMaxSize);

    m_bodyLayout->setContentsMargins(Metrics::dialogPadding, Metrics::dialogPadding, Metrics::dialogPadding, Metrics::dialogPadding);
    m_bodyLayout->setSpacing(s_titleGap);
    m_title->setFont(textFont(TextStyle::Subtitle));
    m_title->setWordWrap(true);
    m_title->setTextFormat(Qt::PlainText);
    m_title->hide();
    m_bodyLayout->addWidget(m_title);
    outer->addLayout(m_bodyLayout, 1);

    m_buttonRow->setFixedHeight(Metrics::dialogButtonRow);
    m_buttonRow->setLayout(m_buttonLayout);
    m_buttonLayout->setContentsMargins(Metrics::dialogPadding, 0, Metrics::dialogPadding, 0);
    m_buttonLayout->setSpacing(Metrics::dialogButtonGap);
    m_buttonRow->hide();
    outer->addWidget(m_buttonRow);

    const auto followTokens = [this]() {
        QPalette p = palette();
        p.setColor(QPalette::WindowText, Tokens::current().color(TokenSet::textPrimary));
        p.setColor(QPalette::Text, Tokens::current().color(TokenSet::textPrimary));
        setPalette(p);
        if (isVisible()) {
            applyWindowEffects();
        }
        update();
    };
    connect(Tokens::instance(), &Tokens::changed, this, followTokens);
    followTokens();
}

CardDialog::~CardDialog() = default;

QString CardDialog::title() const
{
    return m_title->text();
}

void CardDialog::setTitle(const QString &title)
{
    m_title->setText(title);
    m_title->setVisible(!title.isEmpty());
    setWindowTitle(title);
    setAccessibleName(title);
}

void CardDialog::setContentWidget(QWidget *content)
{
    if (m_content == content) {
        return;
    }
    if (m_content) {
        m_bodyLayout->removeWidget(m_content);
        m_content->deleteLater();
    }
    m_content = content;
    if (content) {
        content->setParent(this);
        m_bodyLayout->addWidget(content, 1);
    }
}

void CardDialog::setMessage(const QString &message)
{
    auto *label = new QLabel(message);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    label->setFont(textFont(TextStyle::Body));
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setContentWidget(label);
}

CommandButton *CardDialog::button(ButtonRole role) const
{
    return m_buttons.at(role);
}

CommandButton *CardDialog::setButtonText(ButtonRole role, const QString &text)
{
    QPointer<CommandButton> &slot = m_buttons.at(role);
    if (text.isEmpty()) {
        delete slot.data();
        slot = nullptr;
        relayoutButtons();
        return nullptr;
    }
    if (!slot) {
        slot = new CommandButton(m_buttonRow);
        slot->setKind(CommandButton::Standard);
        // The row shares its width out: every button the same size.
        slot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(slot, &QAbstractButton::clicked, this, [this, role]() {
            press(role);
        });
    }
    slot->setText(text);
    relayoutButtons();
    return slot;
}

void CardDialog::setDefaultButton(ButtonRole role)
{
    m_default = role;
    relayoutButtons();
}

void CardDialog::relayoutButtons()
{
    while (m_buttonLayout->count() > 0) {
        delete m_buttonLayout->takeAt(0);
    }
    bool any = false;
    for (int r = Primary; r <= Close; ++r) {
        CommandButton *b = m_buttons.at(r);
        if (!b) {
            continue;
        }
        any = true;
        const bool accent = r == m_default && r != Close;
        b->setKind(accent ? CommandButton::Accent : CommandButton::Standard);
        m_buttonLayout->addWidget(b, 1);
    }
    m_buttonRow->setVisible(any);
    updateGeometry();
    update();
}

void CardDialog::press(ButtonRole role)
{
    Q_EMIT buttonClicked(role);
    if (!m_autoClose) {
        return;
    }
    switch (role) {
    case Primary:
        done(PrimaryChosen);
        break;
    case Secondary:
        done(SecondaryChosen);
        break;
    case Close:
        reject();
        break;
    }
}

void CardDialog::setTranslucent(bool translucent)
{
    m_translucent = translucent;
    setAttribute(Qt::WA_TranslucentBackground, translucent);
    update();
}

QSize CardDialog::sizeHint() const
{
    QSize hint = QDialog::sizeHint();
    hint.setWidth(qBound(minimumWidth, hint.width(), maximumWidth));
    return hint;
}

void CardDialog::setVisible(bool visible)
{
    QDialog::setVisible(visible);
    // QDialog hands the first button the focus "by Tab". As in Windows, the
    // ring only shows once the user moves with the keyboard.
    if (visible && focusWidget()) {
        if (FocusRing *ring = FocusRing::of(focusWidget())) {
            ring->hideRing();
        }
    }
}

int CardDialog::buttonRowTop() const
{
    return m_buttonRow->isVisible() ? m_buttonRow->geometry().top() : height();
}

void CardDialog::paintEvent(QPaintEvent *)
{
    const TokenSet &t = Tokens::current();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal radius = m_translucent ? Metrics::radiusOverlay : 0;
    const QRectF card = QRectF(rect());

    if (m_translucent) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }
    QPainterPath shape;
    shape.addRoundedRect(card, radius, radius);
    painter.save();
    painter.setClipPath(shape);
    painter.fillRect(card, t.color(TokenSet::bgCardSolid));
    const int rowTop = buttonRowTop();
    if (rowTop < height()) {
        painter.fillRect(QRectF(0, rowTop, width(), height() - rowTop), t.color(TokenSet::bgBase));
        painter.fillRect(QRectF(0, rowTop, width(), 1), t.color(TokenSet::strokeCardSolid));
    }
    painter.restore();

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(t.color(TokenSet::strokeSurfaceSolid), 1));
    painter.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), qMax(0.0, radius - 0.5), qMax(0.0, radius - 0.5));
}

void CardDialog::keyPressEvent(QKeyEvent *event)
{
    const bool plain = event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::KeypadModifier;
    if (plain && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        // A button with the focus takes Enter itself; what reaches us comes
        // from a field or a check box. A text box that takes Enter for a
        // new line keeps it.
        QWidget *focus = focusWidget();
        const bool multiLine = qobject_cast<QPlainTextEdit *>(focus) || qobject_cast<QTextEdit *>(focus);
        CommandButton *main = m_default == Close ? nullptr : m_buttons.at(m_default).data();
        if (!multiLine && main && main->isEnabled()) {
            main->animateClick();
            event->accept();
            return;
        }
    }
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        // Esc is the Close button, even when that button is not shown.
        Q_EMIT buttonClicked(Close);
        reject();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void CardDialog::mousePressEvent(QMouseEvent *event)
{
    // The card has no title bar: its body above the button row moves it.
    if (event->button() == Qt::LeftButton && event->position().y() < buttonRowTop() && windowHandle()) {
        windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void CardDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    applyWindowEffects();
    // Focus starts on the default button, as in Windows.
    CommandButton *main = m_buttons.at(m_default);
    if (!main) {
        for (const auto &b : m_buttons) {
            if (b) {
                main = b;
                break;
            }
        }
    }
    if (main && (!focusWidget() || !m_content || !m_content->isAncestorOf(focusWidget()))) {
        main->setFocus(Qt::OtherFocusReason);
    }
}

void CardDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (isVisible()) {
        applyWindowEffects();
    }
}

void CardDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LayoutDirectionChange || event->type() == QEvent::EnabledChange) {
        update();
    }
    QDialog::changeEvent(event);
}

void CardDialog::applyWindowEffects()
{
    if (!m_translucent || !windowHandle()) {
        return;
    }
    // The card itself is opaque: no blur, only the dialog shadow.
    m_shadow = Shadow::apply(this, Metrics::radiusOverlay, Elevation::Dialog, m_shadow);
}

}
