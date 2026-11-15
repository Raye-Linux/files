/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mapdrivedialog.h"

#include "jobs/quickhost.h"
#include "ui/carddialog.h"
#include "ui/commandbutton.h"
#include "ui/showcase.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTimer>

namespace Raye::NetDrives
{

MapDriveModel::MapDriveModel(const QString &home, const Connector &connector, QObject *parent)
    : QObject(parent)
    , m_home(home.isEmpty() ? QDir::homePath() : home)
    , m_connector(connector ? connector : Connector(&connectDrive))
{
}

void MapDriveModel::setFolder(const QString &folder)
{
    if (folder == m_folder) {
        return;
    }
    m_folder = folder;
    const Address a = parseAddress(folder);
    if (!m_nameEdited && a.isValid()) {
        m_name = a.path.isEmpty() ? a.share : a.path.section(QLatin1Char('/'), -1);
    }
    if (!m_locationEdited && isValidName(m_name)) {
        m_location = defaultMountPoint(m_home, m_name);
    }
    const QString user = userOf(folder);
    if (!user.isEmpty()) {
        m_user = user;
        m_otherCredentials = true;
    }
    m_error.clear();
    Q_EMIT changed();
}

void MapDriveModel::setName(const QString &name)
{
    if (name == m_name) {
        return;
    }
    m_name = name;
    m_nameEdited = true;
    if (!m_locationEdited && isValidName(name)) {
        m_location = defaultMountPoint(m_home, name);
    }
    Q_EMIT changed();
}

void MapDriveModel::setLocation(const QString &location)
{
    if (location == m_location) {
        return;
    }
    m_location = location;
    m_locationEdited = true;
    Q_EMIT changed();
}

void MapDriveModel::setOtherCredentials(bool other)
{
    if (other == m_otherCredentials) {
        return;
    }
    m_otherCredentials = other;
    Q_EMIT changed();
}

bool MapDriveModel::canFinish() const
{
    return !m_busy && parseAddress(m_folder).isValid() && isValidName(m_name) && !m_location.trimmed().isEmpty()
        && (!m_otherCredentials || !m_user.trimmed().isEmpty());
}

Mapping MapDriveModel::mapping() const
{
    Mapping m;
    m.address = parseAddress(m_folder);
    m.name = m_name.trimmed();
    m.mountPoint = QDir::cleanPath(m_location.trimmed());
    m.reconnect = m_reconnect;
    if (m_otherCredentials) {
        QString user = m_user.trimmed();
        const int slash = user.indexOf(QLatin1Char('\\'));
        if (slash > 0) {
            m.domain = user.left(slash);
            user = user.mid(slash + 1);
        }
        m.user = user;
        m.remember = m_remember;
    }
    return m;
}

void MapDriveModel::chooseLocation()
{
    Q_EMIT chooseLocationRequested();
}

void MapDriveModel::finish()
{
    if (!canFinish()) {
        if (!parseAddress(m_folder).isValid()) {
            m_error = i18nc("@info", "Type the folder as \\\\server\\share.");
            Q_EMIT changed();
        }
        return;
    }
    const Mapping m = mapping();
    const QString home = QDir::cleanPath(m_home);
    if (!m.mountPoint.startsWith(home + QLatin1Char('/'))) {
        m_error = i18nc("@info", "Pick a folder inside your home folder.");
        Q_EMIT changed();
        return;
    }
    const QFileInfo place(m.mountPoint);
    if (place.exists() && (!place.isDir() || !QDir(m.mountPoint).isEmpty())) {
        m_error = i18nc("@info", "%1 already has files in it. Pick an empty folder.", m.mountPoint);
        Q_EMIT changed();
        return;
    }
    m_busy = true;
    m_error.clear();
    Q_EMIT changed();
    QPointer<MapDriveModel> self(this);
    const bool asGuest = !m_otherCredentials;
    m_connector(m, m_otherCredentials ? m_password : QString(), this, [self, m, asGuest](Failure failure, const QString &text) {
        if (!self) {
            return;
        }
        self->m_busy = false;
        if (failure == Failure::None) {
            self->m_password.clear();
            Q_EMIT self->changed();
            Q_EMIT self->finished(m.mountPoint);
            return;
        }
        // A share that wants a name and password: ask for them.
        self->m_error = failure == Failure::Credentials && asGuest
            ? i18nc("@info", "%1 needs a user name and password.", uncText(m.address))
            : text;
        if (failure == Failure::Credentials) {
            self->m_otherCredentials = true;
        }
        Q_EMIT self->changed();
    });
}

namespace
{
constexpr int bodyWidth = 480;

Raye::Ui::CardDialog *makeDialog(MapDriveModel *model, QWidget *parent)
{
    using Raye::Ui::CardDialog;
    auto *dialog = new CardDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFixedWidth(bodyWidth + 48);
    dialog->setTitle(i18nc("@title:window", "Map network drive"));
    model->setParent(dialog);

    QQuickWidget *view = Raye::Jobs::quickPage(QStringLiteral("MapDrivePage"), {{QStringLiteral("model"), QVariant::fromValue<QObject *>(model)}}, nullptr, "bgCardSolid");
    dialog->setContentWidget(view);
    Raye::Jobs::fitToPage(view, bodyWidth, dialog);

    dialog->setButtonText(CardDialog::Primary, i18nc("@action:button connect the network drive", "Finish"));
    dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
    if (auto *b = dialog->button(CardDialog::Secondary)) {
        b->hide();
    }
    dialog->setDefaultButton(CardDialog::Primary);
    dialog->setClosesOnButton(false);

    QPointer<CardDialog> guard(dialog);
    auto update = [guard, model] {
        if (guard) {
            if (auto *b = guard->button(CardDialog::Primary)) {
                b->setEnabled(model->canFinish());
                b->setText(model->busy() ? i18nc("@action:button busy connecting", "Connecting…") : i18nc("@action:button connect the network drive", "Finish"));
            }
        }
    };
    QObject::connect(model, &MapDriveModel::changed, dialog, update);
    update();
    QObject::connect(dialog, &CardDialog::buttonClicked, dialog, [guard, model](CardDialog::ButtonRole role) {
        if (!guard) {
            return;
        }
        if (role == CardDialog::Primary) {
            model->finish();
        } else {
            guard->reject();
        }
    });
    QObject::connect(model, &MapDriveModel::chooseLocationRequested, dialog, [guard, model] {
        const QString start = QFileInfo(model->location()).absolutePath();
        const QString dir = QFileDialog::getExistingDirectory(guard, i18nc("@title:window", "Choose where the drive goes"), start);
        if (!dir.isEmpty() && guard) {
            model->setLocation(QDir::cleanPath(dir + QLatin1Char('/') + model->name().trimmed()));
        }
    });
    if (QQuickItem *root = view->rootObject()) {
        QMetaObject::invokeMethod(root, "focusField", Qt::QueuedConnection);
    }
    return dialog;
}
}

void showMapDriveDialog(const QUrl &share, QWidget *parent, const std::function<void(const QUrl &)> &opened)
{
    auto *model = new MapDriveModel();
    if (share.isValid() && share.scheme() == QLatin1String("smb") && !share.host().isEmpty()) {
        Address a;
        a.server = share.host();
        const QStringList parts = share.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        a.share = parts.value(0);
        a.path = parts.mid(1).join(QLatin1Char('/'));
        model->setFolder(a.share.isEmpty() ? QStringLiteral("\\\\") + a.server + QLatin1Char('\\') : uncText(a));
        if (!share.userName().isEmpty()) {
            model->setOtherCredentials(true);
            model->setProperty("user", share.userName());
        }
    }
    Raye::Ui::CardDialog *dialog = makeDialog(model, parent);
    QPointer<Raye::Ui::CardDialog> guard(dialog);
    QObject::connect(model, &MapDriveModel::finished, dialog, [guard, opened](const QString &mountPoint) {
        if (opened) {
            opened(QUrl::fromLocalFile(mountPoint));
        }
        if (guard) {
            guard->accept();
        }
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void showMapDriveDialog(const Mapping &mapping, QWidget *parent, const std::function<void(const QUrl &)> &opened)
{
    auto *model = new MapDriveModel();
    model->setFolder(uncText(mapping.address));
    model->setName(mapping.name);
    model->setLocation(mapping.mountPoint);
    model->setProperty("reconnect", mapping.reconnect);
    if (!mapping.user.isEmpty()) {
        model->setOtherCredentials(true);
        model->setProperty("user", mapping.domain.isEmpty() ? mapping.user : mapping.domain + QLatin1Char('\\') + mapping.user);
    }
    Raye::Ui::CardDialog *dialog = makeDialog(model, parent);
    QPointer<Raye::Ui::CardDialog> guard(dialog);
    QObject::connect(model, &MapDriveModel::finished, dialog, [guard, opened](const QString &mountPoint) {
        if (opened) {
            opened(QUrl::fromLocalFile(mountPoint));
        }
        if (guard) {
            guard->accept();
        }
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

}

RAYE_SHOWCASE(mapDrive, "MapDrive", "Map network drive, with other credentials")
{
    using namespace Raye::NetDrives;
    auto *model = new MapDriveModel(QStringLiteral("/home/alex"), [](const Mapping &, const QString &, QObject *, const auto &done) {
        done(Failure::Credentials, failureText(Failure::Credentials, parseAddress(QStringLiteral("\\\\nas\\photos"))));
    });
    model->setFolder(QStringLiteral("\\\\nas\\photos"));
    model->setOtherCredentials(true);
    model->setProperty("user", QStringLiteral("alex"));
    model->setProperty("password", QStringLiteral("secret"));
    model->finish();
    Raye::Ui::CardDialog *dialog = makeDialog(model, parent);
    dialog->setTranslucent(true);
    QTimer::singleShot(0, dialog, [dialog] {
        dialog->show();
    });
    auto *host = new QWidget(parent);
    host->setAutoFillBackground(true);
    return host;
}

RAYE_SHOWCASE(mapDriveEmpty, "MapDriveEmpty", "Map network drive, fresh")
{
    using namespace Raye::NetDrives;
    auto *model = new MapDriveModel(QStringLiteral("/home/alex"));
    Raye::Ui::CardDialog *dialog = makeDialog(model, parent);
    dialog->setTranslucent(true);
    QTimer::singleShot(0, dialog, [dialog] {
        dialog->show();
    });
    auto *host = new QWidget(parent);
    host->setAutoFillBackground(true);
    return host;
}
