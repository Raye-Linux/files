/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "navpane/navigationmodel.h"

#include <KConfigWatcher>

#include <QPersistentModelIndex>
#include <QTreeView>

class KFilePlacesModel;

namespace Raye::NavPane
{

// The drives of KDE's places list (Solid underneath), shown until FX-15's
// drive model with Raye's names takes over. Hidden places stay hidden, so
// the system partitions FX-23 hides never show.
class DOLPHIN_EXPORT PlacesDriveSource : public DriveSource
{
    Q_OBJECT

public:
    explicit PlacesDriveSource(KFilePlacesModel *places, QObject *parent = nullptr);
    QList<Drive> drives() const override;

private:
    KFilePlacesModel *m_places;
};

// The navigation pane on the left of Files (explorer spec 3.5). Rows 32
// high (24 in compact view), 16 px icons, 16 px indent, chevrons while the
// pointer is over the pane, the selected row in a rounded fill with the
// accent pill. Clicking a row opens it; files dropped on a row are moved or
// copied with the Windows modifier rules.
class DOLPHIN_EXPORT NavigationPane : public QTreeView
{
    Q_OBJECT

public:
    explicit NavigationPane(QWidget *parent = nullptr);
    // A pane on given sources, for tests and the render harness. The pane
    // owns neither.
    NavigationPane(KFilePlacesModel *places, DriveSource *drives, QWidget *parent = nullptr);
    ~NavigationPane() override;

    NavigationModel *navigationModel() const;

    // The folder shown in the view: its row is selected; with "Expand to
    // open folder" on, the rows down to it are opened.
    void setCurrentUrl(const QUrl &url);
    QUrl currentUrl() const;

    void setCompact(bool compact);
    bool isCompact() const;
    void setShowAllFolders(bool show);
    void setExpandToOpenFolder(bool expand);
    bool expandToOpenFolder() const;

    int rowHeight() const;
    QSize sizeHint() const override;

    // The rectangle of a row's chevron, in viewport coordinates.
    QRect chevronRect(const QModelIndex &index) const;

Q_SIGNALS:
    void urlActivated(const QUrl &url);
    void openInNewTabRequested(const QUrl &url);
    void openInNewWindowRequested(const QUrl &url);
    void propertiesRequested(const QUrl &url);
    void ejectRequested(const QString &driveId, const QUrl &url);
    // Emitted after the copy, move or link job is started.
    void dropped(const QList<QUrl> &sources, const QUrl &destination, Qt::DropAction action);

protected:
    void drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void init();
    void readSettings();
    void followCurrent();
    Qt::DropAction actionFor(const QModelIndex &target, const QList<QUrl> &urls, Qt::KeyboardModifiers modifiers) const;
    void activate(const QModelIndex &index);

    KFilePlacesModel *m_ownPlaces = nullptr;
    DriveSource *m_ownDrives = nullptr;
    NavigationModel *m_model = nullptr;
    KConfigWatcher::Ptr m_watcher;
    QUrl m_currentUrl;
    QPersistentModelIndex m_hover;
    QPersistentModelIndex m_dropTarget;
    bool m_paneHovered = false;
    bool m_compact = false;
    bool m_expandToOpenFolder = false;
};

}
