/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rightpane.h"

#include "preview/detailspane.h"
#include "preview/previewpane.h"
#include "ui/motion.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QPropertyAnimation>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Raye::Preview
{

namespace
{
constexpr int defaultWidth = 320;
constexpr int slideMs = 250;
}

RightPane::RightPane(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("RayeRightPane"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    m_details = new DetailsPane(m_stack);
    m_preview = new PreviewPane(m_stack);
    m_stack->addWidget(m_details);
    m_stack->addWidget(m_preview);
    layout->addWidget(m_stack);
    setMinimumWidth(240);
    setMaximumWidth(480);

    connect(m_details, &DetailsPane::urlActivated, this, &RightPane::urlActivated);
    connect(m_details, &DetailsPane::shareRequested, this, &RightPane::shareRequested);
    connect(m_details, &DetailsPane::openRequested, this, &RightPane::openRequested);

    const KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("dolphinrc"))->group(QStringLiteral("Raye"));
    m_mode = modeFromKey(group.readEntry("PreviewPane", QString()));
    m_stack->setCurrentWidget(m_mode == Preview ? static_cast<QWidget *>(m_preview) : m_details);
}

RightPane::~RightPane() = default;

QString RightPane::modeKey(Mode mode)
{
    return mode == Preview ? QStringLiteral("preview") : QStringLiteral("details");
}

RightPane::Mode RightPane::modeFromKey(const QString &key, Mode fallback)
{
    if (key == QLatin1String("preview")) {
        return Preview;
    }
    if (key == QLatin1String("details")) {
        return Details;
    }
    return fallback;
}

void RightPane::setMode(Mode mode)
{
    if (mode == m_mode) {
        return;
    }
    m_mode = mode;
    m_stack->setCurrentWidget(mode == Preview ? static_cast<QWidget *>(m_preview) : m_details);
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("dolphinrc"))->group(QStringLiteral("Raye"));
    group.writeEntry("PreviewPane", modeKey(mode), KConfig::Notify);
    group.sync();
    refresh();
    Q_EMIT modeChanged(mode);
}

RightPane::Mode RightPane::mode() const
{
    return m_mode;
}

DetailsPane *RightPane::detailsPane() const
{
    return m_details;
}

PreviewPane *RightPane::previewPane() const
{
    return m_preview;
}

void RightPane::showItems(const KFileItemList &selection, const KFileItem &folder)
{
    m_selection = selection;
    m_folder = folder;
    refresh();
}

void RightPane::refresh()
{
    // Only the pane on screen does any work: the other one is updated when
    // it comes up.
    if (m_mode == Details) {
        m_details->showItems(m_selection, m_folder);
    } else if (m_selection.size() == 1) {
        m_preview->showItem(m_selection.first());
    } else {
        m_preview->clear();
    }
}

QSize RightPane::sizeHint() const
{
    return QSize(defaultWidth, 480);
}

void RightPane::slideIn()
{
    if (m_slide) {
        m_slide->stop();
    }
    show();
    if (Raye::Ui::Motion::speedFactor() <= 0) {
        setMaximumWidth(480);
        return;
    }
    m_slide = new QPropertyAnimation(this, "maximumWidth", this);
    m_slide->setDuration(qRound(slideMs * Raye::Ui::Motion::speedFactor()));
    m_slide->setEasingCurve(QEasingCurve::OutCubic);
    m_slide->setStartValue(0);
    m_slide->setEndValue(qMax(width(), defaultWidth));
    connect(m_slide.data(), &QPropertyAnimation::finished, this, [this] {
        setMaximumWidth(480);
    });
    m_slide->start(QAbstractAnimation::DeleteWhenStopped);
}

void RightPane::slideOut()
{
    if (m_slide) {
        m_slide->stop();
    }
    if (Raye::Ui::Motion::speedFactor() <= 0 || !isVisible()) {
        hide();
        return;
    }
    m_slide = new QPropertyAnimation(this, "maximumWidth", this);
    m_slide->setDuration(qRound(slideMs * Raye::Ui::Motion::speedFactor()));
    m_slide->setEasingCurve(QEasingCurve::OutCubic);
    m_slide->setStartValue(width());
    m_slide->setEndValue(0);
    connect(m_slide.data(), &QPropertyAnimation::finished, this, [this] {
        hide();
        setMaximumWidth(480);
    });
    m_slide->start(QAbstractAnimation::DeleteWhenStopped);
}

}
