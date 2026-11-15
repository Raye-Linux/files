/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <KService>

#include <QAbstractListModel>

namespace Raye::OpenWith
{

// The apps of "How do you want to open this file?" (explorer spec 3.15):
// the ones made for the file's type first ("Recommended", the current
// default at the top), then every other app that opens files ("More
// apps"), each list in its own order. Hidden apps never show.
class DOLPHIN_EXPORT AppChoicesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString mimeType READ mimeType WRITE setMimeType NOTIFY mimeTypeChanged)
    Q_PROPERTY(int recommendedCount READ recommendedCount NOTIFY mimeTypeChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        IconRole,
        StorageIdRole,
        RecommendedRole,
        DefaultRole,
        CommentRole,
    };

    explicit AppChoicesModel(QObject *parent = nullptr);

    QString mimeType() const
    {
        return m_mime;
    }
    void setMimeType(const QString &mime);
    int recommendedCount() const
    {
        return m_recommended;
    }
    // The app picked; the default (or the first) to start with.
    int currentIndex() const
    {
        return m_current;
    }
    void setCurrentIndex(int index);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    KService::Ptr service(int row) const;
    KService::Ptr currentService() const;
    // Moves these apps, given by desktop id, to the top of the recommended ones
    // and picks the first. Ids of apps that do not exist are left out.
    void prefer(const QStringList &desktopIds);
    // Puts an app the user browsed to at the top of the list and picks it.
    void addBrowsed(const KService::Ptr &service);

    // Apps for the type, the default first; and the rest that open files.
    static KService::List recommendedApps(const QString &mime);
    static KService::List otherApps(const KService::List &except);

Q_SIGNALS:
    void mimeTypeChanged();
    void currentIndexChanged();

private:
    QString m_mime;
    KService::List m_services;
    int m_recommended = 0;
    int m_current = -1;
    QString m_defaultId;
};

// "Always": the app becomes the default for the type in mimeapps.list.
DOLPHIN_EXPORT bool setAlways(const QString &mime, const KService::Ptr &service);

// An app the user browsed to: a desktop entry of the user's own, so it can
// be remembered ("Always") and shows next time. Null when the file is not
// a program the user may run.
DOLPHIN_EXPORT KService::Ptr serviceForProgram(const QString &path, const QString &mime);

}
