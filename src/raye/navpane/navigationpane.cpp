/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "navigationpane.h"

#include "netdrive/mapdrivedialog.h"
#include "netdrive/networkdrives.h"
#include "pages/thispcmodel.h"

#include "navpane/droprules.h"
#include "ui/tokens.h"
#include "ui/wheelstep.h"

#include <KConfigGroup>
#include <KFilePlacesModel>
#include <KIconEngine>
#include <KIconLoader>
#include <KIO/CopyJob>
#include <KIO/DeleteOrTrashJob>
#include <KIO/FileUndoManager>
#include <KIO/JobUiDelegateFactory>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>
#include <Solid/Device>

#include <QApplication>
#include <QContextMenuEvent>
#include <QDragMoveEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QStorageInfo>
#include <QStyledItemDelegate>

namespace Raye::NavPane
{

namespace
{
constexpr int rowNormal = 32;
constexpr int rowCompact = 24;
constexpr int separatorHeight = 9;
constexpr int indentStep = 16;
constexpr int sideMargin = 4; // the row's fill stops this far from the pane edges
constexpr int contentLeft = 8;
constexpr int chevronBox = 16;
constexpr int pillWidth = 3;
constexpr int pillHeight = 16;

// Row heights: separators are thin, everything else follows compact view.
class RowDelegate : public QStyledItemDelegate
{
public:
    explicit RowDelegate(NavigationPane *pane)
        : QStyledItemDelegate(pane)
        , m_pane(pane)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const override
    {
        const bool separator = index.data(NavigationModel::KindRole).toInt() == NavigationModel::Separator;
        return QSize(0, separator ? separatorHeight : m_pane->rowHeight());
    }

private:
    NavigationPane *m_pane;
};

int depthOf(const QModelIndex &index)
{
    return index.data(NavigationModel::DepthRole).toInt();
}

QList<QUrl> urlsOf(const QMimeData *mime)
{
    return mime && mime->hasUrls() ? mime->urls() : QList<QUrl>();
}
}

PlacesDriveSource::PlacesDriveSource(KFilePlacesModel *places, QObject *parent)
    : DriveSource(parent)
    , m_places(places)
{
    if (m_places) {
        auto changed = [this] {
            Q_EMIT drivesChanged();
        };
        connect(m_places, &QAbstractItemModel::rowsInserted, this, changed);
        connect(m_places, &QAbstractItemModel::rowsRemoved, this, changed);
        connect(m_places, &QAbstractItemModel::dataChanged, this, changed);
        connect(m_places, &QAbstractItemModel::modelReset, this, changed);
    }
    connect(Raye::NetDrives::Notifier::instance(), &Raye::NetDrives::Notifier::changed, this, [this] {
        Q_EMIT drivesChanged();
    });
}

QList<Drive> PlacesDriveSource::drives() const
{
    QList<Drive> list;
    if (!m_places) {
        return list;
    }
    for (int row = 0; row < m_places->rowCount(); ++row) {
        const QModelIndex i = m_places->index(row, 0);
        if (!m_places->isDevice(i) || m_places->isHidden(i)) {
            continue;
        }
        const KFilePlacesModel::GroupType group = m_places->groupType(i);
        if (group != KFilePlacesModel::DevicesType && group != KFilePlacesModel::RemovableDevicesType) {
            continue;
        }
        Drive d;
        d.id = m_places->deviceForIndex(i).udi();
        d.name = m_places->text(i);
        d.iconName = m_places->icon(i).name();
        d.url = m_places->url(i);
        d.removable = group == KFilePlacesModel::RemovableDevicesType;
        if (d.url.isLocalFile()) {
            const QStorageInfo info(d.url.toLocalFile());
            if (info.isValid() && info.bytesTotal() > 0) {
                d.usage = 1.0 - qreal(info.bytesAvailable()) / qreal(info.bytesTotal());
            }
        }
        list.append(d);
    }
    // Network drives, as File Explorer lists them under This PC.
    for (const Raye::NetDrives::Mount &m : Raye::NetDrives::networkMounts()) {
        const QUrl url = QUrl::fromLocalFile(m.mountPoint);
        if (std::any_of(list.cbegin(), list.cend(), [&url](const Drive &d) {
                return d.url == url;
            })) {
            continue;
        }
        const Raye::NetDrives::Mapping mapping = Raye::NetDrives::mappingAt(m.mountPoint);
        const Raye::NetDrives::Address a = Raye::NetDrives::addressOfSource(m.source);
        Drive d;
        d.id = QStringLiteral("network:") + m.mountPoint;
        d.name = !mapping.name.isEmpty() ? mapping.name : a.isValid() ? Raye::Pages::ThisPcModel::shareName(a.share, a.server) : QFileInfo(m.mountPoint).fileName();
        d.iconName = QStringLiteral("folder-network");
        d.url = url;
        const QStorageInfo info(m.mountPoint);
        if (info.isValid() && info.bytesTotal() > 0) {
            d.usage = 1.0 - qreal(info.bytesAvailable()) / qreal(info.bytesTotal());
        }
        list.append(d);
    }
    return list;
}

NavigationPane::NavigationPane(QWidget *parent)
    : QTreeView(parent)
{
    m_ownPlaces = new KFilePlacesModel(this);
    m_ownDrives = new PlacesDriveSource(m_ownPlaces, this);
    m_model = new NavigationModel(m_ownPlaces, m_ownDrives, this);
    init();
}

NavigationPane::NavigationPane(KFilePlacesModel *places, DriveSource *drives, QWidget *parent)
    : QTreeView(parent)
{
    m_model = new NavigationModel(places, drives, this);
    init();
}

NavigationPane::~NavigationPane() = default;

void NavigationPane::init()
{
    setObjectName(QStringLiteral("RayeNavigationPane"));
    setModel(m_model);
    setItemDelegate(new RowDelegate(this));
    setHeaderHidden(true);
    setIndentation(indentStep);
    setRootIsDecorated(true);
    setUniformRowHeights(false);
    setAnimated(true);
    setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DropOnly);
    setDropIndicatorShown(false);
    setAutoExpandDelay(700);
    setMinimumWidth(160);
    setMaximumWidth(480);
    setFont(Raye::Ui::textFont(Raye::Ui::TextStyle::Body));
    viewport()->setAutoFillBackground(false);
    setAttribute(Qt::WA_Hover);
    setAccessibleName(i18nc("@info:accessible", "Navigation pane"));

    // This PC open from the start, like Windows.
    expand(m_model->indexForUrl(NavigationModel::thisPcUrl()));
    connect(m_model, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent) {
        if (parent.isValid() && parent.data(NavigationModel::UrlRole).toUrl() == NavigationModel::thisPcUrl()) {
            expand(parent);
        }
    });
    connect(m_model, &NavigationModel::loadingFinished, this, &NavigationPane::followCurrent);

    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("dolphinrc"));
    m_watcher = KConfigWatcher::create(config);
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == QLatin1String("Raye")) {
            readSettings();
        }
    });
    readSettings();
}

void NavigationPane::readSettings()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("dolphinrc"));
    config->reparseConfiguration();
    const KConfigGroup group = config->group(QStringLiteral("Raye"));
    setCompact(group.readEntry("Compact", false));
    setShowAllFolders(group.readEntry("ShowAllFolders", false));
    setExpandToOpenFolder(group.readEntry("ExpandToOpenFolder", false));
}

NavigationModel *NavigationPane::navigationModel() const
{
    return m_model;
}

void NavigationPane::setCompact(bool compact)
{
    if (compact == m_compact) {
        return;
    }
    m_compact = compact;
    // Row heights are asked again from the delegate.
    scheduleDelayedItemsLayout();
    viewport()->update();
}

bool NavigationPane::isCompact() const
{
    return m_compact;
}

void NavigationPane::setShowAllFolders(bool show)
{
    m_model->setShowAllFolders(show);
}

void NavigationPane::setExpandToOpenFolder(bool expand)
{
    m_expandToOpenFolder = expand;
    if (expand) {
        followCurrent();
    }
}

bool NavigationPane::expandToOpenFolder() const
{
    return m_expandToOpenFolder;
}

int NavigationPane::rowHeight() const
{
    return m_compact ? rowCompact : rowNormal;
}

QSize NavigationPane::sizeHint() const
{
    return QSize(240, 480);
}

void NavigationPane::setCurrentUrl(const QUrl &url)
{
    m_currentUrl = url;
    followCurrent();
}

QUrl NavigationPane::currentUrl() const
{
    return m_currentUrl;
}

void NavigationPane::followCurrent()
{
    if (m_currentUrl.isEmpty()) {
        return;
    }
    QModelIndex index = m_expandToOpenFolder ? m_model->expandTowards(m_currentUrl) : m_model->indexForUrl(m_currentUrl);
    if (m_expandToOpenFolder) {
        for (QModelIndex p = index; p.isValid(); p = p.parent()) {
            if (p != index || p.data(NavigationModel::UrlRole).toUrl().adjusted(QUrl::StripTrailingSlash)
                    != m_currentUrl.adjusted(QUrl::StripTrailingSlash)) {
                expand(p);
            }
        }
    } else if (index.isValid() && index.data(NavigationModel::KindRole).toInt() == NavigationModel::Folder) {
        // Without "Expand to open folder" Windows selects the pin or drive
        // the folder is in, unless its row is already on screen.
        QModelIndex top = index;
        while (top.parent().isValid() && !isExpanded(top.parent())) {
            top = top.parent();
        }
        index = top;
    }
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    if (index.isValid()) {
        scrollTo(index);
    }
}

QRect NavigationPane::chevronRect(const QModelIndex &index) const
{
    const QRect row = visualRect(index);
    if (!row.isValid()) {
        return QRect();
    }
    const int x = contentLeft + depthOf(index) * indentStep;
    return QRect(x, row.top() + (row.height() - chevronBox) / 2, chevronBox, chevronBox);
}

void NavigationPane::drawBranches(QPainter *, const QRect &, const QModelIndex &) const
{
    // Chevrons are drawn by drawRow, in Raye's place and size.
}

void NavigationPane::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const Raye::Ui::TokenSet &t = Raye::Ui::Tokens::current();
    const QRect row(0, option.rect.top(), viewport()->width(), option.rect.height());
    const int kind = index.data(NavigationModel::KindRole).toInt();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    if (kind == NavigationModel::Separator) {
        const int y = row.center().y();
        painter->setPen(QPen(t.color(Raye::Ui::TokenSet::strokeDivider), 1));
        painter->drawLine(QPointF(12, y + 0.5), QPointF(row.width() - 12, y + 0.5));
        painter->restore();
        return;
    }

    const bool selected = selectionModel() && selectionModel()->isSelected(index);
    const bool hovered = index == m_hover;
    const bool dropTarget = index == m_dropTarget;
    const QRectF fill = QRectF(row).adjusted(sideMargin, 1, -sideMargin, -1);
    if (selected || hovered || dropTarget) {
        QColor color = t.color(Raye::Ui::TokenSet::fillSubtleHover);
        if (selected) {
            color = t.color(Raye::Ui::TokenSet::fillSubtleSelected);
        }
        if (dropTarget) {
            color = Raye::Ui::withAlpha(t.color(Raye::Ui::TokenSet::accent), 0.3);
        }
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawRoundedRect(fill, Raye::Ui::Metrics::radiusControl, Raye::Ui::Metrics::radiusControl);
    }
    if (selected) {
        const QRectF pill(fill.left(), fill.center().y() - pillHeight / 2.0, pillWidth, pillHeight);
        painter->setBrush(t.color(Raye::Ui::TokenSet::accent));
        painter->drawRoundedRect(pill, pillWidth / 2.0, pillWidth / 2.0);
    }

    // Chevron: only while the pointer is over the pane, or on an open row
    // when the keyboard is in the pane (so the tree stays readable).
    const QRect chevron = chevronRect(index);
    if (m_model->hasChildren(index) && (m_paneHovered || hasFocus())) {
        const qreal s = Raye::Ui::Metrics::chevronSize / 2.0 - 2;
        const QPointF c = QRectF(chevron).center();
        QPainterPath path;
        if (isExpanded(index)) {
            path.moveTo(c.x() - s, c.y() - s / 2);
            path.lineTo(c.x(), c.y() + s / 2);
            path.lineTo(c.x() + s, c.y() - s / 2);
        } else {
            path.moveTo(c.x() - s / 2, c.y() - s);
            path.lineTo(c.x() + s / 2, c.y());
            path.lineTo(c.x() - s / 2, c.y() + s);
        }
        painter->setPen(QPen(t.color(Raye::Ui::TokenSet::textSecondary), 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    }

    const int iconSize = Raye::Ui::Metrics::iconSize;
    const QRect iconRect(chevron.right() + 1 + 4, row.top() + (row.height() - iconSize) / 2, iconSize, iconSize);
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    if (!icon.isNull()) {
        icon.paint(painter, iconRect);
    }

    // Pins show their glyph on hover, at the right, like Quick access.
    int textRight = row.right() - 12;
    if (kind == NavigationModel::Pin && hovered) {
        const QRect pinRect(row.right() - 12 - iconSize, iconRect.top(), iconSize, iconSize);
        QIcon(new KIconEngine(QStringLiteral("pin"), KIconLoader::global())).paint(painter, pinRect);
        textRight = pinRect.left() - 4;
    }

    const QString text = index.data(Qt::DisplayRole).toString();
    const QRect textRect(iconRect.right() + 1 + Raye::Ui::Metrics::iconTextGap, row.top(), textRight - iconRect.right() - Raye::Ui::Metrics::iconTextGap, row.height());
    painter->setFont(font());
    painter->setPen(t.color(isEnabled() ? Raye::Ui::TokenSet::textPrimary : Raye::Ui::TokenSet::textDisabled));
    const QString elided = painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width());
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elided);

    // Drives: a 2 px usage line under the name while hovered.
    const qreal usage = index.data(NavigationModel::UsageRole).toReal();
    if (kind == NavigationModel::Drive && hovered && usage >= 0) {
        const QRectF track(textRect.left(), row.bottom() - 5, qMin(120, textRect.width()), 2);
        painter->setPen(Qt::NoPen);
        painter->setBrush(t.color(Raye::Ui::TokenSet::strokeDivider));
        painter->drawRoundedRect(track, 1, 1);
        QRectF used = track;
        used.setWidth(track.width() * qBound(0.0, usage, 1.0));
        const bool full = usage > Raye::Ui::Metrics::usageCautionAbove;
        painter->setBrush(full ? t.color(Raye::Ui::TokenSet::statusCaution) : t.color(Raye::Ui::TokenSet::accent));
        painter->drawRoundedRect(used, 1, 1);
    }
    painter->restore();
}

void NavigationPane::activate(const QModelIndex &index)
{
    const QUrl url = index.data(NavigationModel::UrlRole).toUrl();
    if (!url.isEmpty()) {
        m_currentUrl = url;
        Q_EMIT urlActivated(url);
        return;
    }
    // A USB stick or disk that is plugged in but not mounted has no address yet.
    const QString driveId = index.data(NavigationModel::DriveIdRole).toString();
    if (!driveId.isEmpty()) {
        Q_EMIT mountRequested(driveId);
    }
}

void NavigationPane::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    if (index.isValid() && event->button() == Qt::LeftButton && m_model->hasChildren(index)
        && chevronRect(index).adjusted(-4, -8, 4, 8).contains(event->position().toPoint())) {
        setExpanded(index, !isExpanded(index));
        event->accept();
        return;
    }
    if (index.isValid() && !(index.flags() & Qt::ItemIsSelectable)) {
        event->accept();
        return;
    }
    QTreeView::mousePressEvent(event);
    if (index.isValid() && event->button() == Qt::LeftButton) {
        activate(index);
    } else if (index.isValid() && event->button() == Qt::MiddleButton) {
        const QUrl url = index.data(NavigationModel::UrlRole).toUrl();
        if (!url.isEmpty()) {
            Q_EMIT openInNewTabRequested(url);
        }
    }
}

void NavigationPane::updateGeometries()
{
    QTreeView::updateGeometries();
    // QTreeView sets its own wheel step on every layout.
    Raye::Ui::setWheelStep(this);
}

void NavigationPane::keyPressEvent(QKeyEvent *event)
{
    QTreeView::keyPressEvent(event);
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
        activate(currentIndex());
    }
}

void NavigationPane::mouseMoveEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    if (index != m_hover) {
        m_hover = index;
        viewport()->update();
    }
    QTreeView::mouseMoveEvent(event);
}

void NavigationPane::enterEvent(QEnterEvent *event)
{
    m_paneHovered = true;
    viewport()->update();
    QTreeView::enterEvent(event);
}

void NavigationPane::leaveEvent(QEvent *event)
{
    m_paneHovered = false;
    m_hover = QModelIndex();
    viewport()->update();
    QTreeView::leaveEvent(event);
}

void NavigationPane::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    const QUrl url = index.data(NavigationModel::UrlRole).toUrl();
    if (!index.isValid() || url.isEmpty()) {
        return;
    }
    const int kind = index.data(NavigationModel::KindRole).toInt();
    QMenu menu(this);
    menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "Open in new tab"), this, [this, url] {
        Q_EMIT openInNewTabRequested(url);
    });
    menu.addAction(QIcon::fromTheme(QStringLiteral("window-new")), i18nc("@action:inmenu", "Open in new window"), this, [this, url] {
        Q_EMIT openInNewWindowRequested(url);
    });
    if (kind == NavigationModel::Pin || kind == NavigationModel::Folder || kind == NavigationModel::Drive) {
        menu.addSeparator();
        if (m_model->isPinned(url)) {
            menu.addAction(QIcon::fromTheme(QStringLiteral("window-unpin")), i18nc("@action:inmenu", "Unpin from Quick access"), this, [this, url] {
                m_model->unpin(url);
            });
        } else if (url.isLocalFile()) {
            menu.addAction(QIcon::fromTheme(QStringLiteral("pin")), i18nc("@action:inmenu", "Pin to Quick access"), this, [this, url, index] {
                m_model->pin(url, index.data(Qt::DisplayRole).toString());
            });
        }
    }
    if (kind == NavigationModel::Drive && index.data(NavigationModel::RemovableRole).toBool()) {
        const QString id = index.data(NavigationModel::DriveIdRole).toString();
        menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")), i18nc("@action:inmenu", "Eject"), this, [this, id, url] {
            Q_EMIT ejectRequested(id, url);
        });
    }
    if (kind == NavigationModel::Drive && index.data(NavigationModel::DriveIdRole).toString().startsWith(QLatin1String("network:"))) {
        menu.addAction(QIcon::fromTheme(QStringLiteral("network-disconnect")), i18nc("@action:inmenu remove a network drive", "Disconnect"), this, [this, url] {
            QPointer<NavigationPane> self(this);
            Raye::NetDrives::disconnectDrive(url.toLocalFile(), this, [self](Raye::NetDrives::Failure failure, const QString &text) {
                if (self && failure != Raye::NetDrives::Failure::None) {
                    KMessageBox::error(self->window(), text);
                }
            });
        });
    }
    if (url == NavigationModel::networkUrl()) {
        menu.addAction(QIcon::fromTheme(QStringLiteral("folder-network")), i18nc("@action:inmenu", "Map network drive…"), this, [this] {
            QPointer<NavigationPane> self(this);
            Raye::NetDrives::showMapDriveDialog(QUrl(), window(), [self](const QUrl &drive) {
                if (self) {
                    Q_EMIT self->urlActivated(drive);
                }
            });
        });
    }
    if (url.scheme() == QLatin1String("trash")) {
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme(QStringLiteral("trash-empty")), i18nc("@action:inmenu", "Empty Recycle Bin"), this, [this] {
            using Ask = KIO::AskUserActionInterface;
            auto *job = new KIO::DeleteOrTrashJob(QList<QUrl>{}, Ask::EmptyTrash, Ask::DefaultConfirmation, window());
            job->start();
        });
    }
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), i18nc("@action:inmenu", "Properties"), this, [this, url] {
        Q_EMIT propertiesRequested(url);
    });
    menu.exec(event->globalPos());
}

Qt::DropAction NavigationPane::actionFor(const QModelIndex &target, const QList<QUrl> &urls, Qt::KeyboardModifiers modifiers) const
{
    if (!target.isValid() || !(target.flags() & Qt::ItemIsDropEnabled) || urls.isEmpty()) {
        return Qt::IgnoreAction;
    }
    const QUrl destination = target.data(NavigationModel::UrlRole).toUrl();
    const bool toBin = destination.scheme() == QLatin1String("trash");
    const Qt::DropAction action = dropAction(modifiers, sameDrive(urls, destination), toBin);
    if (isPointlessDrop(urls, destination, action)) {
        return Qt::IgnoreAction;
    }
    return action;
}

void NavigationPane::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void NavigationPane::dragMoveEvent(QDragMoveEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    const Qt::DropAction action = actionFor(index, urlsOf(event->mimeData()), event->modifiers());
    const QPersistentModelIndex target = action == Qt::IgnoreAction ? QModelIndex() : index;
    if (target != m_dropTarget) {
        m_dropTarget = target;
        viewport()->update();
    }
    // Hovering a closed row opens it after a moment, like Windows.
    QTreeView::dragMoveEvent(event);
    if (action == Qt::IgnoreAction) {
        event->ignore();
        return;
    }
    event->setDropAction(action);
    event->accept();
}

void NavigationPane::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_dropTarget = QModelIndex();
    viewport()->update();
    QTreeView::dragLeaveEvent(event);
}

void NavigationPane::dropEvent(QDropEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    const QList<QUrl> urls = urlsOf(event->mimeData());
    const Qt::DropAction action = actionFor(index, urls, event->modifiers());
    m_dropTarget = QModelIndex();
    viewport()->update();
    if (action == Qt::IgnoreAction) {
        event->ignore();
        return;
    }
    const QUrl destination = index.data(NavigationModel::UrlRole).toUrl();
    KIO::Job *job = nullptr;
    KIO::FileUndoManager::CommandType command = KIO::FileUndoManager::Copy;
    if (destination.scheme() == QLatin1String("trash")) {
        job = KIO::trash(urls);
        command = KIO::FileUndoManager::Trash;
    } else if (action == Qt::MoveAction) {
        job = KIO::move(urls, destination);
        command = KIO::FileUndoManager::Move;
    } else if (action == Qt::LinkAction) {
        job = KIO::link(urls, destination);
        command = KIO::FileUndoManager::Link;
    } else {
        job = KIO::copy(urls, destination);
    }
    if (auto *copyJob = qobject_cast<KIO::CopyJob *>(job)) {
        KIO::FileUndoManager::self()->recordCopyJob(copyJob);
    } else {
        KIO::FileUndoManager::self()->recordJob(command, urls, destination, job);
    }
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
    event->setDropAction(action);
    event->accept();
    Q_EMIT dropped(urls, destination, action);
}

}

// The render harness: a pane on a places list of its own (the six folders
// of Quick access) and three sample drives, Documents selected and the
// pointer over the Windows drive.
#include "ui/showcase.h"

#include <QHBoxLayout>
#include <QTemporaryDir>
#include <QTimer>

RAYE_SHOWCASE(navigationPane, "NavigationPane", "Navigation pane with pins and drives")
{
    static QTemporaryDir dir;
    const QString home = dir.path();
    for (const char *name : {"Documents/Projects", "Documents/Taxes"}) {
        QDir().mkpath(home + QLatin1Char('/') + QLatin1String(name));
    }
    // The places list and the folders of this showcase only, never the user's.
    qputenv("XDG_DATA_HOME", (home + QStringLiteral("/.data")).toUtf8());
    auto *places = new KFilePlacesModel(parent);
    for (int row = 0; row < places->rowCount(); ++row) {
        places->setPlaceHidden(places->index(row, 0), true);
    }
    const std::pair<const char *, const char *> pins[] = {
        {"Desktop", "folder-desktop"},
        {"Downloads", "folder-download"},
        {"Documents", "folder-documents"},
        {"Pictures", "folder-pictures"},
        {"Music", "folder-music"},
        {"Videos", "folder-videos"},
    };
    for (const auto &[name, icon] : pins) {
        const QString path = home + QLatin1Char('/') + QLatin1String(name);
        QDir().mkpath(path);
        places->addPlace(QLatin1String(name), QUrl::fromLocalFile(path), QLatin1String(icon));
    }

    Raye::NavPane::Drive raye;
    raye.id = QStringLiteral("raye");
    raye.name = i18nc("@item the system drive", "Raye");
    raye.iconName = QStringLiteral("drive-harddisk");
    raye.url = QUrl::fromLocalFile(home);
    raye.usage = 0.42;
    Raye::NavPane::Drive windows = raye;
    windows.id = QStringLiteral("windows");
    windows.name = QStringLiteral("Windows");
    windows.usage = 0.71;
    Raye::NavPane::Drive usb = raye;
    usb.id = QStringLiteral("usb");
    usb.name = i18nc("@item a removable drive with no name", "USB Drive");
    usb.iconName = QStringLiteral("drive-removable-media");
    usb.removable = true;
    usb.usage = 0.93;
    auto *drives = new Raye::NavPane::StaticDriveSource({raye, windows, usb}, parent);

    auto *frame = new QWidget(parent);
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(0);
    auto *pane = new Raye::NavPane::NavigationPane(places, drives, frame);
    pane->setFixedWidth(240);
    layout->addWidget(pane);
    layout->addStretch(1);
    places->setParent(pane);
    drives->setParent(pane);

    QTimer::singleShot(100, pane, [pane, home] {
        pane->setCurrentUrl(QUrl::fromLocalFile(home + QStringLiteral("/Documents")));
        Raye::NavPane::NavigationModel *model = pane->navigationModel();
        pane->expand(pane->currentIndex());
        const QModelIndex pc = model->indexForUrl(Raye::NavPane::NavigationModel::thisPcUrl());
        const QModelIndex hover = model->index(1, 0, pc);
        const QPoint at = pane->visualRect(hover).center();
        QEnterEvent enter(at, at, pane->viewport()->mapToGlobal(at));
        QCoreApplication::sendEvent(pane, &enter);
        QMouseEvent move(QEvent::MouseMove, at, pane->viewport()->mapToGlobal(at), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(pane->viewport(), &move);
    });
    return frame;
}
