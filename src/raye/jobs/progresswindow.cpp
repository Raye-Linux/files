/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "progresswindow.h"

#include "askuserlogic.h"
#include "jobsmodel.h"
#include "quickhost.h"
#include "ui/showcase.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QQuickItem>
#include <QQuickWidget>
#include <QVBoxLayout>

#include <cmath>

namespace Raye::Jobs
{

namespace
{
constexpr int windowWidth = 480;

KConfigGroup stateGroup()
{
    return KSharedConfig::openStateConfig()->group(QStringLiteral("Raye Progress"));
}
}

ProgressWindow::ProgressWindow(JobsModel *model, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_model(model)
{
    setObjectName(QStringLiteral("RayeProgressWindow"));
    setAttribute(Qt::WA_ShowWithoutActivating);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_view = quickPage(QStringLiteral("ProgressPage"),
                       {{QStringLiteral("model"), QVariant::fromValue<QObject *>(model)},
                        {QStringLiteral("details"), stateGroup().readEntry("Details", true)}},
                       this);
    layout->addWidget(m_view);
    fitToPage(m_view, windowWidth, this);
    if (QQuickItem *root = m_view->rootObject()) {
        // updateTitle() also remembers the details choice.
        connect(root, SIGNAL(detailsToggled(bool)), this, SLOT(updateTitle()));
    }
    connect(model, &JobsModel::overallChanged, this, &ProgressWindow::updateTitle);
    updateTitle();
}

bool ProgressWindow::details() const
{
    QQuickItem *root = m_view->rootObject();
    return root ? root->property("details").toBool() : true;
}

void ProgressWindow::updateTitle()
{
    stateGroup().writeEntry("Details", details());
    setWindowTitle(i18nc("@title:window progress of file operations", "%1% complete", m_model->overallPercent()));
}

void ProgressWindow::showOperations()
{
    if (!isVisible()) {
        show();
    }
    raise();
}

}

RAYE_SHOWCASE(progressWindow, "ProgressWindow", "Copy progress with two stacked operations")
{
    using Raye::Jobs::JobsModel;
    auto *model = new JobsModel;
    JobsModel::Row copy;
    copy.op = Raye::Jobs::Operation::Copy;
    copy.from = QUrl::fromLocalFile(QStringLiteral("/home/ann/Downloads"));
    copy.to = QUrl::fromLocalFile(QStringLiteral("/media/ann/USB Drive"));
    copy.itemsTotal = 1204;
    copy.itemsDone = 446;
    copy.bytesTotal = 4718592000ULL;
    copy.bytesDone = 1745879040ULL;
    copy.percent = 37;
    copy.canPause = true;
    copy.currentName = QStringLiteral("Holiday 2026 - 0418.jpg");
    for (int i = 0; i < 120; ++i) {
        const double wave = 100.0 + 18.0 * std::sin(i / 6.0) + 9.0 * std::sin(i / 2.3);
        copy.fixedHistory.append(wave * 1024 * 1024 * qMin(1.0, i / 12.0));
    }
    copy.fixedSpeed = 112.0 * 1024 * 1024;
    copy.fixedRemaining = 27000;
    model->addRow(copy);
    JobsModel::Row move;
    move.op = Raye::Jobs::Operation::Move;
    move.from = QUrl::fromLocalFile(QStringLiteral("/home/ann/Pictures"));
    move.to = QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents"));
    move.itemsTotal = 3;
    move.percent = 12;
    move.canPause = true;
    move.currentName = QStringLiteral("report.pdf");
    move.error = QStringLiteral("You don't have permission to copy to this folder.");
    move.errorMultiple = true;
    move.errorAnswer = [](const Raye::Jobs::SkipAnswer &) { };
    model->addRow(move);
    auto *window = new Raye::Jobs::ProgressWindow(model, nullptr);
    model->setParent(window);
    // In the harness the window is the content of its frame.
    window->setWindowFlags(Qt::Widget);
    window->setParent(parent);
    return window;
}
