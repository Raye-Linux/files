/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QWidget>

class QQuickWidget;

namespace Raye::Jobs
{

class JobsModel;

// The Raye progress window (explorer spec 3.13): 480 wide, on the window
// colour, the operations as stacked cards. Its title is the overall
// progress ("37% complete"), like Windows, so the taskbar says it too.
// "Fewer details" is remembered.
class DOLPHIN_EXPORT ProgressWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ProgressWindow(JobsModel *model, QWidget *parent = nullptr);

    // Shows the window (raised, not stealing focus from the file window).
    void showOperations();
    bool details() const;

private Q_SLOTS:
    void updateTitle();

private:

    JobsModel *m_model;
    QQuickWidget *m_view;
};

}
