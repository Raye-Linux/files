/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>

class KFilePlacesModel;

// Where Home (explorer spec 3.9) gets its lists. The Start menu's
// "Recommended" reads the same activity statistics, so both agree; the
// tests and the render harness use the in-memory sources.
namespace Raye::Pages
{

struct DOLPHIN_EXPORT HomeResource {
    QUrl url;
    QString title; // empty: the file name
    QString mimeType; // empty: looked up from the name
    QDateTime used; // last opened
    bool operator==(const HomeResource &other) const
    {
        return url == other.url;
    }
};

// Recent files, frequent folders and Favorites.
class DOLPHIN_EXPORT HomeStatsSource : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    // Newest first.
    virtual QList<HomeResource> recentFiles(int limit) const = 0;
    // Most used first.
    virtual QList<HomeResource> frequentFolders(int limit) const = 0;
    virtual QList<HomeResource> favorites() const = 0;
    virtual bool isFavorite(const QUrl &url) const;
    virtual void addFavorite(const QUrl &url) = 0;
    virtual void removeFavorite(const QUrl &url) = 0;
    // "Clear File Explorer history": recent and frequent go, Favorites stay.
    virtual void forgetHistory() = 0;

Q_SIGNALS:
    void changed();
};

// Fixed lists, changed by hand.
class DOLPHIN_EXPORT StaticStatsSource : public HomeStatsSource
{
    Q_OBJECT
public:
    using HomeStatsSource::HomeStatsSource;
    QList<HomeResource> recent;
    QList<HomeResource> folders;
    QList<HomeResource> favs;
    int forgetCalls = 0;

    QList<HomeResource> recentFiles(int limit) const override;
    QList<HomeResource> frequentFolders(int limit) const override;
    QList<HomeResource> favorites() const override;
    void addFavorite(const QUrl &url) override;
    void removeFavorite(const QUrl &url) override;
    void forgetHistory() override;
    // Tell the views the lists were changed by hand.
    void notify()
    {
        Q_EMIT changed();
    }
};

// Without activity statistics: KDE's recent documents list, folders ranked
// by how many recent files they hold, Favorites in Files' state file.
class DOLPHIN_EXPORT RecentDocumentsSource : public HomeStatsSource
{
    Q_OBJECT
public:
    using HomeStatsSource::HomeStatsSource;
    QList<HomeResource> recentFiles(int limit) const override;
    QList<HomeResource> frequentFolders(int limit) const override;
    QList<HomeResource> favorites() const override;
    void addFavorite(const QUrl &url) override;
    void removeFavorite(const QUrl &url) override;
    void forgetHistory() override;
};

// The activities agent Favorites are linked under; the Start menu reads it.
DOLPHIN_EXPORT QString favoritesAgent();

// The source Files uses: activity statistics when Files is built with
// them, else RecentDocumentsSource.
DOLPHIN_EXPORT HomeStatsSource *createStatsSource(QObject *parent = nullptr);
DOLPHIN_EXPORT bool hasActivityStats();

// Quick access pins.
class DOLPHIN_EXPORT HomePins : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    // In the order of the places list.
    virtual QList<HomeResource> pins() const = 0;
    virtual bool isPinned(const QUrl &url) const;
    virtual bool pin(const QUrl &url) = 0;
    virtual bool unpin(const QUrl &url) = 0;
Q_SIGNALS:
    void changed();
};

class DOLPHIN_EXPORT StaticPins : public HomePins
{
    Q_OBJECT
public:
    using HomePins::HomePins;
    QList<HomeResource> list;
    QList<HomeResource> pins() const override
    {
        return list;
    }
    bool pin(const QUrl &url) override;
    bool unpin(const QUrl &url) override;
};

// The pins are the folders of the shared KDE places list (the navigation
// pane and every KDE dialog see the same ones): local folders that are not
// hidden, without Home, the Recycle Bin and drives.
class DOLPHIN_EXPORT PlacesPins : public HomePins
{
    Q_OBJECT
public:
    // Not owned; a list of its own when null.
    explicit PlacesPins(KFilePlacesModel *places = nullptr, QObject *parent = nullptr);
    QList<HomeResource> pins() const override;
    bool pin(const QUrl &url) override;
    bool unpin(const QUrl &url) override;

private:
    QPointer<KFilePlacesModel> m_places;
};

}
