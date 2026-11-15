/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <KIO/AskUserActionInterface>
#include <kio/jobuidelegateextension.h>

#include <QDateTime>
#include <QList>
#include <QString>
#include <QUrl>

#include <functional>

class KConfigGroup;

// The rules behind the Raye "Replace or skip files", error and delete
// dialogs (explorer spec 3.11, 3.13), kept apart from the dialogs so the
// tests can check every answer KIO gets back.
namespace Raye::Jobs
{

// What a KIO job asks when the destination already has the name.
struct DOLPHIN_EXPORT ConflictInfo {
    QUrl source;
    QUrl destination;
    KIO::filesize_t sourceSize = 0;
    KIO::filesize_t destinationSize = 0;
    QDateTime sourceModified;
    QDateTime destinationModified;
    KIO::RenameDialog_Options options;
    QString jobTitle;

    // Copying a folder onto a folder: Windows asks to merge them.
    bool isFolderMerge() const;
    // A file copied onto itself: only Skip makes sense.
    bool isSameFile() const;
    bool canReplace() const;
    bool canSkip() const;
    bool canKeepBoth() const;
    // Several items in the operation: "Do this for the next conflicts".
    bool multiple() const;
};

enum class ConflictChoice {
    Replace, // or Merge, for folders
    Skip,
    KeepBoth, // "Compare info" with both columns ticked
    Cancel,
};

struct DOLPHIN_EXPORT ConflictAnswer {
    ConflictChoice choice = ConflictChoice::Cancel;
    // "Do this for the next N conflicts"
    bool applyToAll = false;
};

// The answer KIO wants, and the new name for Keep both (empty otherwise).
// Skip with a single item (KIO offers no skip then) cancels: the file in
// the destination stays as it is either way.
struct DOLPHIN_EXPORT ConflictResult {
    KIO::RenameDialog_Result result = KIO::Result_Cancel;
    QUrl newDestination;
};

// exists() is asked for each candidate name in the destination's folder.
DOLPHIN_EXPORT ConflictResult conflictResult(const ConflictInfo &info, const ConflictAnswer &answer, const std::function<bool(const QUrl &)> &exists);

// "report (2).pdf", "report (3).pdf"..., keeping a double extension like
// .tar.gz whole and folders without an extension. exists() decides.
DOLPHIN_EXPORT QUrl keepBothName(const QUrl &destination, bool isFolder, const std::function<bool(const QUrl &)> &exists);

// A file error during a copy ("You don't have permission...").
enum class SkipChoice {
    TryAgain,
    Skip,
    Cancel,
};
struct DOLPHIN_EXPORT SkipAnswer {
    SkipChoice choice = SkipChoice::Cancel;
    bool applyToAll = false;
    bool replaceInvalidChars = false;
};
DOLPHIN_EXPORT KIO::SkipDialog_Result skipResult(KIO::SkipDialog_Options options, const SkipAnswer &answer);

// Delete confirmation.
using DeletionType = KIO::AskUserActionInterface::DeletionType;
using ConfirmationType = KIO::AskUserActionInterface::ConfirmationType;

// Whether to ask at all: Force always asks; otherwise kiorc
// [Confirmations] ConfirmDelete (default on), ConfirmTrash (off),
// ConfirmEmptyTrash (on). Deleting because the Recycle Bin can't take the
// files always asks.
DOLPHIN_EXPORT bool needsConfirmation(DeletionType type, ConfirmationType confirmation, const KConfigGroup &confirmations);

struct DOLPHIN_EXPORT DeleteQuestion {
    QString title; // "Delete file", "Delete multiple items"
    QString question; // "Are you sure you want to permanently delete this file?"
    QString yes; // the label of the button that deletes
};
// isFolder describes the single item when there is one.
DOLPHIN_EXPORT DeleteQuestion deleteQuestion(DeletionType type, int count, bool isFolder);

// The KMessageDialog::ButtonType numbers KIO's messageBoxResult() carries.
enum MessageResult {
    MessageOk = 1,
    MessageCancel = 2,
    MessagePrimary = 3,
    MessageSecondary = 4,
    MessageContinue = 5,
};
// Which button was pressed (0 primary, 1 secondary, 2 close / Esc) to the
// number KIO expects for that kind of message.
DOLPHIN_EXPORT int messageResult(KIO::AskUserActionInterface::MessageDialogType type, int button);

}
