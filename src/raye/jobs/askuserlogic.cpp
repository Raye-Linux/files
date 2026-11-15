/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "askuserlogic.h"

#include <KConfigGroup>
#include <KLocalizedString>

#include <QMimeDatabase>
#include <QRegularExpression>

namespace Raye::Jobs
{

bool ConflictInfo::isFolderMerge() const
{
    return (options & KIO::RenameDialog_DestIsDirectory) && (options & KIO::RenameDialog_SourceIsDirectory);
}

bool ConflictInfo::isSameFile() const
{
    return options & KIO::RenameDialog_OverwriteItself;
}

bool ConflictInfo::canReplace() const
{
    return !isSameFile() && (options & KIO::RenameDialog_Overwrite);
}

bool ConflictInfo::canSkip() const
{
    return options & KIO::RenameDialog_Skip;
}

bool ConflictInfo::canKeepBoth() const
{
    return !(options & KIO::RenameDialog_NoRename);
}

bool ConflictInfo::multiple() const
{
    return options & KIO::RenameDialog_MultipleItems;
}

namespace
{
// The part of a name an index goes before: "a.tar.gz" → "a" + ".tar.gz".
void splitName(const QString &name, bool isFolder, QString *base, QString *suffix)
{
    *base = name;
    suffix->clear();
    if (isFolder || (name.startsWith(QLatin1Char('.')) && name.count(QLatin1Char('.')) == 1)) {
        return;
    }
    QString ext = QMimeDatabase().suffixForFileName(name);
    if (ext.isEmpty()) {
        const int dot = name.lastIndexOf(QLatin1Char('.'));
        if (dot > 0 && dot < name.size() - 1) {
            ext = name.mid(dot + 1);
        }
    }
    if (!ext.isEmpty() && name.size() > ext.size() + 1 && name.endsWith(QLatin1Char('.') + ext, Qt::CaseInsensitive)) {
        *base = name.left(name.size() - ext.size() - 1);
        *suffix = name.right(ext.size() + 1);
    }
}
}

QUrl keepBothName(const QUrl &destination, bool isFolder, const std::function<bool(const QUrl &)> &exists)
{
    const QUrl dir = destination.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    QString base;
    QString suffix;
    splitName(destination.fileName(), isFolder, &base, &suffix);
    // A name that already ends in " (N)" goes on from N.
    static const QRegularExpression numbered(QStringLiteral("^(.*) \\((\\d+)\\)$"));
    int n = 2;
    const QRegularExpressionMatch m = numbered.match(base);
    if (m.hasMatch()) {
        base = m.captured(1);
        n = qMax(2, m.captured(2).toInt() + 1);
    }
    for (; n < 10000; ++n) {
        QUrl candidate = dir;
        candidate.setPath(dir.path() + QLatin1Char('/') + QStringLiteral("%1 (%2)%3").arg(base).arg(n).arg(suffix));
        if (!exists(candidate)) {
            return candidate;
        }
    }
    return QUrl();
}

ConflictResult conflictResult(const ConflictInfo &info, const ConflictAnswer &answer, const std::function<bool(const QUrl &)> &exists)
{
    ConflictResult r;
    switch (answer.choice) {
    case ConflictChoice::Replace:
        if (!info.canReplace()) {
            break;
        }
        r.result = answer.applyToAll && info.multiple() ? KIO::Result_OverwriteAll : KIO::Result_Overwrite;
        return r;
    case ConflictChoice::Skip:
        if (!info.canSkip()) {
            break;
        }
        r.result = answer.applyToAll && info.multiple() ? KIO::Result_AutoSkip : KIO::Result_Skip;
        return r;
    case ConflictChoice::KeepBoth:
        if (!info.canKeepBoth()) {
            break;
        }
        if (answer.applyToAll && info.multiple()) {
            // KIO picks the names for the rest of the operation.
            r.result = KIO::Result_AutoRename;
            return r;
        }
        r.newDestination = keepBothName(info.destination, info.options & KIO::RenameDialog_SourceIsDirectory, exists);
        if (r.newDestination.isValid()) {
            r.result = KIO::Result_Rename;
            return r;
        }
        break;
    case ConflictChoice::Cancel:
        break;
    }
    r = ConflictResult();
    return r;
}

KIO::SkipDialog_Result skipResult(KIO::SkipDialog_Options options, const SkipAnswer &answer)
{
    if (answer.replaceInvalidChars && (options & KIO::SkipDialog_Replace_Invalid_Chars)) {
        return answer.applyToAll ? KIO::Result_ReplaceAllInvalidChars : KIO::Result_ReplaceInvalidChars;
    }
    switch (answer.choice) {
    case SkipChoice::TryAgain:
        if (options & KIO::SkipDialog_Hide_Retry) {
            return KIO::Result_Cancel;
        }
        return KIO::Result_Retry;
    case SkipChoice::Skip:
        return answer.applyToAll && (options & KIO::SkipDialog_MultipleItems) ? KIO::Result_AutoSkip : KIO::Result_Skip;
    case SkipChoice::Cancel:
        break;
    }
    return KIO::Result_Cancel;
}

bool needsConfirmation(DeletionType type, ConfirmationType confirmation, const KConfigGroup &confirmations)
{
    if (confirmation == KIO::AskUserActionInterface::ForceConfirmation) {
        return true;
    }
    switch (type) {
    case KIO::AskUserActionInterface::Delete:
        return confirmations.readEntry("ConfirmDelete", true);
    case KIO::AskUserActionInterface::Trash:
        return confirmations.readEntry("ConfirmTrash", false);
    case KIO::AskUserActionInterface::EmptyTrash:
        return confirmations.readEntry("ConfirmEmptyTrash", true);
    case KIO::AskUserActionInterface::DeleteInsteadOfTrash:
    case KIO::AskUserActionInterface::DeleteNoTrashAvailable:
        return true;
    }
    return true;
}

DeleteQuestion deleteQuestion(DeletionType type, int count, bool isFolder)
{
    DeleteQuestion q;
    const bool one = count <= 1;
    if (one) {
        q.title = isFolder ? i18nc("@title:window", "Delete folder") : i18nc("@title:window", "Delete file");
    } else {
        q.title = i18nc("@title:window", "Delete multiple items");
    }
    q.yes = i18nc("@action:button", "Yes");
    switch (type) {
    case KIO::AskUserActionInterface::Trash:
        if (one) {
            q.question = isFolder ? i18nc("@info", "Are you sure you want to move this folder to the Recycle Bin?")
                                  : i18nc("@info", "Are you sure you want to move this file to the Recycle Bin?");
        } else {
            q.question = i18ncp("@info", "Are you sure you want to move this item to the Recycle Bin?", "Are you sure you want to move these %1 items to the Recycle Bin?", count);
        }
        break;
    case KIO::AskUserActionInterface::DeleteInsteadOfTrash:
        q.question = i18ncp("@info",
                            "This item is too big for the Recycle Bin. Do you want to permanently delete it?",
                            "These %1 items are too big for the Recycle Bin. Do you want to permanently delete them?",
                            count);
        break;
    case KIO::AskUserActionInterface::DeleteNoTrashAvailable:
        q.question = i18ncp("@info",
                            "This item can't be moved to the Recycle Bin. Do you want to permanently delete it?",
                            "These %1 items can't be moved to the Recycle Bin. Do you want to permanently delete them?",
                            count);
        break;
    case KIO::AskUserActionInterface::EmptyTrash:
        q.title = i18nc("@title:window", "Empty Recycle Bin");
        if (count > 0) {
            q.question = i18ncp("@info", "Are you sure you want to permanently delete this item?", "Are you sure you want to permanently delete these %1 items?", count);
        } else {
            q.question = i18nc("@info", "Are you sure you want to permanently delete everything in the Recycle Bin?");
        }
        break;
    case KIO::AskUserActionInterface::Delete:
        if (one) {
            q.question = isFolder ? i18nc("@info", "Are you sure you want to permanently delete this folder?")
                                  : i18nc("@info", "Are you sure you want to permanently delete this file?");
        } else {
            q.question = i18ncp("@info", "Are you sure you want to permanently delete this item?", "Are you sure you want to permanently delete these %1 items?", count);
        }
        break;
    }
    return q;
}

int messageResult(KIO::AskUserActionInterface::MessageDialogType type, int button)
{
    using I = KIO::AskUserActionInterface;
    switch (type) {
    case I::QuestionTwoActions:
    case I::WarningTwoActions:
        return button == 0 ? MessagePrimary : MessageSecondary;
    case I::QuestionTwoActionsCancel:
    case I::WarningTwoActionsCancel:
        return button == 0 ? MessagePrimary : button == 1 ? MessageSecondary : MessageCancel;
    case I::WarningContinueCancel:
        return button == 0 ? MessageContinue : MessageCancel;
    case I::Information:
    case I::Error:
        return MessageOk;
    }
    return MessageCancel;
}

}
