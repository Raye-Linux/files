/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include "ui/flyout.h"

#include <QAbstractItemModel>
#include <QPointer>
#include <QQuickWidget>
#include <QUrl>


namespace Raye::Share
{

class ShareModel;

// The Raye share flyout (explorer spec 3.11): 420 wide, acrylic, radius 8,
// anchored to the Share button or the menu that opened it. The files at the
// top, a "Share with" row of targets, then "More options" with every target
// Purpose knows. Drawn by the QML page org.raye.files.ShareSheet.
class DOLPHIN_EXPORT ShareFlyout : public Ui::Flyout
{
    Q_OBJECT
public:
    static constexpr int sheetWidth = 420;

    explicit ShareFlyout(QWidget *parent = nullptr);
    ~ShareFlyout() override;

    void setUrls(const QList<QUrl> &urls, const QStringList &mimeTypes = {});

    ShareModel *model() const
    {
        return m_model;
    }
    QQuickWidget *view() const
    {
        return m_view;
    }
    QAbstractItemModel *purposeModel() const
    {
        return m_purpose;
    }
    // False when the page could not load (it then says so, plainly).
    bool isReady() const;

    // Makes one, opens it at anchor and lets it go when it closes.
    static ShareFlyout *share(QWidget *anchor, const QList<QUrl> &urls, const QStringList &mimeTypes = {});

    // Where else QML modules are looked for (the build tree, for tests and
    // the render harness).
    static void addImportPath(const QString &path);

private Q_SLOTS:
    void sheetClosed();

private:
    void load();

    ShareModel *m_model;
    QPointer<QQuickWidget> m_view;
    QPointer<QAbstractItemModel> m_purpose;
};

}
