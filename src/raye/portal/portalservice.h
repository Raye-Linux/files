/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QDBusAbstractAdaptor>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QVariantMap>

class QDialog;

namespace Raye::Portal
{

// The two portal interfaces this backend serves. Nothing here decides what
// the dialogs look like: it reads the options, shows the dialog parented on the
// app's window, and answers on D-Bus when it closes.
//
// The names and argument order are xdg-desktop-portal's:
// org.freedesktop.impl.portal.{FileChooser,AppChooser}.

constexpr uint responseSuccess = 0;
constexpr uint responseCancelled = 1;
constexpr uint responseOther = 2;

class DOLPHIN_EXPORT FileChooserAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
    Q_PROPERTY(uint version READ version CONSTANT)
public:
    explicit FileChooserAdaptor(QObject *parent);
    uint version() const
    {
        return 3;
    }

public Q_SLOTS:
    void OpenFile(const QDBusObjectPath &handle,
                  const QString &app_id,
                  const QString &parent_window,
                  const QString &title,
                  const QVariantMap &options,
                  const QDBusMessage &message,
                  uint &response,
                  QVariantMap &results);
    void SaveFile(const QDBusObjectPath &handle,
                  const QString &app_id,
                  const QString &parent_window,
                  const QString &title,
                  const QVariantMap &options,
                  const QDBusMessage &message,
                  uint &response,
                  QVariantMap &results);
    void SaveFiles(const QDBusObjectPath &handle,
                   const QString &app_id,
                   const QString &parent_window,
                   const QString &title,
                   const QVariantMap &options,
                   const QDBusMessage &message,
                   uint &response,
                   QVariantMap &results);
};

class DOLPHIN_EXPORT AppChooserAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.AppChooser")
    Q_PROPERTY(uint version READ version CONSTANT)
public:
    explicit AppChooserAdaptor(QObject *parent);
    uint version() const
    {
        return 2;
    }

public Q_SLOTS:
    void ChooseApplication(const QDBusObjectPath &handle,
                           const QString &app_id,
                           const QString &parent_window,
                           const QStringList &choices,
                           const QVariantMap &options,
                           const QDBusMessage &message,
                           uint &response,
                           QVariantMap &results);
    // New apps turned up while the dialog is open. The dialog lists every app
    // of the system from the start, so there is nothing to add.
    void UpdateChoices(const QDBusObjectPath &handle, const QStringList &choices);
};

// Adds both interfaces to an object on the bus. dialogsChanged-style idling is
// up to the caller: it can ask openDialogs().
DOLPHIN_EXPORT void addPortalInterfaces(QObject *object);
DOLPHIN_EXPORT int openDialogs();

}
