/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "propertiesdialog.h"

#include "jobs/quickhost.h"
#include "openwith/openwithdialog.h"
#include "properties.h"
#include "netdrive/networkdrives.h"
#include "ui/carddialog.h"
#include "ui/commandbutton.h"
#include "ui/showcase.h"

#include <KLocalizedString>
#include <KPropertiesDialog>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QMimeDatabase>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWidget>
#include <QStandardPaths>
#include <QTimer>

namespace Raye::Properties
{
namespace
{
constexpr int bodyWidth = 452;

Raye::Ui::CardDialog *makeDialog(PropertiesModel *model, QWidget *parent)
{
    using Raye::Ui::CardDialog;
    auto *dialog = new CardDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFixedWidth(bodyWidth + 48);
    dialog->setTitle(i18nc("@title:window, %1 is a file name", "%1 Properties", model->name()));
    model->setParent(dialog);

    QQuickWidget *view = Raye::Jobs::quickPage(QStringLiteral("PropertiesPage"), {{QStringLiteral("model"), QVariant::fromValue<QObject *>(model)}}, nullptr, "bgCardSolid");
    dialog->setContentWidget(view);
    Raye::Jobs::fitToPage(view, bodyWidth, dialog);

    dialog->setButtonText(CardDialog::Primary, i18nc("@action:button", "OK"));
    dialog->setButtonText(CardDialog::Secondary, i18nc("@action:button", "Apply"));
    dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
    dialog->setClosesOnButton(false);
    // Nothing of a network place can be changed here.
    if (model->network()) {
        if (auto *b = dialog->button(CardDialog::Secondary)) {
            b->hide();
        }
    }

    QPointer<CardDialog> guard(dialog);
    auto updateApply = [guard, model] {
        if (guard) {
            if (auto *b = guard->button(CardDialog::Secondary)) {
                b->setEnabled(model->dirty());
            }
        }
    };
    QObject::connect(model, &PropertiesModel::changed, dialog, updateApply);
    updateApply();

    QObject::connect(dialog, &CardDialog::buttonClicked, dialog, [guard, model](CardDialog::ButtonRole role) {
        if (!guard) {
            return;
        }
        if (role == CardDialog::Close) {
            guard->reject();
        } else if (model->apply() && role == CardDialog::Primary) {
            guard->accept();
        }
    });
    QObject::connect(model, &PropertiesModel::advancedRequested, dialog, [guard, model] {
        QList<QUrl> urls;
        for (const QString &p : model->paths()) {
            urls << QUrl::fromLocalFile(p);
        }
        KPropertiesDialog::showDialog(urls, guard ? guard->parentWidget() : nullptr);
        if (guard) {
            guard->reject();
        }
    });
    QObject::connect(model, &PropertiesModel::changeAppRequested, dialog, [guard, model] {
        if (!guard || model->paths().isEmpty()) {
            return;
        }
        const QUrl url = QUrl::fromLocalFile(model->paths().first());
        const QString mime = QMimeDatabase().mimeTypeForFile(url.toLocalFile()).name();
        Raye::Ui::CardDialog *chooser = Raye::OpenWith::openWithDialog(guard, {url}, mime, [model](const KService::Ptr &service, bool always) {
            if (service && always) {
                model->setOpenWith(service->name(), service->icon());
            }
        });
        // Here the question is only which app opens files of this type.
        if (auto *b = chooser->button(Raye::Ui::CardDialog::Secondary)) {
            b->hide();
        }
        chooser->setDefaultButton(Raye::Ui::CardDialog::Primary);
        chooser->show();
    });
    if (QQuickItem *root = view->rootObject()) {
        QMetaObject::invokeMethod(root, "focusField", Qt::QueuedConnection);
    }
    return dialog;
}
}

void showProperties(const QList<QUrl> &urls, QWidget *parent)
{
    if (!PropertiesModel::handles(urls)) {
        KPropertiesDialog::showDialog(urls, parent);
        return;
    }
    Raye::Ui::CardDialog *dialog = makeDialog(new PropertiesModel(urls), parent);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

}

namespace
{
QString sampleDir()
{
    const QString dir = QDir::tempPath() + QStringLiteral("/raye-properties-sample");
    QDir().mkpath(dir + QStringLiteral("/Holiday 2026"));
    QFile f(dir + QStringLiteral("/Report Q3.pdf"));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QByteArray(2516582, 'x'));
    }
    QImage image(1600, 1200, QImage::Format_RGB32);
    image.fill(QColor(40, 40, 48));
    image.save(dir + QStringLiteral("/Holiday 2026/Beach.png"));
    return dir;
}

QWidget *sample(QWidget *parent, const QString &path, int tab, const QString &drive = QString())
{
    using namespace Raye::Properties;
    auto *model = new PropertiesModel({QUrl::fromLocalFile(path)}, nullptr, drive, drive.isEmpty() ? PropertiesModel::SpaceReader() : [](const QString &) {
        constexpr qint64 GiB = qint64(1024) * 1024 * 1024;
        return PropertiesModel::Space{244 * GiB, 77 * GiB, QStringLiteral("btrfs")};
    });
    Raye::Ui::CardDialog *dialog = makeDialog(model, parent);
    dialog->setTranslucent(true);
    if (tab > 0) {
        if (QQuickWidget *view = qobject_cast<QQuickWidget *>(dialog->contentWidget())) {
            if (QQuickItem *root = view->rootObject()) {
                QMetaObject::invokeMethod(root, "showTab", Q_ARG(QVariant, tab));
            }
        }
    }
    // The harness shoots floating dialogs over its window.
    QTimer::singleShot(0, dialog, [dialog] {
        dialog->show();
    });
    auto *host = new QWidget(parent);
    host->setAutoFillBackground(true);
    return host;
}
}

RAYE_SHOWCASE(propertiesFile, "PropertiesFile", "Properties of a PDF file, General")
{
    return sample(parent, sampleDir() + QStringLiteral("/Report Q3.pdf"), 0);
}

RAYE_SHOWCASE(propertiesSecurity, "PropertiesSecurity", "Properties of a PDF file, Security")
{
    return sample(parent, sampleDir() + QStringLiteral("/Report Q3.pdf"), 1);
}

RAYE_SHOWCASE(propertiesPicture, "PropertiesPicture", "Properties of a picture, Details")
{
    return sample(parent, sampleDir() + QStringLiteral("/Holiday 2026/Beach.png"), 2);
}

RAYE_SHOWCASE(propertiesFolder, "PropertiesFolder", "Properties of a folder")
{
    return sample(parent, sampleDir(), 0);
}

RAYE_SHOWCASE(propertiesDrive, "PropertiesDrive", "Properties of the Raye drive")
{
    return sample(parent, QStringLiteral("/"), 0, QStringLiteral("Raye"));
}

RAYE_SHOWCASE(propertiesNetwork, "PropertiesNetwork", "Properties of a network drive")
{
    const QString folder = sampleDir() + QStringLiteral("/data");
    QDir().mkpath(folder);
    Raye::NetDrives::setMountReader([folder] {
        return QList<Raye::NetDrives::Mount>{{QStringLiteral("//192.168.50.222/data"), folder, QStringLiteral("cifs")}};
    });
    using namespace Raye::Properties;
    auto *model = new PropertiesModel({QUrl::fromLocalFile(folder)}, nullptr, QString(), [](const QString &) {
        constexpr qint64 GiB = qint64(1024) * 1024 * 1024;
        return PropertiesModel::Space{2794 * GiB, 650 * GiB, QStringLiteral("cifs")};
    });
    Raye::NetDrives::setMountReader({});
    Raye::Ui::CardDialog *dialog = makeDialog(model, parent);
    dialog->setTranslucent(true);
    QTimer::singleShot(0, dialog, [dialog] {
        dialog->show();
    });
    auto *host = new QWidget(parent);
    host->setAutoFillBackground(true);
    return host;
}

RAYE_SHOWCASE(propertiesShare, "PropertiesShare", "Properties of a shared folder seen on the network")
{
    using namespace Raye::Properties;
    auto *model = new PropertiesModel({QUrl(QStringLiteral("smb://nas/photos"))});
    Raye::Ui::CardDialog *dialog = makeDialog(model, parent);
    dialog->setTranslucent(true);
    QTimer::singleShot(0, dialog, [dialog] {
        dialog->show();
    });
    auto *host = new QWidget(parent);
    host->setAutoFillBackground(true);
    return host;
}
