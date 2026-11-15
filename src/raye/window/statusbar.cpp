/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "statusbar.h"

#include "ui/commandbutton.h"
#include "ui/tokens.h"

#include <KLocalizedString>

#include <QIcon>
#include <QPainter>
#include <QPaintEvent>

using Raye::Ui::TokenSet;
using Raye::Ui::Tokens;

namespace Raye::Window
{
namespace
{
constexpr int sideMargin = 12;
constexpr int dividerGap = 10;
constexpr int toggleWidth = 24;
constexpr int toggleHeight = 20;

Ui::CommandButton *makeToggle(const char *icon, const QString &name, QWidget *parent)
{
    auto *b = new Ui::CommandButton(QIcon::fromTheme(QString::fromLatin1(icon)), name, parent);
    b->setIconOnly(true);
    b->setCompact(true);
    b->setCheckable(true);
    b->setToolTip(name);
    b->setFixedSize(toggleWidth, toggleHeight);
    return b;
}
}

StatusBar::StatusBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("RayeStatusBar"));
    setFixedHeight(height);
    m_details = makeToggle("view-list-details", i18nc("@action:button status bar", "Details"), this);
    m_large = makeToggle("view-list-icons", i18nc("@action:button status bar", "Large icons"), this);
    connect(m_details, &QAbstractButton::clicked, this, &StatusBar::detailsRequested);
    connect(m_large, &QAbstractButton::clicked, this, &StatusBar::largeIconsRequested);
}

void StatusBar::setCounts(const StatusCounts &counts)
{
    m_parts = statusParts(counts);
    m_message.clear();
    update();
}

void StatusBar::clear()
{
    m_parts.clear();
    m_message.clear();
    update();
}

void StatusBar::setMessage(const QString &message)
{
    m_message = message;
    update();
}

QString StatusBar::text() const
{
    return m_message.isEmpty() ? m_parts.join(QStringLiteral(" | ")) : m_message;
}

void StatusBar::setMode(Mode mode)
{
    m_details->setChecked(mode == Mode::Details);
    m_large->setChecked(mode == Mode::LargeIcons);
}

QSize StatusBar::sizeHint() const
{
    return QSize(400, height);
}

void StatusBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const int y = (height - toggleHeight) / 2;
    m_large->move(width() - sideMargin - toggleWidth, y);
    m_details->move(m_large->x() - 2 - toggleWidth, y);
}

void StatusBar::paintEvent(QPaintEvent *)
{
    const auto &t = Tokens::current();
    QPainter p(this);
    p.setFont(Ui::textFont(Ui::TextStyle::Caption));
    p.setPen(t.color(TokenSet::textSecondary));
    const QFontMetrics fm(p.font());
    const int right = m_details->x() - dividerGap;
    int x = sideMargin;
    const QStringList parts = m_message.isEmpty() ? m_parts : QStringList{m_message};
    for (int i = 0; i < parts.size() && x < right; ++i) {
        if (i > 0) {
            // A thin divider, like Windows draws between the counts.
            p.save();
            p.setPen(t.color(TokenSet::strokeDivider));
            p.drawLine(x, 7, x, height - 7);
            p.restore();
            x += dividerGap + 1;
        }
        const QString text = fm.elidedText(parts.at(i), Qt::ElideRight, right - x);
        p.drawText(QRect(x, 0, right - x, height), Qt::AlignVCenter | Qt::AlignLeft, text);
        x += fm.horizontalAdvance(text) + dividerGap;
    }
}
}
