/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "quickhost.h"

#include "pages/thumbnails.h"
#include "ui/tokens.h"

#include <QPointer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QtMath>

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
#include <KLocalizedContext>
QT_WARNING_POP

namespace Raye::Jobs
{

QQuickWidget *quickPage(const QString &type, const QVariantMap &properties, QWidget *parent, const char *clearToken)
{
    auto *view = new QQuickWidget(parent);
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    auto *context = new KLocalizedContext(view->engine());
    QT_WARNING_POP
    context->setTranslationDomain(QStringLiteral("raye-files"));
    view->engine()->rootContext()->setContextObject(context);
    view->engine()->addImageProvider(QString::fromUtf16(Raye::Pages::ThumbnailProvider::id), new Raye::Pages::ThumbnailProvider);
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

void fitToPage(QQuickWidget *view, int width, QWidget *resize)
{
    QQuickItem *root = view ? view->rootObject() : nullptr;
    if (!root) {
        return;
    }
    view->setResizeMode(QQuickWidget::SizeViewToRootObject);
    root->setWidth(width);
    root->setHeight(root->implicitHeight());
    QObject::connect(root, &QQuickItem::implicitHeightChanged, root, [root] {
        root->setHeight(root->implicitHeight());
    });
    QPointer<QWidget> target(resize);
    auto fit = [view, root, width, target] {
        view->setFixedSize(width, qMax(1, qCeil(root->height())));
        if (target) {
            target->adjustSize();
        }
    };
    QObject::connect(root, &QQuickItem::heightChanged, view, fit, Qt::QueuedConnection);
    fit();
}

}
