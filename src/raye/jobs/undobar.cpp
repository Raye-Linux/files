/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "undobar.h"

#include "ui/commandbutton.h"
#include "ui/showcase.h"
#include "ui/tokens.h"

#include <KLocalizedString>

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>

namespace Raye::Jobs
{

QString deletedText(const QList<QUrl> &urls)
{
    if (urls.size() == 1) {
        return i18nc("@info %1 is a file name", "Deleted %1", urls.first().fileName());
    }
    return i18ncp("@info", "Deleted %1 item", "Deleted %1 items", urls.size());
}

UndoBar::UndoBar(QWidget *parent)
    : QWidget(parent)
    , m_label(new QLabel(this))
    , m_undo(new Raye::Ui::CommandButton(this))
{
    setObjectName(QStringLiteral("RayeUndoBar"));
    setAttribute(Qt::WA_TranslucentBackground);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 8, 8, 8);
    layout->setSpacing(12);
    m_label->setTextFormat(Qt::PlainText);
    layout->addWidget(m_label, 1);
    m_undo->setText(i18nc("@action:button", "Undo"));
    layout->addWidget(m_undo);
    connect(m_undo, &QAbstractButton::clicked, this, [this] {
        hide();
        Q_EMIT undoRequested();
    });
    m_timer.setSingleShot(true);
    m_timer.setInterval(visibleMs);
    connect(&m_timer, &QTimer::timeout, this, &QWidget::hide);
    setFixedHeight(48);
    hide();
}

void UndoBar::showDeleted(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return;
    }
    m_label->setText(deletedText(urls));
    show();
    raise();
    m_timer.start();
}

QString UndoBar::text() const
{
    return m_label->text();
}

void UndoBar::enterEvent(QEnterEvent *event)
{
    // Not while the pointer is on it: it would go as you reach Undo.
    m_timer.stop();
    QWidget::enterEvent(event);
}

void UndoBar::leaveEvent(QEvent *event)
{
    if (isVisible()) {
        m_timer.start();
    }
    QWidget::leaveEvent(event);
}

void UndoBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    const Raye::Ui::TokenSet &t = Raye::Ui::Tokens::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    p.fillPath(path, t.color(Raye::Ui::TokenSet::bgCardSolid));
    p.setPen(QPen(t.color(Raye::Ui::TokenSet::strokeSurface), 1));
    p.drawPath(path);
}

}

RAYE_SHOWCASE(undoBar, "UndoBar", "Deleted report.pdf, Undo")
{
    auto *host = new QWidget(parent);
    auto *bar = new Raye::Jobs::UndoBar(host);
    bar->setGeometry(24, 24, 360, 48);
    bar->showDeleted({QUrl::fromLocalFile(QStringLiteral("/home/ann/report.pdf"))});
    return host;
}
