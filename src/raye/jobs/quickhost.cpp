/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "quickhost.h"

#include "pages/thumbnails.h"
#include "ui/tokens.h"

#include <QLayout>
#include <QPointer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTimer>
#include <QtMath>

#include <utility>

#ifdef __GLIBC__
#include <malloc.h>
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
#include <KLocalizedContext>
QT_WARNING_POP

namespace Raye::Jobs
{

namespace
{
// Every page of the app (Home in each tab, This PC, the dialogs) runs in one
// engine: each QQuickWidget used to make its own, with its own copy of every
// type and of the theme, about 4 MB a page. It goes with the last page.
QQmlEngine *s_engine = nullptr;
int s_pages = 0;

QQmlEngine *pageEngine()
{
    if (!s_engine) {
        s_engine = new QQmlEngine;
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        auto *context = new KLocalizedContext(s_engine);
        QT_WARNING_POP
        context->setTranslationDomain(QStringLiteral("raye-files"));
        s_engine->rootContext()->setContextObject(context);
        s_engine->addImageProvider(QString::fromUtf16(Raye::Pages::ThumbnailProvider::id), new Raye::Pages::ThumbnailProvider);
    }
    return s_engine;
}

void holdEngine(QQuickWidget *view)
{
    ++s_pages;
    QObject::connect(view, &QObject::destroyed, [] {
        if (--s_pages == 0) {
            // a page asked for before it is gone gets a new one
            std::exchange(s_engine, nullptr)->deleteLater();
        }
#ifdef __GLIBC__
        // the heap keeps what the page freed otherwise, and the window
        // stays the size of every page it showed
        QTimer::singleShot(1000, [] {
            malloc_trim(0);
        });
#endif
    });
}
}

QQuickWidget *quickPage(const QString &type, const QVariantMap &properties, QWidget *parent, const char *clearToken)
{
    auto *view = new QQuickWidget(pageEngine(), parent);
    holdEngine(view);
    view->setClearColor(Raye::Ui::Tokens::current().color(QString::fromLatin1(clearToken)));
    view->loadFromModule("org.raye.files", type);
    QQuickItem *root = view->rootObject();
    if (!root) {
        for (const QQmlError &e : view->errors()) {
            qWarning("%s", qPrintable(e.toString()));
        }
        return view;
    }
    for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
        if (!root->setProperty(it.key().toUtf8().constData(), it.value())) {
            qWarning("%s has no property %s", qPrintable(type), qPrintable(it.key()));
        }
        if (QObject *object = it.value().value<QObject *>(); object && !object->parent()) {
            object->setParent(view);
        }
    }
    return view;
}

namespace
{
// Qt Quick layouts work out their size when polished, on the first frame.
// Polish now, children first, so the window has its real height before it
// maps instead of growing (or not) once it is up.
void polishTree(QQuickItem *item)
{
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        polishTree(child);
    }
    item->ensurePolished();
}
}

void fitToPage(QQuickWidget *view, int width, QWidget *resize)
{
    QQuickItem *root = view ? view->rootObject() : nullptr;
    if (!root) {
        return;
    }
    view->setResizeMode(QQuickWidget::SizeViewToRootObject);
    root->setWidth(width);
    polishTree(root);
    polishTree(root);
    root->setHeight(root->implicitHeight());
    QObject::connect(root, &QQuickItem::implicitHeightChanged, root, [root] {
        root->setHeight(root->implicitHeight());
    });
    QPointer<QWidget> target(resize);
    auto fit = [view, root, width, target] {
        view->setFixedSize(width, qMax(1, qCeil(root->height())));
        if (target) {
            // the new fixed size reaches the window's own limits first,
            // or adjustSize() is held to the old ones
            if (target->layout()) {
                target->layout()->activate();
            }
            target->adjustSize();
        }
    };
    QObject::connect(root, &QQuickItem::heightChanged, view, fit, Qt::QueuedConnection);
    fit();
}

}
