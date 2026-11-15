/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QPointer>
#include <QUrl>
#include <QWidget>

#include <functional>

class QQuickWidget;

namespace Raye::Pages
{

// The pages Files shows in a tab instead of a folder view (explorer spec
// 3.9): raye:home, and raye:gallery, raye:thispc once their packages
// register them. The view container (FX-9) puts a PageHost in place of
// the Dolphin view for these URLs; history, the address row and the
// command bar treat them as locations.
class DOLPHIN_EXPORT PageHost : public QWidget
{
    Q_OBJECT
public:
    // Makes the model a page shows; parented to the host.
    using ModelFactory = std::function<QObject *(QObject *parent)>;

    struct Page {
        QString name; // "home", the part after "raye:"
        QString qmlType; // a type of org.raye.files
        QString title; // shown as the tab and window name
        QString iconName;
        ModelFactory model;
    };

    // A second page with the same name replaces the first.
    static void registerPage(const Page &page);
    static bool isPageUrl(const QUrl &url);
    // "home" for raye:home (and raye:/home); empty for other URLs.
    static QString pageName(const QUrl &url);
    static QString title(const QUrl &url);
    static QString iconName(const QUrl &url);
    static QUrl homeUrl();

    explicit PageHost(QWidget *parent = nullptr);

    // Shows the page of a raye: URL; false for anything else.
    bool setUrl(const QUrl &url);
    QUrl url() const
    {
        return m_url;
    }
    QQuickWidget *view() const
    {
        return m_view;
    }
    // The model of the page shown.
    QObject *model() const
    {
        return m_model;
    }

Q_SIGNALS:
    // A folder or file chosen on the page: the container opens it like a
    // double-click in a view (a folder in this tab, a file in its app).
    void openUrlRequested(const QUrl &url);
    // "Open file location": go to the folder and select the item.
    void openLocationRequested(const QUrl &folder, const QUrl &item);

protected:
    void focusInEvent(QFocusEvent *event) override;

private Q_SLOTS:
    void relayOpen(const QUrl &url);
    void relayOpenLocation(const QUrl &folder, const QUrl &item);
    void relayNewTab(const QUrl &url);
    void relayNewWindow(const QUrl &url);
    void relayProperties(const QUrl &url);

private:
    QUrl m_url;
    QQuickWidget *m_view = nullptr;
    QPointer<QObject> m_model;
};

}
