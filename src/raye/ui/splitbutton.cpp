/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "splitbutton.h"
#include "focusring.h"

#include <KLocalizedString>

#include <QHBoxLayout>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMenu>

namespace Raye::Ui
{

SplitButton::SplitButton(QWidget *parent)
    : QWidget(parent)
    , m_main(new CommandButton(this))
    , m_more(new CommandButton(this))
{
    init();
}

SplitButton::SplitButton(const QIcon &icon, const QString &text, QWidget *parent)
    : SplitButton(parent)
{
    m_main->setIcon(icon);
    m_main->setText(text);
}

void SplitButton::init()
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_main);
    layout->addWidget(m_more);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_main->setJoin(CommandButton::JoinRight);
    m_more->setJoin(CommandButton::JoinLeft);
    m_more->setShowChevron(true);
    m_more->setFocusPolicy(Qt::NoFocus);
    m_more->setText(QString());
    m_more->setAccessibleName(i18nc("@action:button the menu part of a split button", "More options"));
    m_more->setToolTip(i18nc("@info:tooltip the menu part of a split button", "More options"));
    setFocusProxy(m_main);

    // One ring around both parts, shown while the main part has the focus.
    FocusRing::detach(m_main);
    FocusRing::detach(m_more);
    FocusRing::attach(this);
    m_main->installEventFilter(this);
    connect(m_main, &QAbstractButton::clicked, this, &SplitButton::clicked);
}

void SplitButton::setKind(CommandButton::Kind kind)
{
    m_main->setKind(kind);
    m_more->setKind(kind);
    // Only the main part is a Tab stop, whatever the kind says.
    m_more->setFocusPolicy(Qt::NoFocus);
}

void SplitButton::setText(const QString &text)
{
    m_main->setText(text);
    m_main->updateGeometry();
}

void SplitButton::setIcon(const QIcon &icon)
{
    m_main->setIcon(icon);
    m_main->updateGeometry();
}

void SplitButton::setMenu(QMenu *menu)
{
    m_more->setMenu(menu);
}

QMenu *SplitButton::menu() const
{
    return m_more->menu();
}

void SplitButton::showMenu()
{
    if (!menu()) {
        return;
    }
    m_more->showMenu();
    if (!menu()->actions().isEmpty()) {
        menu()->setActiveAction(menu()->actions().constFirst());
    }
}

bool SplitButton::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_main && event->type() == QEvent::FocusIn) {
        if (FocusRing::isKeyboardReason(static_cast<QFocusEvent *>(event)->reason())) {
            FocusRing::of(this)->showRing();
        }
    } else if (watched == m_main && event->type() == QEvent::FocusOut) {
        FocusRing::of(this)->hideRing();
    } else if (watched == m_main && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        const bool altDown = key->key() == Qt::Key_Down && (key->modifiers() & Qt::AltModifier);
        if (altDown || key->key() == Qt::Key_F4) {
            showMenu();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SplitButton::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange) {
        update();
    }
    QWidget::changeEvent(event);
}

}
