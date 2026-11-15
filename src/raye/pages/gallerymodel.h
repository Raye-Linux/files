/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

namespace Raye::Pages
{
// Gallery (explorer spec 1.9): every picture and video of the gallery
// folders, newest first, under a header per month ("November 2026"). The
// folders come from dolphinrc [Raye] GalleryFolders, the account's Pictures
// when empty. The page shows them a batch at a time as it scrolls.
class DOLPHIN_EXPORT GalleryModel : public QObject
{
    Q_OBJECT
    // [{title, items: [{url, name, aspect, video, duration}]}], one group
    // per month, for the items shown so far.
    Q_PROPERTY(QVariantList groups READ groups NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int shown READ shown NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    // [{path, name, included}]: what Collection offers to tick.
    Q_PROPERTY(QVariantList collection READ collection NOTIFY collectionChanged)

public:
    static constexpr int batchSize = 120;

    struct Picture {
        QUrl url;
        QDateTime date;
        qreal aspect = 1.5; // width / height
        bool video = false;
        int duration = -1; // seconds, videos only
    };

    struct Row {
        int start = 0;
        int count = 0;
        qreal height = 0;
    };

    explicit GalleryModel(const QStringList &folders = {}, QObject *parent = nullptr);

    QVariantList groups() const
    {
        return m_groups;
    }
    int count() const
    {
        return m_count;
    }
    bool loading() const
    {
        return m_loading;
    }
    int shown() const
    {
        return m_shown;
    }
    bool hasMore() const
    {
        return m_shown < m_items.size();
    }
    QVariantList collection() const;

    // The pictures and videos under these folders, newest first.
    static QList<Picture> scan(const QStringList &folders);
    // Grouped by month, in the given order.
    static QVariantList group(const QList<Picture> &pictures);
    static QStringList configuredFolders();
    // Rows as wide as the page: each row is scaled so its pictures fill
    // the width; the last row keeps the target height.
    static QList<Row> justify(const QList<qreal> &aspects, qreal width, qreal targetHeight, qreal gap);
    // "3:42", "1:02:05"; empty for no duration.
    static QString durationText(int seconds);

    // [{start, count, height}] for the QML page.
    Q_INVOKABLE QVariantList rows(const QVariantList &aspects, qreal width, qreal targetHeight, qreal gap) const;
    Q_INVOKABLE void open(const QUrl &url);
    Q_INVOKABLE void openLocation(const QUrl &url);
    Q_INVOKABLE void properties(const QUrl &url);
    Q_INVOKABLE void copyAsPath(const QUrl &url);
    Q_INVOKABLE void setAsBackground(const QUrl &url);
    // "Add a folder…" of Collection.
    Q_INVOKABLE void addFolder();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void setFolderIncluded(const QString &path, bool included);

Q_SIGNALS:
    void changed();
    void collectionChanged();
    void openRequested(const QUrl &url);
    void openLocationRequested(const QUrl &folder, const QUrl &item);
    void propertiesRequested(const QUrl &url);
    void addFolderRequested();

private:
    void regroup();

    bool m_fixedFolders = false;
    QStringList m_folders;
    QList<Picture> m_items;
    QVariantList m_groups;
    int m_count = 0;
    int m_shown = 0;
    bool m_loading = false;
};
}
