/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "askuser.h"

namespace Raye::Ui
{
class CardDialog;
}

namespace Raye::Jobs
{

// The Raye dialogs for KIO's questions (explorer spec 3.11, 3.13), on the
// Raye card dialog. Errors of a job shown in the progress window appear
// in its card instead of a dialog.
class DOLPHIN_EXPORT DialogPrompter : public Prompter
{
public:
    void askConflict(KJob *job, QWidget *parent, const ConflictInfo &info, const std::function<void(const ConflictAnswer &)> &done) override;
    void askSkip(KJob *job, QWidget *parent, const SkipInfo &info, const std::function<void(const SkipAnswer &)> &done) override;
    void askDelete(const QList<QUrl> &urls, DeletionType type, QWidget *parent, const std::function<void(bool)> &done) override;
    void askMessage(const MessageInfo &info, QWidget *parent, const std::function<void(int)> &done) override;
};

// The dialogs themselves, not shown yet; done is called once (Cancel and
// Esc included). Deleted on close.
DOLPHIN_EXPORT Raye::Ui::CardDialog *conflictDialog(QWidget *parent, const ConflictInfo &info, const std::function<void(const ConflictAnswer &)> &done);
DOLPHIN_EXPORT Raye::Ui::CardDialog *deleteDialog(QWidget *parent, const QList<QUrl> &urls, DeletionType type, const std::function<void(bool)> &done);
DOLPHIN_EXPORT Raye::Ui::CardDialog *skipDialog(QWidget *parent, const SkipInfo &info, const std::function<void(const SkipAnswer &)> &done);

}
