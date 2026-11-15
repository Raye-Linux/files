/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QAbstractItemModel>
#include <QIcon>
#include <QList>
#include <QPointer>
#include <QUrl>

#include <memory>

class KCoreDirLister;
class KFilePlacesModel;

namespace Raye::NavPane
{

// One drive of "This PC", as the navigation pane needs it. FX-15's drive
// model fills these with Raye's names ("Raye", "Windows", "USB Drive"); until
// then PlacesDriveSource reads KDE's places list.
struct Drive {
    QString id; // stable while the drive is plugged in
    QString name;
    QString iconName;
    QUrl url; // where it opens; empty while not mounted
    bool removable = false;
    qreal usage = -1; // 0..1, or -1 when unknown

    bool operator==(const Drive &other) const = default;
};

class DOLPHIN_EXPORT DriveSource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    virtual QList<Drive> drives() const = 0;

Q_SIGNALS:
    void drivesChanged();
};

// A fixed list, for tests and the render harness.
class DOLPHIN_EXPORT StaticDriveSource : public DriveSource
{
    Q_OBJECT

public:
    explicit StaticDriveSource(const QList<Drive> &drives = {}, QObject *parent = nullptr);
    QList<Drive> drives() const override;
    void setDrives(const QList<Drive> &drives);

private:
    QList<Drive> m_drives;
};

// The navigation pane of Files (explorer spec 3.5): Home, Gallery, the pins
// of Quick access, This PC with its drives, Network, and the Recycle Bin
// when "Show all folders" is on. Pins are the bookmarks of the shared KDE
// places list, so every KDE dialog sees the same ones. Folders under a pin
// or a drive are listed when their row is expanded.
class DOLPHIN_EXPORT NavigationModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Kind {
        Fixed, // Home, Gallery, This PC, Network, Recycle Bin
        Pin,
        Drive,
        Folder, // a sub-folder of a pin or a drive
        Separator,
    };
    Q_ENUM(Kind)

    enum Roles {
        UrlRole = Qt::UserRole + 1,
        KindRole,
        IconNameRole,
        RemovableRole,
        UsageRole, // drives: 0..1, or -1
        DriveIdRole,
        DepthRole,
    };

    // Neither is owned. places may be null (no pins), drives too (no drives).
    explicit NavigationModel(KFilePlacesModel *places, DriveSource *drives, QObject *parent = nullptr);
    ~NavigationModel() override;

    // Windows' "Show all folders": the Recycle Bin joins the top level.
    void setShowAllFolders(bool show);
    bool showAllFolders() const;

    // The URLs of the fixed rows.
    static QUrl homeUrl();
    static QUrl galleryUrl();
    static QUrl thisPcUrl();
    static QUrl networkUrl();
    static QUrl recycleBinUrl();

    // Pin and unpin a folder in Quick access (the shared places list).
    bool isPinned(const QUrl &url) const;
    bool pin(const QUrl &url, const QString &name = QString());
    bool unpin(const QUrl &url);

    // The deepest row already in the model that holds url (the folder itself,
    // or the pin or drive it is under). Invalid when none does.
    QModelIndex indexForUrl(const QUrl &url) const;

    // The rows from the top down to url, loading folders on the way; the
    // index of the last one reached. For "Expand to open folder".
    QModelIndex expandTowards(const QUrl &url);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;
    QHash<int, QByteArray> roleNames() const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;

    // True while some expanded row is still being listed.
    bool isLoading() const;

Q_SIGNALS:
    // Every expanded folder finished listing.
    void loadingFinished();

public:
    // A row of the tree; public only so the implementation's helpers see it.
    struct Node;

private:
    void rebuild();
    void reloadPins();
    void reloadDrives();
    void startListing(Node *node);
    void finishListing(Node *node);
    Node *nodeFor(const QModelIndex &index) const;
    QModelIndex indexFor(Node *node) const;
    Node *topNode(const QUrl &url) const;
    void replaceChildren(Node *parent, std::vector<std::unique_ptr<Node>> children);
    // Stops listening to the listers below node before it goes away.
    void dropListers(Node *node, bool notify = true);

    QPointer<KFilePlacesModel> m_places;
    QPointer<DriveSource> m_drives;
    std::unique_ptr<Node> m_root;
    bool m_showAllFolders = false;
    int m_listing = 0;
};

}
