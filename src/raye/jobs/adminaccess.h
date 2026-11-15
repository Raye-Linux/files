/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QList>
#include <QString>
#include <QUrl>

#include <functional>

class KJob;
namespace KIO
{
class CopyJob;
}
namespace Raye::Ui
{
class CardDialog;
}
class QWidget;

// "You'll need to provide administrator permission": what Files does when
// a local file or folder needs root. Nothing here runs as root; the work is
// done again through kio-admin (admin:/), and its polkit prompt asks for
// the password.
namespace Raye::Jobs::AdminAccess
{

// kio-admin is installed.
DOLPHIN_EXPORT bool available();

// The KIO errors a local operation gets when the user lacks the rights.
DOLPHIN_EXPORT bool isAccessError(int kioError);

// file:///etc/fstab -> admin:/etc/fstab. Empty for anything not local.
DOLPHIN_EXPORT QUrl toAdmin(const QUrl &url);

// The folder new items go to: through kio-admin when it is a local folder
// the user can't write to.
DOLPHIN_EXPORT QUrl newItemFolder(const QUrl &folder);

// A local folder the user can't write to still takes the action when
// kio-admin can do it for him.
DOLPHIN_EXPORT bool allowsWrite(bool writable, bool local, bool adminAvailable);

enum class Kind {
    None,
    Copy,
    Move,
    Link,
    Delete,
    Rename,
    NewFolder,
};

// An operation Files started, so it can be done again as admin. For Copy,
// Move and Link the destination is the folder; for Rename it is the new
// name; for NewFolder it is the folder to create in.
struct DOLPHIN_EXPORT Operation {
    Kind kind = Kind::None;
    QList<QUrl> sources;
    QUrl destination;
};

// Whether a failed operation is offered again as admin.
DOLPHIN_EXPORT bool canRedo(const Operation &op, int kioError, bool adminAvailable);
DOLPHIN_EXPORT Operation asAdmin(const Operation &op);

// What the dialog says. continueText is the primary button; skipText is
// empty when skipping makes no sense.
struct DOLPHIN_EXPORT Question {
    QString title;
    QString message;
    QString continueText;
    QString skipText;
    QString cancelText;
};
DOLPHIN_EXPORT Question openQuestion(bool folder);
DOLPHIN_EXPORT Question redoQuestion(const Operation &op, bool folder, bool multiple);

enum class Answer {
    Continue,
    Skip,
    Cancel,
};

// The Raye card dialog for a question, not shown yet; done is called once
// (Esc and closing give Cancel).
DOLPHIN_EXPORT Raye::Ui::CardDialog *questionDialog(QWidget *parent, const Question &question, const std::function<void(Answer)> &done);
DOLPHIN_EXPORT void ask(QWidget *parent, const Question &question, const std::function<void(Answer)> &done);

// Keeps the operation on the job, for when it fails.
DOLPHIN_EXPORT void remember(KJob *job, const Operation &op);
// Copy, Move or Link with the copy job's sources and destination folder.
DOLPHIN_EXPORT Operation copyOperation(const KIO::CopyJob *job);
DOLPHIN_EXPORT Operation recall(const KJob *job);

// For a failed job Files remembered: asks, and on Continue starts it
// again as admin. False when the error is not about rights (show it).
DOLPHIN_EXPORT bool offerRedo(KJob *job, QWidget *window);

// Asked by a copy in the middle of several items: the destination folder
// is not writable. True when it took the question; done gets the answer
// (Continue has already started the copy again, the old one must cancel).
DOLPHIN_EXPORT bool offerRedoOnSkip(KJob *job, QWidget *window, const std::function<void(Answer)> &done);

DOLPHIN_EXPORT KJob *start(const Operation &adminOp, QWidget *window);

}
