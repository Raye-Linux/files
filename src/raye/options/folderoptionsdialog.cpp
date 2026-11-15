/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "folderoptionsdialog.h"

#include "folderoptions.h"
#include "jobs/quickhost.h"
#include "ui/carddialog.h"
#include "ui/commandbutton.h"
#include "ui/showcase.h"

#include <KLocalizedString>

#include <QPointer>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTimer>

namespace Raye::Options
{

using Raye::Ui::CardDialog;

CardDialog *folderOptionsDialog(QWidget *parent, int tab, FolderOptions *options)
{
    constexpr int bodyWidth = 500;
    auto *dialog = new CardDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFixedWidth(bodyWidth + 48);
    dialog->setTitle(i18nc("@title:window", "Folder options"));
    if (!options) {
        options = new FolderOptions;
    }
    options->setParent(dialog);
    QQuickWidget *view = Raye::Jobs::quickPage(QStringLiteral("FolderOptionsPage"),
                                               {{QStringLiteral("options"), QVariant::fromValue<QObject *>(options)}, {QStringLiteral("tab"), tab}},
                                               nullptr,
                                               "bgCardSolid");
    dialog->setContentWidget(view);
    Raye::Jobs::fitToPage(view, bodyWidth, dialog);

    dialog->setButtonText(CardDialog::Primary, i18nc("@action:button", "OK"));
    dialog->setButtonText(CardDialog::Secondary, i18nc("@action:button", "Apply"));
    dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
    dialog->setClosesOnButton(false);

    QPointer<CardDialog> guard(dialog);
    QPointer<FolderOptions> opts(options);
    auto updateApply = [guard, opts] {
        if (guard && opts) {
            if (auto *b = guard->button(CardDialog::Secondary)) {
                b->setEnabled(opts->dirty());
            }
        }
    };
    QObject::connect(options, &FolderOptions::changed, dialog, updateApply);
    updateApply();
    QObject::connect(dialog, &CardDialog::buttonClicked, dialog, [guard, opts](CardDialog::ButtonRole role) {
        if (!guard || !opts) {
            return;
        }
        switch (role) {
        case CardDialog::Primary:
            if (opts->apply()) {
                guard->accept();
            }
            break;
        case CardDialog::Secondary:
            opts->apply();
            break;
        case CardDialog::Close:
            guard->reject();
            break;
        }
    });
    return dialog;
}

}

RAYE_SHOWCASE(folderOptions, "FolderOptions", "Folder options, General")
{
    auto *host = new QWidget(parent);
    host->setAutoFillBackground(true);
    auto *dialog = Raye::Options::folderOptionsDialog(host, 0);
    QTimer::singleShot(0, dialog, [dialog] {
        dialog->show();
    });
    return host;
}

RAYE_SHOWCASE(folderOptionsView, "FolderOptionsView", "Folder options, View")
{
    auto *host = new QWidget(parent);
    host->setAutoFillBackground(true);
    auto *dialog = Raye::Options::folderOptionsDialog(host, 1);
    QTimer::singleShot(0, dialog, [dialog] {
        dialog->show();
    });
    return host;
}

RAYE_SHOWCASE(folderOptionsSearch, "FolderOptionsSearch", "Folder options, Search")
{
    auto *host = new QWidget(parent);
    host->setAutoFillBackground(true);
    auto *dialog = Raye::Options::folderOptionsDialog(host, 2);
    QTimer::singleShot(0, dialog, [dialog] {
        dialog->show();
    });
    return host;
}
