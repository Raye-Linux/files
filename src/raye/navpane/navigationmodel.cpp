/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "navigationmodel.h"

#include <KCoreDirLister>
#include <KFileItem>
#include <KFilePlacesModel>
#include <KIconEngine>
#include <KIconLoader>
#include <KLocalizedString>

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QLocale>
#include <QMimeData>

#include <algorithm>
#include <functional>

namespace Raye::NavPane
{

struct NavigationModel::Node {
    Kind kind = Fixed;
    QUrl url;
    QString text;
    QString iconName;
    QString driveId;
    bool removable = false;
    qreal usage = -1;
    Node *parent = nullptr;
    std::vector<std::unique_ptr<Node>> children;
    // Folders below are listed on demand: until then the row says it may
    // have some, so the view draws the chevron.
    bool expandable = false;
    bool fetched = false;
    std::unique_ptr<KCoreDirLister> lister;

    int row() const
    {
        if (!parent) {
            return 0;
        }
        for (size_t i = 0; i < parent->children.size(); ++i) {
            if (parent->children[i].get() == this) {
                return int(i);
            }
        }
        return 0;
    }
};

namespace
{
using Node = NavigationModel::Node;

// A local folder with no visible sub-folder gets no chevron right away
// (Windows shows none either); one read of the folder, stopped at the first
// sub-folder. Other places are only known once listed.
bool mayHaveSubfolders(const QUrl &url)
{
    if (!url.isLocalFile())
        return true;
    // unknown (missing, unreadable): keep the chevron, the listing decides
    const QDir dir(url.toLocalFile());
    if (!dir.exists() || !dir.isReadable())
        return true;
    QDirIterator it(dir.path(), QDir::Dirs | QDir::NoDotAndDotDot);
    return it.hasNext();
}

std::unique_ptr<Node> makeNode(NavigationModel::Kind kind, const QUrl &url, const QString &text, const QString &icon)
{
    auto node = std::make_unique<Node>();
    node->kind = kind;
    node->url = url;
    node->text = text;
    node->iconName = icon;
    node->expandable = (kind == NavigationModel::Pin || kind == NavigationModel::Drive || kind == NavigationModel::Folder)
        && mayHaveSubfolders(url);
    return node;
}

QUrl cleaned(const QUrl &url)
{
    return url.adjusted(QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
}

// url is the folder itself or sits below it.
bool contains(const QUrl &folder, const QUrl &url)
{
    if (folder.isEmpty()) {
        return false;
    }
    const QUrl f = cleaned(folder);
    const QUrl u = cleaned(url);
    return f == u || f.isParentOf(u);
}
}

StaticDriveSource::StaticDriveSource(const QList<Drive> &drives, QObject *parent)
    : DriveSource(parent)
    , m_drives(drives)
{
}

QList<Drive> StaticDriveSource::drives() const
{
    return m_drives;
}

void StaticDriveSource::setDrives(const QList<Drive> &drives)
{
    if (drives == m_drives) {
        return;
    }
    m_drives = drives;
    Q_EMIT drivesChanged();
}

NavigationModel::NavigationModel(KFilePlacesModel *places, DriveSource *drives, QObject *parent)
    : QAbstractItemModel(parent)
    , m_places(places)
    , m_drives(drives)
    , m_root(std::make_unique<Node>())
{
    rebuild();
    if (m_places) {
        auto reload = [this] {
            reloadPins();
        };
        connect(m_places, &QAbstractItemModel::rowsInserted, this, reload);
        connect(m_places, &QAbstractItemModel::rowsRemoved, this, reload);
        connect(m_places, &QAbstractItemModel::rowsMoved, this, reload);
        connect(m_places, &QAbstractItemModel::dataChanged, this, reload);
        connect(m_places, &QAbstractItemModel::modelReset, this, reload);
        connect(m_places, &QAbstractItemModel::layoutChanged, this, reload);
    }
    if (m_drives) {
        connect(m_drives, &DriveSource::drivesChanged, this, &NavigationModel::reloadDrives);
    }
}

NavigationModel::~NavigationModel()
{
    dropListers(m_root.get(), false);
}

void NavigationModel::dropListers(Node *node, bool notify)
{
    const int before = m_listing;
    std::function<void(Node *)> drop = [&](Node *n) {
        if (n->lister) {
            // A lister being destroyed reports "canceled": nobody must hear it.
            QObject::disconnect(n->lister.get(), nullptr, this, nullptr);
            if (!n->fetched) {
                --m_listing;
            }
            n->lister->stop();
        }
        for (const auto &c : n->children) {
            drop(c.get());
        }
    };
    drop(node);
    if (notify && before > 0 && m_listing == 0) {
        Q_EMIT loadingFinished();
    }
}

QUrl NavigationModel::homeUrl()
{
    return QUrl(QStringLiteral("raye:home"));
}

QUrl NavigationModel::galleryUrl()
{
    return QUrl(QStringLiteral("raye:gallery"));
}

QUrl NavigationModel::thisPcUrl()
{
    return QUrl(QStringLiteral("raye:thispc"));
}

QUrl NavigationModel::networkUrl()
{
    return QUrl(QStringLiteral("smb:/"));
}

QUrl NavigationModel::recycleBinUrl()
{
    return QUrl(QStringLiteral("trash:/"));
}

void NavigationModel::rebuild()
{
    beginResetModel();
    dropListers(m_root.get(), false);
    m_root->children.clear();
    auto add = [this](std::unique_ptr<Node> node) {
        node->parent = m_root.get();
        m_root->children.push_back(std::move(node));
    };
    add(makeNode(Fixed, homeUrl(), i18nc("@item navigation pane", "Home"), QStringLiteral("user-home")));
    add(makeNode(Fixed, galleryUrl(), i18nc("@item navigation pane", "Gallery"), QStringLiteral("image-x-generic")));
    add(makeNode(Separator, QUrl(), QString(), QString()));
    add(makeNode(Separator, QUrl(), QString(), QString()));
    auto thisPc = makeNode(Fixed, thisPcUrl(), i18nc("@item navigation pane", "This PC"), QStringLiteral("computer"));
    thisPc->expandable = true;
    thisPc->fetched = true;
    add(std::move(thisPc));
    add(makeNode(Fixed, networkUrl(), i18nc("@item navigation pane", "Network"), QStringLiteral("network-workgroup")));
    if (m_showAllFolders) {
        add(makeNode(Fixed, recycleBinUrl(), i18nc("@item navigation pane", "Recycle Bin"), QStringLiteral("user-trash")));
    }
    endResetModel();
    reloadPins();
    reloadDrives();
}

void NavigationModel::setShowAllFolders(bool show)
{
    if (show == m_showAllFolders) {
        return;
    }
    m_showAllFolders = show;
    const int last = int(m_root->children.size());
    if (show) {
        beginInsertRows(QModelIndex(), last, last);
        auto bin = makeNode(Fixed, recycleBinUrl(), i18nc("@item navigation pane", "Recycle Bin"), QStringLiteral("user-trash"));
        bin->parent = m_root.get();
        m_root->children.push_back(std::move(bin));
        endInsertRows();
    } else {
        beginRemoveRows(QModelIndex(), last - 1, last - 1);
        m_root->children.pop_back();
        endRemoveRows();
    }
}

bool NavigationModel::showAllFolders() const
{
    return m_showAllFolders;
}

// Pins sit between the two separators of the top level.
void NavigationModel::reloadPins()
{
    std::vector<std::unique_ptr<Node>> pins;
    if (m_places) {
        const QUrl trash = recycleBinUrl();
        for (int row = 0; row < m_places->rowCount(); ++row) {
            const QModelIndex i = m_places->index(row, 0);
            if (m_places->isHidden(i) || m_places->isDevice(i) || m_places->groupType(i) != KFilePlacesModel::PlacesType) {
                continue;
            }
            const QUrl url = m_places->url(i);
            if (url.isEmpty() || cleaned(url) == trash || url.scheme() == QLatin1String("trash")) {
                continue;
            }
            auto node = makeNode(Pin, url, m_places->text(i), m_places->icon(i).name());
            if (node->iconName.isEmpty()) {
                node->iconName = QStringLiteral("folder");
            }
            // Only local and KIO folders list sub-folders; a search or a
            // recent-files place has none to show.
            node->expandable = (url.isLocalFile() || url.scheme() == QLatin1String("smb") || url.scheme() == QLatin1String("sftp"))
                && mayHaveSubfolders(url);
            pins.push_back(std::move(node));
        }
    }

    int first = -1;
    int last = -1;
    for (size_t i = 0; i < m_root->children.size(); ++i) {
        if (m_root->children[i]->kind == Separator) {
            if (first < 0) {
                first = int(i);
            } else {
                last = int(i);
                break;
            }
        }
    }
    Q_ASSERT(first >= 0 && last > first);

    // Same pins, same order and names: keep the rows (and what is expanded).
    const int oldCount = last - first - 1;
    bool same = oldCount == int(pins.size());
    for (int k = 0; same && k < oldCount; ++k) {
        const Node *old = m_root->children[first + 1 + k].get();
        same = old->url == pins[k]->url && old->text == pins[k]->text && old->iconName == pins[k]->iconName;
    }
    if (same) {
        return;
    }

    if (oldCount > 0) {
        beginRemoveRows(QModelIndex(), first + 1, last - 1);
        for (int k = first + 1; k < last; ++k) {
            dropListers(m_root->children[k].get());
        }
        m_root->children.erase(m_root->children.begin() + first + 1, m_root->children.begin() + last);
        endRemoveRows();
    }
    if (!pins.empty()) {
        beginInsertRows(QModelIndex(), first + 1, first + int(pins.size()));
        for (size_t k = 0; k < pins.size(); ++k) {
            pins[k]->parent = m_root.get();
            m_root->children.insert(m_root->children.begin() + first + 1 + k, std::move(pins[k]));
        }
        endInsertRows();
    }
}

void NavigationModel::reloadDrives()
{
    Node *thisPc = topNode(thisPcUrl());
    if (!thisPc) {
        return;
    }
    std::vector<std::unique_ptr<Node>> drives;
    if (m_drives) {
        const QList<NavPane::Drive> list = m_drives->drives();
        for (const NavPane::Drive &d : list) {
            auto node = makeNode(Drive, d.url, d.name, d.iconName.isEmpty() ? QStringLiteral("drive-harddisk") : d.iconName);
            node->driveId = d.id;
            node->removable = d.removable;
            node->usage = d.usage;
            node->expandable = !d.url.isEmpty();
            drives.push_back(std::move(node));
        }
    }

    // The same drives in the same order: update their rows in place, so an
    // expanded drive stays open while its usage changes.
    bool sameIds = drives.size() == thisPc->children.size();
    for (size_t k = 0; sameIds && k < drives.size(); ++k) {
        sameIds = thisPc->children[k]->driveId == drives[k]->driveId && thisPc->children[k]->url == drives[k]->url;
    }
    if (sameIds) {
        for (size_t k = 0; k < drives.size(); ++k) {
            Node *old = thisPc->children[k].get();
            if (old->text != drives[k]->text || old->iconName != drives[k]->iconName || old->usage != drives[k]->usage
                || old->removable != drives[k]->removable) {
                old->text = drives[k]->text;
                old->iconName = drives[k]->iconName;
                old->usage = drives[k]->usage;
                old->removable = drives[k]->removable;
                const QModelIndex i = indexFor(old);
                Q_EMIT dataChanged(i, i);
            }
        }
        return;
    }
    replaceChildren(thisPc, std::move(drives));
}

void NavigationModel::replaceChildren(Node *parent, std::vector<std::unique_ptr<Node>> children)
{
    const QModelIndex parentIndex = indexFor(parent);
    if (!parent->children.empty()) {
        beginRemoveRows(parentIndex, 0, int(parent->children.size()) - 1);
        for (const auto &c : parent->children) {
            dropListers(c.get());
        }
        parent->children.clear();
        endRemoveRows();
    }
    if (!children.empty()) {
        beginInsertRows(parentIndex, 0, int(children.size()) - 1);
        for (auto &c : children) {
            c->parent = parent;
            parent->children.push_back(std::move(c));
        }
        endInsertRows();
    } else if (parent->kind != Fixed) {
        // Listed and empty: no chevron any more.
        Q_EMIT dataChanged(parentIndex, parentIndex);
    }
}

Node *NavigationModel::topNode(const QUrl &url) const
{
    for (const auto &c : m_root->children) {
        if (c->kind != Separator && c->url == url) {
            return c.get();
        }
    }
    return nullptr;
}

Node *NavigationModel::nodeFor(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return m_root.get();
    }
    return static_cast<Node *>(index.internalPointer());
}

QModelIndex NavigationModel::indexFor(Node *node) const
{
    if (!node || node == m_root.get()) {
        return QModelIndex();
    }
    return createIndex(node->row(), 0, node);
}

QModelIndex NavigationModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0 || row < 0) {
        return QModelIndex();
    }
    Node *p = nodeFor(parent);
    if (row >= int(p->children.size())) {
        return QModelIndex();
    }
    return createIndex(row, 0, p->children[row].get());
}

QModelIndex NavigationModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return QModelIndex();
    }
    Node *n = nodeFor(child);
    return indexFor(n->parent);
}

int NavigationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }
    return int(nodeFor(parent)->children.size());
}

int NavigationModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant NavigationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    const Node *n = nodeFor(index);
    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return n->text;
    case Qt::DecorationRole:
        // KIconEngine, as Plasma's platform theme gives: symbolic icons take
        // the text colour of the palette.
        return n->iconName.isEmpty() ? QVariant() : QVariant(QIcon(new KIconEngine(n->iconName, KIconLoader::global())));
    case Qt::AccessibleTextRole:
        return n->kind == Separator ? QString() : n->text;
    case UrlRole:
        return n->url;
    case KindRole:
        return int(n->kind);
    case IconNameRole:
        return n->iconName;
    case RemovableRole:
        return n->removable;
    case UsageRole:
        return n->usage;
    case DriveIdRole:
        return n->driveId;
    case DepthRole: {
        int depth = 0;
        for (const Node *p = n->parent; p && p->parent; p = p->parent) {
            ++depth;
        }
        return depth;
    }
    default:
        return QVariant();
    }
}

Qt::ItemFlags NavigationModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    const Node *n = nodeFor(index);
    if (n->kind == Separator) {
        return Qt::NoItemFlags;
    }
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    // Home, Gallery, This PC and Network are pages: nothing can be dropped
    // on them. The Recycle Bin takes what is dropped.
    if (n->kind != Fixed || n->url == recycleBinUrl()) {
        if (!n->url.isEmpty()) {
            f |= Qt::ItemIsDropEnabled;
        }
    }
    return f;
}

bool NavigationModel::hasChildren(const QModelIndex &parent) const
{
    const Node *n = nodeFor(parent);
    if (n == m_root.get()) {
        return true;
    }
    if (!n->children.empty()) {
        return true;
    }
    return n->expandable && !n->fetched;
}

bool NavigationModel::canFetchMore(const QModelIndex &parent) const
{
    const Node *n = nodeFor(parent);
    return n != m_root.get() && n->expandable && !n->fetched && !n->lister && !n->url.isEmpty();
}

void NavigationModel::fetchMore(const QModelIndex &parent)
{
    if (!canFetchMore(parent)) {
        return;
    }
    startListing(nodeFor(parent));
}

void NavigationModel::startListing(Node *node)
{
    node->lister = std::make_unique<KCoreDirLister>();
    KCoreDirLister *lister = node->lister.get();
    lister->setDirOnlyMode(true);
    lister->setAutoErrorHandlingEnabled(false);
    lister->setShowHiddenFiles(false);
    ++m_listing;
    auto done = [this, node] {
        finishListing(node);
    };
    connect(lister, &KCoreDirLister::completed, this, done);
    connect(lister, &KCoreDirLister::canceled, this, done);
    connect(lister, &KCoreDirLister::jobError, this, done);
    lister->openUrl(node->url);
}

void NavigationModel::finishListing(Node *node)
{
    KCoreDirLister *lister = node->lister.get();
    if (!lister) {
        return;
    }
    const bool first = !node->fetched;
    node->fetched = true;

    QList<KFileItem> items;
    const KFileItemList all = lister->items();
    for (const KFileItem &item : all) {
        if (item.isDir() && !item.isHidden()) {
            items.append(item);
        }
    }
    // The C locale has no collation rules: numbers would sort as text there.
    QCollator collator(QLocale().language() == QLocale::C ? QLocale(QLocale::English) : QLocale());
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(items.begin(), items.end(), [&collator](const KFileItem &a, const KFileItem &b) {
        return collator.compare(a.text(), b.text()) < 0;
    });

    // Unchanged after a refresh of the folder: keep the rows.
    bool same = !first && int(node->children.size()) == items.size();
    for (int k = 0; same && k < items.size(); ++k) {
        same = node->children[k]->url == items[k].url();
    }
    if (!same) {
        std::vector<std::unique_ptr<Node>> children;
        for (const KFileItem &item : std::as_const(items)) {
            children.push_back(makeNode(Folder, item.url(), item.text(), item.iconName().isEmpty() ? QStringLiteral("folder") : item.iconName()));
        }
        replaceChildren(node, std::move(children));
        if (items.isEmpty()) {
            node->expandable = false;
        }
    }

    if (first) {
        // Later changes of the folder (a folder made or deleted) refresh the rows.
        auto refresh = [this, node] {
            finishListing(node);
        };
        connect(lister, &KCoreDirLister::itemsAdded, this, refresh);
        connect(lister, &KCoreDirLister::itemsDeleted, this, refresh);
        --m_listing;
        if (m_listing == 0) {
            Q_EMIT loadingFinished();
        }
    }
}

bool NavigationModel::isLoading() const
{
    return m_listing > 0;
}

bool NavigationModel::isPinned(const QUrl &url) const
{
    for (const auto &c : m_root->children) {
        if (c->kind == Pin && cleaned(c->url) == cleaned(url)) {
            return true;
        }
    }
    return false;
}

bool NavigationModel::pin(const QUrl &url, const QString &name)
{
    if (!m_places || url.isEmpty() || isPinned(url)) {
        return false;
    }
    // A place the first run hid (a KDE default) comes back instead of being doubled.
    for (int row = 0; row < m_places->rowCount(); ++row) {
        const QModelIndex i = m_places->index(row, 0);
        if (!m_places->isDevice(i) && cleaned(m_places->url(i)) == cleaned(url)) {
            m_places->setPlaceHidden(i, false);
            reloadPins();
            return true;
        }
    }
    QString text = name;
    if (text.isEmpty()) {
        text = url.fileName();
        if (text.isEmpty()) {
            text = url.toDisplayString(QUrl::PreferLocalFile);
        }
    }
    m_places->addPlace(text, url, QStringLiteral("folder"));
    reloadPins();
    return true;
}

bool NavigationModel::unpin(const QUrl &url)
{
    if (!m_places) {
        return false;
    }
    for (int row = 0; row < m_places->rowCount(); ++row) {
        const QModelIndex i = m_places->index(row, 0);
        if (m_places->isDevice(i) || cleaned(m_places->url(i)) != cleaned(url)) {
            continue;
        }
        // Unpinning removes the bookmark, as in Windows; pinning the folder
        // again adds it back.
        m_places->removePlace(i);
        reloadPins();
        return true;
    }
    return false;
}

QModelIndex NavigationModel::indexForUrl(const QUrl &url) const
{
    if (url.isEmpty()) {
        return QModelIndex();
    }
    // Fixed rows match exactly.
    for (const auto &c : m_root->children) {
        if (c->kind == Fixed && c->url == url) {
            return indexFor(c.get());
        }
    }
    // Otherwise the deepest loaded row that holds it; pins first (Windows
    // selects Documents in Quick access before This PC).
    std::function<Node *(Node *)> deepest = [&](Node *n) -> Node * {
        for (const auto &c : n->children) {
            if (c->kind != Separator && contains(c->url, url)) {
                Node *below = deepest(c.get());
                return below ? below : c.get();
            }
        }
        return nullptr;
    };
    Node *best = nullptr;
    int bestLength = -1;
    for (const auto &c : m_root->children) {
        if (c->kind == Separator || c->kind == Fixed) {
            continue;
        }
        if (contains(c->url, url)) {
            Node *n = deepest(c.get());
            Node *hit = n ? n : c.get();
            const int length = cleaned(hit->url).path().size();
            if (length > bestLength) {
                best = hit;
                bestLength = length;
            }
        }
    }
    if (Node *pc = topNode(thisPcUrl())) {
        for (const auto &d : pc->children) {
            if (contains(d->url, url)) {
                Node *n = deepest(d.get());
                Node *hit = n ? n : d.get();
                const int length = cleaned(hit->url).path().size();
                if (length > bestLength) {
                    best = hit;
                    bestLength = length;
                }
            }
        }
    }
    return indexFor(best);
}

QModelIndex NavigationModel::expandTowards(const QUrl &url)
{
    QModelIndex index = indexForUrl(url);
    // Load one level below the row reached, if the folder is further down.
    Node *n = index.isValid() ? nodeFor(index) : nullptr;
    if (n && cleaned(n->url) != cleaned(url) && canFetchMore(index)) {
        startListing(n);
    }
    return index;
}

QHash<int, QByteArray> NavigationModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    roles.insert(UrlRole, "url");
    roles.insert(KindRole, "kind");
    roles.insert(IconNameRole, "iconName");
    roles.insert(RemovableRole, "removable");
    roles.insert(UsageRole, "usage");
    roles.insert(DriveIdRole, "driveId");
    roles.insert(DepthRole, "depth");
    return roles;
}

Qt::DropActions NavigationModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

QStringList NavigationModel::mimeTypes() const
{
    return {QStringLiteral("text/uri-list")};
}

}
