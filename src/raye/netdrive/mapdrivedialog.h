/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"
#include "networkdrives.h"

#include <QObject>
#include <QUrl>

#include <functional>

class QWidget;

namespace Raye::NetDrives
{

// What the Map network drive window asks and does: the shared folder, a
// name, the folder it goes in, Reconnect at sign-in and other credentials.
class DOLPHIN_EXPORT MapDriveModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY changed)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(QString location READ location WRITE setLocation NOTIFY changed)
    Q_PROPERTY(bool reconnect MEMBER m_reconnect NOTIFY changed)
    Q_PROPERTY(bool otherCredentials READ otherCredentials WRITE setOtherCredentials NOTIFY changed)
    Q_PROPERTY(QString user MEMBER m_user NOTIFY changed)
    Q_PROPERTY(QString password MEMBER m_password NOTIFY changed)
    Q_PROPERTY(bool remember MEMBER m_remember NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canFinish READ canFinish NOTIFY changed)

public:
    using Connector = std::function<void(const Mapping &, const QString &password, QObject *context, const std::function<void(Failure, const QString &)> &)>;

    // home: where drives go by default. connector: connectDrive unless set.
    explicit MapDriveModel(const QString &home = QString(), const Connector &connector = {}, QObject *parent = nullptr);

    QString folder() const { return m_folder; }
    void setFolder(const QString &folder);
    QString name() const { return m_name; }
    void setName(const QString &name);
    QString location() const { return m_location; }
    void setLocation(const QString &location);
    bool otherCredentials() const { return m_otherCredentials; }
    void setOtherCredentials(bool other);
    QString error() const { return m_error; }
    bool busy() const { return m_busy; }
    bool canFinish() const;

    // The mapping Finish would make (user "DOMAIN\name" split in two).
    Mapping mapping() const;

    Q_INVOKABLE void chooseLocation();
    Q_INVOKABLE void finish();

Q_SIGNALS:
    void changed();
    void finished(const QString &mountPoint);
    void chooseLocationRequested();

private:
    QString m_home;
    Connector m_connector;
    QString m_folder;
    QString m_name;
    QString m_location;
    bool m_nameEdited = false;
    bool m_locationEdited = false;
    bool m_reconnect = true;
    bool m_otherCredentials = false;
    QString m_user;
    QString m_password;
    bool m_remember = true;
    QString m_error;
    bool m_busy = false;
};

// The window. share prefills the folder (smb://server/share, or empty).
// opened: called with the new drive's folder once it is connected.
DOLPHIN_EXPORT void showMapDriveDialog(const QUrl &share, QWidget *parent, const std::function<void(const QUrl &)> &opened = {});
// The same window for a drive already in the list (its password is not in
// the wallet): same name and folder, the password to type again.
DOLPHIN_EXPORT void showMapDriveDialog(const Mapping &mapping, QWidget *parent, const std::function<void(const QUrl &)> &opened = {});

}
