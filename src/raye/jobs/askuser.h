/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "askuserlogic.h"

#include <KIO/AskUserActionInterface>

#include <QPointer>

#include <functional>
#include <memory>

class KJob;
class QWidget;

namespace KIO
{
class WidgetsAskUserActionHandler;
}

namespace Raye::Jobs
{

struct DOLPHIN_EXPORT SkipInfo {
    QString errorText;
    KIO::SkipDialog_Options options;
};

struct DOLPHIN_EXPORT MessageInfo {
    KIO::AskUserActionInterface::MessageDialogType type = KIO::AskUserActionInterface::Information;
    QString text;
    QString title;
    QString primaryText;
    QString secondaryText;
    QString details;
};

// Who answers the questions. The default one shows the Raye dialogs
// (dialogs.cpp); the tests give scripted answers. Every call answers
// through its callback exactly once, now or later.
class DOLPHIN_EXPORT Prompter
{
public:
    virtual ~Prompter();
    virtual void askConflict(KJob *job, QWidget *parent, const ConflictInfo &info, const std::function<void(const ConflictAnswer &)> &done) = 0;
    virtual void askSkip(KJob *job, QWidget *parent, const SkipInfo &info, const std::function<void(const SkipAnswer &)> &done) = 0;
    virtual void askDelete(const QList<QUrl> &urls, DeletionType type, QWidget *parent, const std::function<void(bool)> &done) = 0;
    // button: 0 primary, 1 secondary, 2 close
    virtual void askMessage(const MessageInfo &info, QWidget *parent, const std::function<void(int)> &done) = 0;
};

// KIO's questions answered the Raye way (explorer spec 3.13): KIO jobs that
// Files starts find it in their UI delegate (jobuidelegate.h). SSL questions
// and messages with a "don't ask again" box keep KDE's handler, which knows
// how to remember them.
class DOLPHIN_EXPORT AskUser : public KIO::AskUserActionInterface
{
    Q_OBJECT
public:
    explicit AskUser(std::shared_ptr<Prompter> prompter, QObject *parent = nullptr);
    ~AskUser() override;

    void askUserRename(KJob *job,
                       const QString &title,
                       const QUrl &src,
                       const QUrl &dest,
                       KIO::RenameDialog_Options options,
                       KIO::filesize_t sizeSrc,
                       KIO::filesize_t sizeDest,
                       const QDateTime &ctimeSrc = {},
                       const QDateTime &ctimeDest = {},
                       const QDateTime &mtimeSrc = {},
                       const QDateTime &mtimeDest = {}) override;
    void askUserSkip(KJob *job, KIO::SkipDialog_Options options, const QString &errorText) override;
    void askUserDelete(const QList<QUrl> &urls, DeletionType deletionType, ConfirmationType confirmationType, QWidget *parent = nullptr) override;
    void requestUserMessageBox(MessageDialogType type,
                               const QString &text,
                               const QString &title,
                               const QString &primaryActionText,
                               const QString &secondatyActionText,
                               const QString &primaryActionIconName = {},
                               const QString &secondatyActionIconName = {},
                               const QString &dontAskAgainName = {},
                               const QString &details = {},
                               QWidget *parent = nullptr) override;
    void askIgnoreSslErrors(const QVariantMap &sslErrorData, QWidget *parent) override;

    // Where kiorc is read from (tests point it at their own file).
    void setConfirmationsConfig(const QString &fileName);

private:
    KIO::WidgetsAskUserActionHandler *fallback();

    std::shared_ptr<Prompter> m_prompter;
    QPointer<KIO::WidgetsAskUserActionHandler> m_fallback;
    QString m_kiorc = QStringLiteral("kiorc");
};

}
