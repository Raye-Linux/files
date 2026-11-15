/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

class QAbstractItemModel;
class QQmlEngine;

namespace Raye::Share
{

// What the share flyout shows (explorer spec 3.11): the files, the targets
// of the "Share with" row, and the rest for "More options". The targets come
// from Purpose's list of Export plugins (org.kde.purpose,
// PurposeAlternativesModel), plus Files' own Copy, and Copy link for files
// that are not on this computer.
class DOLPHIN_EXPORT ShareModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList rowTargets READ rowTargets NOTIFY changed)
    Q_PROPERTY(QVariantList moreTargets READ moreTargets NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY changed)
    Q_PROPERTY(QString iconName READ iconName NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)

public:
    // Purpose's roles (purpose/alternativesmodel.h).
    static constexpr int pluginIdRole = Qt::UserRole + 1;
    static constexpr int iconNameRole = Qt::UserRole + 2;

    explicit ShareModel(QObject *parent = nullptr);
    ~ShareModel() override;

    // Empty MIME types: looked up from the names.
    void setUrls(const QList<QUrl> &urls, const QStringList &mimeTypes = {});
    QList<QUrl> urls() const
    {
        return m_urls;
    }

    // Purpose's model of the targets (or anything with the same roles).
    void setSource(QAbstractItemModel *source);
    QAbstractItemModel *source() const
    {
        return m_source;
    }

    // Each entry: {id, name, icon, glyph, row} where row is Purpose's row, -1 for
    // Files' own targets.
    QVariantList rowTargets() const
    {
        return m_row;
    }
    QVariantList moreTargets() const
    {
        return m_more;
    }
    QStringList rowIds() const;
    QStringList moreIds() const;

    QString title() const;
    QString subtitle() const;
    QString iconName() const;
    int count() const
    {
        return int(m_urls.size());
    }

    // Runs one of Files' own targets; false for an id that is not one.
    Q_INVOKABLE bool activateOwn(const QString &id);

    // What Purpose's Export plugins are given.
    static QJsonObject inputData(const QList<QUrl> &urls, const QStringList &mimeTypes);
    // A PurposeAlternativesModel for those files, made in engine; nullptr
    // with the reason when org.kde.purpose is not installed.
    static QAbstractItemModel *createPurposeModel(QQmlEngine *engine, const QJsonObject &input, QObject *parent, QString *error = nullptr);

Q_SIGNALS:
    void changed();
    // A target did its work: the flyout can close.
    void done();

private:
    void rebuild();

    QList<QUrl> m_urls;
    QStringList m_mimeTypes;
    QPointer<QAbstractItemModel> m_source;
    QVariantList m_row;
    QVariantList m_more;
};

}
