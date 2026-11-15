/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "shareflyout.h"
#include "sharemodel.h"

#include "ui/tokens.h"

#include <KLocalizedContext>
#include <KLocalizedString>

#include <QAbstractItemModel>
#include <QLabel>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>

namespace Raye::Share
{
namespace
{
QStringList &importPaths()
{
    static QStringList paths;
    return paths;
}
}

void ShareFlyout::addImportPath(const QString &path)
{
    if (!importPaths().contains(path)) {
        importPaths().append(path);
    }
}

ShareFlyout::ShareFlyout(QWidget *parent)
    : Ui::Flyout(parent)
    , m_model(new ShareModel(this))
    , m_view(new QQuickWidget)
{
    Ui::registerQmlTypes();
    m_view->setResizeMode(QQuickWidget::SizeViewToRootObject);
    m_view->setAttribute(Qt::WA_AlwaysStackOnTop);
    m_view->setAttribute(Qt::WA_TranslucentBackground);
    m_view->setClearColor(Qt::transparent);
    for (const QString &path : std::as_const(importPaths())) {
        m_view->engine()->addImportPath(path);
    }
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    auto *context = new KLocalizedContext(m_view->engine());
    QT_WARNING_POP
    context->setTranslationDomain(QStringLiteral("raye-files"));
    m_view->engine()->rootContext()->setContextObject(context);
    setContentWidget(m_view);
    connect(m_model, &ShareModel::done, this, &Ui::Flyout::dismiss);
}

ShareFlyout::~ShareFlyout()
{
    // The page goes first: its bindings read the model to the end.
    delete m_view.data();
}

void ShareFlyout::setUrls(const QList<QUrl> &urls, const QStringList &mimeTypes)
{
    m_model->setUrls(urls, mimeTypes);
    const QJsonObject input = ShareModel::inputData(m_model->urls(), mimeTypes.size() == urls.size() ? mimeTypes : QStringList());
    if (!m_view) {
        return;
    }
    if (!m_purpose) {
        QString error;
        m_purpose = ShareModel::createPurposeModel(m_view->engine(), input, this, &error);
        if (!m_purpose) {
            qWarning("Share: no Purpose targets: %s", qPrintable(error));
        }
        m_model->setSource(m_purpose);
    } else {
        m_purpose->setProperty("inputData", input);
    }
    if (m_view && !m_view->rootObject()) {
        load();
    }
}

void ShareFlyout::load()
{
    m_view->setInitialProperties({
        {QStringLiteral("shareModel"), QVariant::fromValue<QObject *>(m_model)},
        {QStringLiteral("purposeModel"), QVariant::fromValue<QObject *>(m_purpose.data())},
        {QStringLiteral("width"), sheetWidth - 2 * padding()},
    });
    m_view->loadFromModule("org.raye.files", "ShareSheet");
    if (QQuickItem *root = m_view->rootObject()) {
        connect(root, SIGNAL(closeRequested()), this, SLOT(sheetClosed()));
    } else {
        const QList<QQmlError> errors = m_view->errors();
        for (const QQmlError &e : errors) {
            qWarning("Share: %s", qPrintable(e.toString()));
        }
        // Said plainly rather than an empty box.
        auto *label = new QLabel(i18nc("@info", "Sharing isn't available right now."));
        label->setFont(Ui::textFont(Ui::TextStyle::Body));
        label->setContentsMargins(12, 12, 12, 12);
        label->setFixedWidth(sheetWidth - 2 * padding());
        label->setWordWrap(true);
        m_view = nullptr;
        setContentWidget(label);
    }
    adjustSize();
}

void ShareFlyout::sheetClosed()
{
    dismiss();
}

bool ShareFlyout::isReady() const
{
    return m_view && m_view->rootObject() != nullptr;
}

ShareFlyout *ShareFlyout::share(QWidget *anchor, const QList<QUrl> &urls, const QStringList &mimeTypes)
{
    auto *flyout = new ShareFlyout(anchor);
    flyout->setUrls(urls, mimeTypes);
    connect(flyout, &Ui::Flyout::closed, flyout, &QObject::deleteLater);
    if (anchor) {
        flyout->showAt(anchor, Qt::BottomEdge, Qt::AlignLeft);
    }
    return flyout;
}

}
