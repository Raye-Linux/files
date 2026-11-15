/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "askuser.h"

#include "adminaccess.h"

#include <KConfigGroup>
#include <KIO/WidgetsAskUserActionHandler>
#include <KJob>
#include <KJobWidgets>
#include <KSharedConfig>

#include <QFileInfo>
#include <QTimer>
#include <QWidget>

namespace Raye::Jobs
{

Prompter::~Prompter() = default;

namespace
{
QWidget *windowOf(KJob *job)
{
    return job ? KJobWidgets::window(job) : nullptr;
}

bool localExists(const QUrl &url)
{
    // A remote destination is not looked at: if the name is taken after
    // all, KIO asks again and the next number is tried.
    return url.isLocalFile() && QFileInfo::exists(url.toLocalFile());
}
}

AskUser::AskUser(std::shared_ptr<Prompter> prompter, QObject *parent)
    : KIO::AskUserActionInterface(parent)
    , m_prompter(std::move(prompter))
{
}

AskUser::~AskUser() = default;

void AskUser::setConfirmationsConfig(const QString &fileName)
{
    m_kiorc = fileName;
}

KIO::WidgetsAskUserActionHandler *AskUser::fallback()
{
    if (!m_fallback) {
        // A child of this object, not of the delegate: KIO looks for the
        // interface among the delegate's direct children only.
        m_fallback = new KIO::WidgetsAskUserActionHandler(this);
        connect(m_fallback, &KIO::AskUserActionInterface::messageBoxResult, this, &KIO::AskUserActionInterface::messageBoxResult);
        connect(m_fallback, &KIO::AskUserActionInterface::askIgnoreSslErrorsResult, this, &KIO::AskUserActionInterface::askIgnoreSslErrorsResult);
    }
    return m_fallback;
}

void AskUser::askUserRename(KJob *job,
                            const QString &title,
                            const QUrl &src,
                            const QUrl &dest,
                            KIO::RenameDialog_Options options,
                            KIO::filesize_t sizeSrc,
                            KIO::filesize_t sizeDest,
                            const QDateTime &ctimeSrc,
                            const QDateTime &ctimeDest,
                            const QDateTime &mtimeSrc,
                            const QDateTime &mtimeDest)
{
    Q_UNUSED(ctimeSrc)
    Q_UNUSED(ctimeDest)
    ConflictInfo info;
    info.source = src;
    info.destination = dest;
    info.sourceSize = sizeSrc;
    info.destinationSize = sizeDest;
    info.sourceModified = mtimeSrc;
    info.destinationModified = mtimeDest;
    info.options = options;
    info.jobTitle = title;

    QPointer<KJob> guard(job);
    QPointer<AskUser> self(this);
    m_prompter->askConflict(job, windowOf(job), info, [guard, self, info](const ConflictAnswer &answer) {
        if (!self) {
            return;
        }
        const ConflictResult r = conflictResult(info, answer, localExists);
        // Never from inside the question: KIO may still be setting up.
        QTimer::singleShot(0, self, [self, guard, r] {
            if (self && guard) {
                Q_EMIT self->askUserRenameResult(r.result, r.newDestination, guard);
            }
        });
    });
}

void AskUser::askUserSkip(KJob *job, KIO::SkipDialog_Options options, const QString &errorText)
{
    QPointer<KJob> guard(job);
    QPointer<AskUser> self(this);
    // A copy into a folder that needs admin: Continue starts it again
    // through kio-admin and this one stops.
    const bool tookIt = AdminAccess::offerRedoOnSkip(job, windowOf(job), [guard, self](AdminAccess::Answer answer) {
        const KIO::SkipDialog_Result r = answer == AdminAccess::Answer::Skip ? KIO::Result_AutoSkip : KIO::Result_Cancel;
        QTimer::singleShot(0, self, [self, guard, r] {
            if (self && guard) {
                Q_EMIT self->askUserSkipResult(r, guard);
            }
        });
    });
    if (tookIt) {
        return;
    }
    m_prompter->askSkip(job, windowOf(job), SkipInfo{errorText, options}, [guard, self, options](const SkipAnswer &answer) {
        const KIO::SkipDialog_Result r = skipResult(options, answer);
        if (!self) {
            return;
        }
        QTimer::singleShot(0, self, [self, guard, r] {
            if (self && guard) {
                Q_EMIT self->askUserSkipResult(r, guard);
            }
        });
    });
}

void AskUser::askUserDelete(const QList<QUrl> &urls, DeletionType deletionType, ConfirmationType confirmationType, QWidget *parent)
{
    const KSharedConfig::Ptr config = KSharedConfig::openConfig(m_kiorc, KConfig::NoGlobals);
    if (!needsConfirmation(deletionType, confirmationType, config->group(QStringLiteral("Confirmations")))) {
        Q_EMIT askUserDeleteResult(true, urls, deletionType, parent);
        return;
    }
    QPointer<AskUser> self(this);
    QPointer<QWidget> parentGuard(parent);
    m_prompter->askDelete(urls, deletionType, parent, [self, urls, deletionType, parentGuard](bool allow) {
        if (self) {
            Q_EMIT self->askUserDeleteResult(allow, urls, deletionType, parentGuard);
        }
    });
}

void AskUser::requestUserMessageBox(MessageDialogType type,
                                    const QString &text,
                                    const QString &title,
                                    const QString &primaryActionText,
                                    const QString &secondatyActionText,
                                    const QString &primaryActionIconName,
                                    const QString &secondatyActionIconName,
                                    const QString &dontAskAgainName,
                                    const QString &details,
                                    QWidget *parent)
{
    if (!dontAskAgainName.isEmpty()) {
        fallback()->requestUserMessageBox(type,
                                          text,
                                          title,
                                          primaryActionText,
                                          secondatyActionText,
                                          primaryActionIconName,
                                          secondatyActionIconName,
                                          dontAskAgainName,
                                          details,
                                          parent);
        return;
    }
    MessageInfo info{type, text, title, primaryActionText, secondatyActionText, details};
    QPointer<AskUser> self(this);
    m_prompter->askMessage(info, parent, [self, type](int button) {
        if (self) {
            Q_EMIT self->messageBoxResult(messageResult(type, button));
        }
    });
}

void AskUser::askIgnoreSslErrors(const QVariantMap &sslErrorData, QWidget *parent)
{
    fallback()->askIgnoreSslErrors(sslErrorData, parent);
}

}
