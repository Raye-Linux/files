/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <KIO/OpenWithHandlerInterface>
#include <KService>

#include <QList>
#include <QStringList>
#include <QUrl>

#include <functional>

class QWidget;

namespace Raye::Ui
{
class CardDialog;
}

namespace Raye::OpenWith
{

// What the dialog answers: the app (null when cancelled) and whether it
// becomes the default for the type ("Always").
using Chosen = std::function<void(const KService::Ptr &service, bool always)>;

// "How do you want to open this file?" (explorer spec 3.15): the file at
// the top, the recommended apps, "More apps", "Look for an app on this PC",
// "Find an app in the Store", then Always, Just once and Cancel. "Always"
// writes mimeapps.list before done is called. Deleted on close.
// preferred: desktop ids (with or without .desktop) that go at the top of the
// recommended apps, the first one picked: what the portal asks for.
DOLPHIN_EXPORT Raye::Ui::CardDialog *openWithDialog(QWidget *parent, const QList<QUrl> &urls, const QString &mime, const Chosen &done, const QStringList &preferred = {});

// For `dolphin --choose-app MIME` and Raye Settings' Default apps: the
// same dialog for a type, no file; it only sets the default. Returns false
// for an unknown type.
DOLPHIN_EXPORT bool chooseDefaultApp(const QString &mime, QWidget *parent = nullptr);

// KIO's "Open with" question answered by the Raye dialog; KIO jobs of
// Files find it in their UI delegate.
class DOLPHIN_EXPORT OpenWithHandler : public KIO::OpenWithHandlerInterface
{
    Q_OBJECT
public:
    explicit OpenWithHandler(QObject *parent = nullptr);
    void promptUserForApplication(KJob *job, const QList<QUrl> &urls, const QString &mimeType) override;
};

}
