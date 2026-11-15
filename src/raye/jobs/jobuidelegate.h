/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <KIO/JobUiDelegate>
#include <KIO/JobUiDelegateFactory>

#include <functional>
#include <memory>

namespace Raye::Jobs
{

class Prompter;

// The UI delegate of the KIO jobs Files starts: KDE's widget delegate
// (clipboard updates, window tracking, error messages) with the Raye
// answers to its questions (AskUser) and the Raye "Open with" handler.
class DOLPHIN_EXPORT JobUiDelegate : public KIO::JobUiDelegate
{
    Q_OBJECT
public:
    explicit JobUiDelegate(KJobUiDelegate::Flags flags = AutoHandlingDisabled,
                           QWidget *window = nullptr,
                           std::shared_ptr<Prompter> prompter = {});
};

class DOLPHIN_EXPORT JobUiDelegateFactory : public KIO::JobUiDelegateFactory
{
public:
    KJobUiDelegate *createDelegate() const override;
    KJobUiDelegate *createDelegate(KJobUiDelegate::Flags flags, QWidget *window) const override;
};

// The prompter every new delegate gets; the Raye dialogs unless set.
DOLPHIN_EXPORT std::shared_ptr<Prompter> prompter();
DOLPHIN_EXPORT void setPrompter(std::shared_ptr<Prompter> prompter);

// Makes extra interfaces for each delegate (FX-20's Open with handler
// registers here). Each call of the maker gives a new object.
DOLPHIN_EXPORT void addDelegateInterface(const std::function<QObject *()> &maker);

// Raye dialogs and the progress window for every KIO job of this process.
// Files calls it once at start (it is also done by itself in the
// application called "dolphin"; RAYE_FILES_KDE_DIALOGS=1 keeps KDE's).
DOLPHIN_EXPORT void install();
DOLPHIN_EXPORT bool installed();

}
