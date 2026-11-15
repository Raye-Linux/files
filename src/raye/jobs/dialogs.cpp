/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dialogs.h"
#include "dialogs_p.h"

#include "jobsmodel.h"
#include "jobuidelegate.h"
#include "jobtracker.h"
#include "progresswindow.h"
#include "quickhost.h"
#include "ui/carddialog.h"
#include "ui/commandbutton.h"
#include "ui/showcase.h"

#include <KFormat>
#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTimer>

#include <memory>

namespace Raye::Jobs
{

using Raye::Ui::CardDialog;

namespace
{
constexpr int bodyWidth = 472;

QString sizeText(KIO::filesize_t bytes)
{
    return KFormat().formatByteSize(double(bytes), 1, KFormat::JEDECBinaryDialect);
}

QString dateText(const QDateTime &date)
{
    return date.isValid() ? QLocale().toString(date.toLocalTime(), QLocale::ShortFormat) : QString();
}

QString iconFor(const QUrl &url, bool folder)
{
    if (folder) {
        return QStringLiteral("folder");
    }
    return QMimeDatabase().mimeTypeForFile(url.isLocalFile() ? url.toLocalFile() : url.fileName(), QMimeDatabase::MatchExtension).iconName();
}

// Pictures show themselves; anything else shows its icon.
QString thumbnailFor(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return QString();
    }
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(url.toLocalFile(), QMimeDatabase::MatchExtension);
    if (mime.name().startsWith(QLatin1String("image/")) && mime.name() != QLatin1String("image/svg+xml")) {
        return url.toString();
    }
    return QString();
}

CardDialog *newDialog(QWidget *parent, const QString &title)
{
    auto *dialog = new CardDialog(parent);
    dialog->setTitle(title);
    dialog->setFixedWidth(bodyWidth + 48);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    return dialog;
}

QQuickWidget *bodyOf(CardDialog *dialog, const QString &type, const QVariantMap &properties)
{
    QQuickWidget *view = quickPage(type, properties, nullptr, "bgCardSolid");
    dialog->setContentWidget(view);
    fitToPage(view, bodyWidth, dialog);
    return view;
}

// Calls done once: from a button, or with the fallback when the dialog
// goes away any other way.
template<typename T>
std::shared_ptr<std::function<void(const T &)>> once(QObject *owner, const std::function<void(const T &)> &done, const T &fallback)
{
    auto pending = std::make_shared<std::function<void(const T &)>>(done);
    QObject::connect(owner, &QObject::destroyed, [pending, fallback] {
        if (*pending) {
            auto f = std::move(*pending);
            *pending = nullptr;
            f(fallback);
        }
    });
    return pending;
}

template<typename T>
void answer(const std::shared_ptr<std::function<void(const T &)>> &pending, const T &value)
{
    if (*pending) {
        auto f = std::move(*pending);
        *pending = nullptr;
        f(value);
    }
}

QVariantMap side(const QUrl &url, KIO::filesize_t size, const QDateTime &date, bool folder, bool newer, bool larger)
{
    return {
        {QStringLiteral("name"), url.fileName()},
        {QStringLiteral("icon"), iconFor(url, folder)},
        {QStringLiteral("thumbnail"), thumbnailFor(url)},
        {QStringLiteral("size"), folder ? QString() : sizeText(size)},
        {QStringLiteral("date"), dateText(date)},
        {QStringLiteral("newer"), newer},
        {QStringLiteral("larger"), larger},
    };
}
}

CardDialog *conflictDialog(QWidget *parent, const ConflictInfo &info, const std::function<void(const ConflictAnswer &)> &done)
{
    const bool merge = info.isFolderMerge();
    CardDialog *dialog = newDialog(parent, merge ? i18nc("@title:window", "Merge folders") : i18nc("@title:window", "Replace or skip files"));
    const bool folder = info.options & KIO::RenameDialog_SourceIsDirectory;
    const bool srcNewer = info.sourceModified.isValid() && info.destinationModified.isValid() && info.sourceModified > info.destinationModified;
    const bool dstNewer = info.sourceModified.isValid() && info.destinationModified.isValid() && info.destinationModified > info.sourceModified;
    const QVariantMap properties{
        {QStringLiteral("mode"), merge ? QStringLiteral("merge") : info.isSameFile() ? QStringLiteral("same") : QStringLiteral("file")},
        {QStringLiteral("headline"), info.jobTitle},
        {QStringLiteral("fileName"), info.destination.fileName()},
        {QStringLiteral("sourceFolder"), folderName(info.source.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash))},
        {QStringLiteral("destinationFolder"), folderName(info.destination.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash))},
        {QStringLiteral("canReplace"), info.canReplace()},
        // Skip is always offered, as on Windows: with a single item KIO has
        // nothing else to do and the answer becomes a quiet cancel.
        {QStringLiteral("canSkip"), true},
        {QStringLiteral("canKeepBoth"), info.canKeepBoth()},
        {QStringLiteral("multiple"), info.multiple()},
        {QStringLiteral("source"), side(info.source, info.sourceSize, info.sourceModified, folder, srcNewer, info.sourceSize > info.destinationSize)},
        {QStringLiteral("destination"), side(info.destination, info.destinationSize, info.destinationModified, folder, dstNewer, info.destinationSize > info.sourceSize)},
    };
    QQuickWidget *view = bodyOf(dialog, QStringLiteral("ConflictPage"), properties);
    dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
    dialog->setDefaultButton(CardDialog::Close);
    auto pending = once<ConflictAnswer>(dialog, done, ConflictAnswer{ConflictChoice::Cancel, false});
    QQuickItem *root = view->rootObject();
    if (!root) {
        return dialog;
    }
    QPointer<QQuickItem> page(root);
    auto *relay = new PageRelay(dialog);
    QObject::connect(root, SIGNAL(chosen(int, bool)), relay, SLOT(chosen(int, bool)));
    QObject::connect(root, SIGNAL(comparingChanged()), relay, SLOT(comparingChanged()));
    QPointer<CardDialog> guard(dialog);
    relay->onChosen = [guard, pending](int choice, bool applyToAll) {
        ConflictAnswer a;
        a.choice = choice == 0 ? ConflictChoice::Replace : choice == 1 ? ConflictChoice::Skip : ConflictChoice::KeepBoth;
        a.applyToAll = applyToAll;
        answer(pending, a);
        if (guard) {
            guard->accept();
        }
    };
    // The compare view: Continue reads the two check boxes.
    relay->onComparing = [guard, page] {
        if (!guard || !page || !page->property("comparing").toBool()) {
            return;
        }
        guard->setButtonText(CardDialog::Primary, i18nc("@action:button", "Continue"));
        guard->setDefaultButton(CardDialog::Primary);
    };
    QObject::connect(dialog, &CardDialog::buttonClicked, relay, [page, pending](CardDialog::ButtonRole role) {
        if (role != CardDialog::Primary || !page) {
            return;
        }
        const bool keepSource = page->property("keepSource").toBool();
        const bool keepDestination = page->property("keepDestination").toBool();
        ConflictAnswer a;
        a.applyToAll = page->property("applyToAll").toBool();
        if (keepSource && keepDestination) {
            a.choice = ConflictChoice::KeepBoth;
        } else if (keepSource) {
            a.choice = ConflictChoice::Replace;
        } else {
            // Nothing ticked is read as "keep what is there", like Windows.
            a.choice = ConflictChoice::Skip;
        }
        answer(pending, a);
    });
    return dialog;
}

CardDialog *deleteDialog(QWidget *parent, const QList<QUrl> &urls, DeletionType type, const std::function<void(bool)> &done)
{
    bool isFolder = false;
    QVariantMap properties{{QStringLiteral("count"), int(urls.size())}};
    if (urls.size() == 1) {
        const QUrl url = urls.first();
        const QFileInfo fi(url.isLocalFile() ? url.toLocalFile() : QString());
        isFolder = url.isLocalFile() && fi.isDir();
        const QMimeType mime = isFolder ? QMimeDatabase().mimeTypeForName(QStringLiteral("inode/directory"))
                                        : QMimeDatabase().mimeTypeForFile(url.isLocalFile() ? url.toLocalFile() : url.fileName(), QMimeDatabase::MatchExtension);
        properties.insert(QStringLiteral("name"), url.fileName());
        properties.insert(QStringLiteral("iconName"), isFolder ? QStringLiteral("folder") : mime.iconName());
        properties.insert(QStringLiteral("thumbnail"), thumbnailFor(url));
        properties.insert(QStringLiteral("typeText"), isFolder ? i18nc("@label item type", "File folder") : mime.comment());
        if (fi.exists() && !isFolder) {
            properties.insert(QStringLiteral("sizeText"), sizeText(fi.size()));
        }
        if (fi.exists()) {
            properties.insert(QStringLiteral("dateText"), dateText(fi.lastModified()));
        }
    }
    const DeleteQuestion q = deleteQuestion(type, int(urls.size()), isFolder);
    properties.insert(QStringLiteral("question"), q.question);
    CardDialog *dialog = newDialog(parent, q.title);
    bodyOf(dialog, QStringLiteral("DeletePage"), properties);
    dialog->setButtonText(CardDialog::Primary, q.yes);
    dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "No"));
    // Yes is the default button, as on Windows: the question itself is the
    // safety, and Esc or No keeps the files.
    dialog->setDefaultButton(CardDialog::Primary);
    auto pending = once<bool>(dialog, done, false);
    QObject::connect(dialog, &CardDialog::buttonClicked, dialog, [pending](CardDialog::ButtonRole role) {
        answer(pending, role == CardDialog::Primary);
    });
    return dialog;
}

CardDialog *skipDialog(QWidget *parent, const SkipInfo &info, const std::function<void(const SkipAnswer &)> &done)
{
    CardDialog *dialog = newDialog(parent, i18nc("@title:window", "Something went wrong"));
    QQuickWidget *view = bodyOf(dialog,
                                QStringLiteral("ErrorPage"),
                                {{QStringLiteral("message"), info.errorText}, {QStringLiteral("multiple"), bool(info.options & KIO::SkipDialog_MultipleItems)}});
    QPointer<QQuickItem> page(view->rootObject());
    if (!(info.options & KIO::SkipDialog_Hide_Retry)) {
        dialog->setButtonText(CardDialog::Primary, i18nc("@action:button", "Try again"));
    }
    dialog->setButtonText(CardDialog::Secondary, i18nc("@action:button", "Skip"));
    dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
    if (info.options & KIO::SkipDialog_Hide_Retry) {
        dialog->setDefaultButton(CardDialog::Secondary);
    }
    auto pending = once<SkipAnswer>(dialog, done, SkipAnswer{});
    QObject::connect(dialog, &CardDialog::buttonClicked, dialog, [pending, page](CardDialog::ButtonRole role) {
        SkipAnswer a;
        a.choice = role == CardDialog::Primary ? SkipChoice::TryAgain : role == CardDialog::Secondary ? SkipChoice::Skip : SkipChoice::Cancel;
        a.applyToAll = page && page->property("applyToAll").toBool();
        answer(pending, a);
    });
    return dialog;
}

void DialogPrompter::askConflict(KJob *job, QWidget *parent, const ConflictInfo &info, const std::function<void(const ConflictAnswer &)> &done)
{
    Q_UNUSED(job)
    CardDialog *dialog = conflictDialog(parent, info, done);
    dialog->show();
}

void DialogPrompter::askSkip(KJob *job, QWidget *parent, const SkipInfo &info, const std::function<void(const SkipAnswer &)> &done)
{
    JobTracker *tracker = installed() ? JobTracker::instance() : nullptr;
    if (tracker && tracker->model()->showError(job, info.errorText, !(info.options & KIO::SkipDialog_Hide_Retry), info.options & KIO::SkipDialog_MultipleItems, done)) {
        tracker->window()->showOperations();
        return;
    }
    skipDialog(parent, info, done)->show();
}

void DialogPrompter::askDelete(const QList<QUrl> &urls, DeletionType type, QWidget *parent, const std::function<void(bool)> &done)
{
    CardDialog *dialog = deleteDialog(parent, urls, type, done);
    dialog->show();
}

void DialogPrompter::askMessage(const MessageInfo &info, QWidget *parent, const std::function<void(int)> &done)
{
    CardDialog *dialog = newDialog(parent, info.title.isEmpty() ? i18nc("@title:window", "Files") : info.title);
    QString text = info.text;
    if (!info.details.isEmpty()) {
        text += QLatin1String("\n\n") + info.details;
    }
    dialog->setMessage(text);
    using I = KIO::AskUserActionInterface;
    switch (info.type) {
    case I::QuestionTwoActions:
    case I::WarningTwoActions:
        dialog->setButtonText(CardDialog::Primary, info.primaryText);
        dialog->setButtonText(CardDialog::Secondary, info.secondaryText);
        break;
    case I::QuestionTwoActionsCancel:
    case I::WarningTwoActionsCancel:
        dialog->setButtonText(CardDialog::Primary, info.primaryText);
        dialog->setButtonText(CardDialog::Secondary, info.secondaryText);
        dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
        break;
    case I::WarningContinueCancel:
        dialog->setButtonText(CardDialog::Primary, info.primaryText.isEmpty() ? i18nc("@action:button", "Continue") : info.primaryText);
        dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
        break;
    case I::Information:
    case I::Error:
        dialog->setButtonText(CardDialog::Primary, i18nc("@action:button", "OK"));
        break;
    }
    // A two-action question with no Cancel answers "secondary" on Esc.
    const int fallback = (info.type == I::QuestionTwoActions || info.type == I::WarningTwoActions) ? 1 : 2;
    auto pending = once<int>(dialog, done, fallback);
    QObject::connect(dialog, &CardDialog::buttonClicked, dialog, [pending](CardDialog::ButtonRole role) {
        answer(pending, role == CardDialog::Primary ? 0 : role == CardDialog::Secondary ? 1 : 2);
    });
    dialog->show();
}

}

// Showcases for the render harness: fixed questions, nothing is copied.
namespace
{
QWidget *frame(QWidget *parent, QDialog *dialog)
{
    // The harness shoots floating dialogs over its window.
    QTimer::singleShot(0, dialog, [dialog] {
        dialog->show();
    });
    auto *host = new QWidget(parent);
    host->setAutoFillBackground(true);
    return host;
}

Raye::Jobs::ConflictInfo sampleConflict()
{
    Raye::Jobs::ConflictInfo info;
    info.source = QUrl::fromLocalFile(QStringLiteral("/home/ann/Downloads/Budget 2026.xlsx"));
    info.destination = QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents/Budget 2026.xlsx"));
    info.sourceSize = 48230;
    info.destinationSize = 41984;
    info.sourceModified = QDateTime(QDate(2026, 10, 30), QTime(16, 12));
    info.destinationModified = QDateTime(QDate(2026, 9, 2), QTime(9, 40));
    info.options = KIO::RenameDialog_Overwrite | KIO::RenameDialog_Skip | KIO::RenameDialog_MultipleItems;
    info.jobTitle = Raye::Jobs::headline(Raye::Jobs::Operation::Copy, 12, QStringLiteral("Downloads"), QStringLiteral("Documents"));
    return info;
}
}

RAYE_SHOWCASE(conflictDialog, "ConflictDialog", "Replace or skip files")
{
    return frame(parent, Raye::Jobs::conflictDialog(parent, sampleConflict(), [](const Raye::Jobs::ConflictAnswer &) { }));
}

RAYE_SHOWCASE(conflictCompare, "ConflictCompare", "Replace or skip files, compare view")
{
    Raye::Ui::CardDialog *dialog = Raye::Jobs::conflictDialog(parent, sampleConflict(), [](const Raye::Jobs::ConflictAnswer &) { });
    if (auto *view = qobject_cast<QQuickWidget *>(dialog->contentWidget()); view && view->rootObject()) {
        view->rootObject()->setProperty("comparing", true);
    }
    return frame(parent, dialog);
}

RAYE_SHOWCASE(folderMergeDialog, "FolderMergeDialog", "Merge two folders of the same name")
{
    Raye::Jobs::ConflictInfo info = sampleConflict();
    info.source = QUrl::fromLocalFile(QStringLiteral("/home/ann/Downloads/Holiday photos"));
    info.destination = QUrl::fromLocalFile(QStringLiteral("/home/ann/Pictures/Holiday photos"));
    info.options |= KIO::RenameDialog_SourceIsDirectory | KIO::RenameDialog_DestIsDirectory;
    return frame(parent, Raye::Jobs::conflictDialog(parent, info, [](const Raye::Jobs::ConflictAnswer &) { }));
}

RAYE_SHOWCASE(deleteDialog, "DeleteDialog", "Permanently delete one file")
{
    return frame(parent,
                 Raye::Jobs::deleteDialog(parent,
                                          {QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents/report.pdf"))},
                                          KIO::AskUserActionInterface::Delete,
                                          [](bool) { }));
}

RAYE_SHOWCASE(deleteDialogMany, "DeleteDialogMany", "Permanently delete several items")
{
    QList<QUrl> urls;
    for (int i = 1; i <= 7; ++i) {
        urls.append(QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents/file %1.txt").arg(i)));
    }
    return frame(parent, Raye::Jobs::deleteDialog(parent, urls, KIO::AskUserActionInterface::Delete, [](bool) { }));
}

RAYE_SHOWCASE(skipDialog, "CopyErrorDialog", "A copy error with Try again / Skip / Cancel")
{
    Raye::Jobs::SkipInfo info{QStringLiteral("You don't have permission to copy to this folder."), KIO::SkipDialog_MultipleItems};
    return frame(parent, Raye::Jobs::skipDialog(parent, info, [](const Raye::Jobs::SkipAnswer &) { }));
}
