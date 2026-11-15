/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "adminaccess.h"

#include "ui/carddialog.h"
#include "ui/showcase.h"

#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KIO/FileUndoManager>
#include <KIO/Global>
#include <KIO/JobUiDelegate>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KProtocolInfo>

#include <QFileInfo>
#include <QPointer>
#include <QTimer>
#include <QVariant>

#include <memory>
#include <utility>

namespace Raye::Jobs::AdminAccess
{

namespace
{
const char kindProperty[] = "rayeAdminKind";
const char sourcesProperty[] = "rayeAdminSources";
const char destinationProperty[] = "rayeAdminDestination";

bool isDir(const QUrl &url)
{
    return url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir();
}
}

bool available()
{
    return KProtocolInfo::isKnownProtocol(QStringLiteral("admin"));
}

bool isAccessError(int kioError)
{
    switch (kioError) {
    case KIO::ERR_ACCESS_DENIED:
    case KIO::ERR_WRITE_ACCESS_DENIED:
    case KIO::ERR_CANNOT_ENTER_DIRECTORY:
    case KIO::ERR_CANNOT_OPEN_FOR_READING:
        return true;
    default:
        return false;
    }
}

QUrl toAdmin(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return QUrl();
    }
    QUrl admin;
    admin.setScheme(QStringLiteral("admin"));
    admin.setPath(url.toLocalFile());
    return admin;
}

QUrl newItemFolder(const QUrl &folder)
{
    if (folder.isLocalFile() && !QFileInfo(folder.toLocalFile()).isWritable() && available()) {
        return toAdmin(folder);
    }
    return folder;
}

bool allowsWrite(bool writable, bool local, bool adminAvailable)
{
    return writable || (local && adminAvailable);
}

bool canRedo(const Operation &op, int kioError, bool adminAvailable)
{
    if (!adminAvailable || op.kind == Kind::None || !isAccessError(kioError) || op.sources.isEmpty()) {
        return false;
    }
    const bool needsDestination = op.kind != Kind::Delete;
    if (needsDestination && !op.destination.isLocalFile()) {
        return false;
    }
    for (const QUrl &url : op.sources) {
        if (!url.isLocalFile()) {
            return false;
        }
    }
    return true;
}

Operation asAdmin(const Operation &op)
{
    Operation admin;
    admin.kind = op.kind;
    for (const QUrl &url : op.sources) {
        admin.sources.append(toAdmin(url));
    }
    admin.destination = toAdmin(op.destination);
    return admin;
}

Question openQuestion(bool folder)
{
    Question q;
    q.title = folder ? i18nc("@title:window", "Folder access") : i18nc("@title:window", "File access");
    q.message = folder ? i18nc("@info", "You don't currently have permission to access this folder.\n\nSelect Continue to get access.")
                       : i18nc("@info", "You don't currently have permission to access this file.\n\nSelect Continue to get access.");
    q.continueText = i18nc("@action:button", "Continue");
    q.cancelText = i18nc("@action:button", "Cancel");
    return q;
}

Question redoQuestion(const Operation &op, bool folder, bool multiple)
{
    Question q;
    q.continueText = i18nc("@action:button", "Continue");
    q.skipText = i18nc("@action:button", "Skip");
    q.cancelText = i18nc("@action:button", "Cancel");
    const QString itemTitle = folder && !multiple ? i18nc("@title:window", "Folder Access Denied") : i18nc("@title:window", "File Access Denied");
    switch (op.kind) {
    case Kind::Copy:
        q.title = i18nc("@title:window", "Destination Folder Access Denied");
        q.message = i18nc("@info", "You'll need to provide administrator permission to copy to this folder.");
        break;
    case Kind::Move:
        q.title = i18nc("@title:window", "Destination Folder Access Denied");
        q.message = i18nc("@info", "You'll need to provide administrator permission to move to this folder.");
        break;
    case Kind::Link:
        q.title = i18nc("@title:window", "Destination Folder Access Denied");
        q.message = i18nc("@info", "You'll need to provide administrator permission to create a link in this folder.");
        break;
    case Kind::NewFolder:
        q.title = i18nc("@title:window", "Destination Folder Access Denied");
        q.message = i18nc("@info", "You'll need to provide administrator permission to create this folder.");
        q.skipText.clear();
        break;
    case Kind::Delete:
        q.title = itemTitle;
        q.message = multiple ? i18nc("@info", "You'll need to provide administrator permission to delete these items.")
            : folder         ? i18nc("@info", "You'll need to provide administrator permission to delete this folder.")
                             : i18nc("@info", "You'll need to provide administrator permission to delete this file.");
        break;
    case Kind::Rename:
        q.title = itemTitle;
        q.message = folder ? i18nc("@info", "You'll need to provide administrator permission to rename this folder.")
                           : i18nc("@info", "You'll need to provide administrator permission to rename this file.");
        q.skipText.clear();
        break;
    case Kind::None:
        break;
    }
    return q;
}

Raye::Ui::CardDialog *questionDialog(QWidget *parent, const Question &question, const std::function<void(Answer)> &done)
{
    auto *dialog = new Raye::Ui::CardDialog(parent);
    dialog->setTitle(question.title);
    dialog->setMessage(question.message);
    // The text column of Files' delete and copy dialogs.
    dialog->contentWidget()->setFixedWidth(472);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setButtonText(Raye::Ui::CardDialog::Primary, question.continueText);
    if (!question.skipText.isEmpty()) {
        dialog->setButtonText(Raye::Ui::CardDialog::Secondary, question.skipText);
    }
    dialog->setButtonText(Raye::Ui::CardDialog::Close, question.cancelText);
    dialog->setDefaultButton(Raye::Ui::CardDialog::Primary);

    auto pending = std::make_shared<std::function<void(Answer)>>(done);
    QObject::connect(dialog, &Raye::Ui::CardDialog::buttonClicked, dialog, [pending](Raye::Ui::CardDialog::ButtonRole role) {
        if (auto f = std::exchange(*pending, nullptr)) {
            f(role == Raye::Ui::CardDialog::Primary ? Answer::Continue : role == Raye::Ui::CardDialog::Secondary ? Answer::Skip : Answer::Cancel);
        }
    });
    QObject::connect(dialog, &QObject::destroyed, [pending] {
        if (auto f = std::exchange(*pending, nullptr)) {
            f(Answer::Cancel);
        }
    });
    return dialog;
}

void ask(QWidget *parent, const Question &question, const std::function<void(Answer)> &done)
{
    questionDialog(parent, question, done)->show();
}

void remember(KJob *job, const Operation &op)
{
    if (!job) {
        return;
    }
    job->setProperty(kindProperty, int(op.kind));
    job->setProperty(sourcesProperty, QVariant::fromValue(op.sources));
    job->setProperty(destinationProperty, op.destination);
}

Operation recall(const KJob *job)
{
    Operation op;
    if (!job) {
        return op;
    }
    op.kind = Kind(job->property(kindProperty).toInt());
    op.sources = job->property(sourcesProperty).value<QList<QUrl>>();
    op.destination = job->property(destinationProperty).toUrl();
    return op;
}

Operation copyOperation(const KIO::CopyJob *job)
{
    Operation op;
    switch (job->operationMode()) {
    case KIO::CopyJob::Copy:
        op.kind = Kind::Copy;
        break;
    case KIO::CopyJob::Move:
        op.kind = Kind::Move;
        break;
    case KIO::CopyJob::Link:
        op.kind = Kind::Link;
        break;
    }
    op.sources = job->srcUrls();
    op.destination = job->destUrl();
    return op;
}

bool offerRedo(KJob *job, QWidget *window)
{
    const Operation op = recall(job);
    if (!canRedo(op, job->error(), available())) {
        return false;
    }
    const bool folder = op.sources.size() == 1 && isDir(op.sources.first());
    QPointer<QWidget> guard(window);
    ask(window, redoQuestion(op, folder, op.sources.size() > 1), [op, guard](Answer answer) {
        if (answer == Answer::Continue) {
            start(asAdmin(op), guard);
        }
    });
    return true;
}

bool offerRedoOnSkip(KJob *job, QWidget *window, const std::function<void(Answer)> &done)
{
    const Operation op = recall(job);
    if (job->property("rayeAdminAsked").toBool() || (op.kind != Kind::Copy && op.kind != Kind::Move && op.kind != Kind::Link)
        || !canRedo(op, KIO::ERR_WRITE_ACCESS_DENIED, available()) || QFileInfo(op.destination.toLocalFile()).isWritable()) {
        return false;
    }
    job->setProperty("rayeAdminAsked", true);
    QPointer<QWidget> guard(window);
    ask(window, redoQuestion(op, false, true), [op, guard, done](Answer answer) {
        if (answer == Answer::Continue) {
            start(asAdmin(op), guard);
        }
        done(answer);
    });
    return true;
}

KJob *start(const Operation &adminOp, QWidget *window)
{
    KIO::Job *job = nullptr;
    switch (adminOp.kind) {
    case Kind::Copy:
    case Kind::Move:
    case Kind::Link: {
        KIO::CopyJob *copy = adminOp.kind == Kind::Copy ? KIO::copy(adminOp.sources, adminOp.destination)
            : adminOp.kind == Kind::Move                ? KIO::move(adminOp.sources, adminOp.destination)
                                                        : KIO::link(adminOp.sources, adminOp.destination);
        KIO::FileUndoManager::self()->recordCopyJob(copy);
        job = copy;
        break;
    }
    case Kind::Delete:
        job = KIO::del(adminOp.sources);
        break;
    case Kind::Rename:
        job = KIO::moveAs(adminOp.sources.first(), adminOp.destination);
        KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Rename, adminOp.sources, adminOp.destination, job);
        break;
    case Kind::NewFolder:
    case Kind::None:
        return nullptr;
    }
    if (window) {
        KJobWidgets::setWindow(job, window);
    }
    job->uiDelegate()->setAutoErrorHandlingEnabled(true);
    return job;
}

}

namespace
{
QWidget *frame(QWidget *parent, QDialog *dialog)
{
    QTimer::singleShot(0, dialog, [dialog] {
        dialog->show();
    });
    auto *host = new QWidget(parent);
    host->setAutoFillBackground(true);
    return host;
}
}

using namespace Raye::Jobs::AdminAccess;

RAYE_SHOWCASE(adminOpenFile, "AdminOpenFile", "Open a file that needs admin")
{
    return frame(parent, questionDialog(parent, openQuestion(false), [](Answer) { }));
}

RAYE_SHOWCASE(adminOpenFolder, "AdminOpenFolder", "Open a folder that needs admin")
{
    return frame(parent, questionDialog(parent, openQuestion(true), [](Answer) { }));
}

RAYE_SHOWCASE(adminCopy, "AdminCopy", "Copy into a folder that needs admin")
{
    const Operation op{Kind::Copy, {QUrl::fromLocalFile(QStringLiteral("/home/ann/hosts"))}, QUrl::fromLocalFile(QStringLiteral("/etc"))};
    return frame(parent, questionDialog(parent, redoQuestion(op, false, false), [](Answer) { }));
}

RAYE_SHOWCASE(adminNewFolder, "AdminNewFolder", "New folder in a folder that needs admin")
{
    const Operation op{Kind::NewFolder, {}, QUrl::fromLocalFile(QStringLiteral("/opt"))};
    return frame(parent, questionDialog(parent, redoQuestion(op, true, false), [](Answer) { }));
}
