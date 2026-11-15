/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archiveindex.h"

#include "archive/archivepaths.h"

#include <sys/stat.h>

namespace Raye::Archive
{

namespace
{
constexpr int linkDepthLimit = 8;

QString parentOf(const QString &path)
{
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? QString() : path.left(slash);
}

QString nameOf(const QString &path)
{
    return path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
}
}

void ArchiveIndex::clear()
{
    m_entries.clear();
    m_children.clear();
}

void ArchiveIndex::build(const QList<Entry> &entries)
{
    clear();
    m_root = Entry();
    m_root.type = Entry::Folder;
    m_root.permissions = 0755;
    m_children.insert(QString(), {});

    auto addFolderChain = [this](const QString &path) {
        // Folders named only in the paths of what they hold.
        QString child = path;
        QString parent = parentOf(child);
        while (true) {
            QStringList &list = m_children[parent];
            if (!list.contains(child)) {
                list.append(child);
            }
            if (parent.isEmpty() || m_entries.contains(parent)) {
                break;
            }
            Entry folder;
            folder.path = parent;
            folder.type = Entry::Folder;
            folder.permissions = 0755;
            m_entries.insert(parent, folder);
            child = parent;
            parent = parentOf(child);
        }
    };

    for (const Entry &entry : entries) {
        if (entry.path.isEmpty()) {
            continue;
        }
        const auto existing = m_entries.constFind(entry.path);
        if (existing != m_entries.constEnd()) {
            // A folder made up from a path gets its real entry (times,
            // permissions); otherwise the first one stays.
            if (existing->type == Entry::Folder && entry.type == Entry::Folder) {
                m_entries.insert(entry.path, entry);
            }
            continue;
        }
        m_entries.insert(entry.path, entry);
        addFolderChain(entry.path);
    }
}

const Entry *ArchiveIndex::find(const QString &path) const
{
    if (path.isEmpty()) {
        return &m_root;
    }
    const auto it = m_entries.constFind(path);
    return it == m_entries.constEnd() ? nullptr : &it.value();
}

QList<Entry> ArchiveIndex::children(const QString &folder) const
{
    QList<Entry> out;
    const auto it = m_children.constFind(folder);
    if (it == m_children.constEnd()) {
        return out;
    }
    out.reserve(it->size());
    for (const QString &path : *it) {
        if (const Entry *e = find(path)) {
            out.append(*e);
        }
    }
    return out;
}

bool ArchiveIndex::resolveFrom(const QStringList &parts, int depth, QString *out) const
{
    if (depth > linkDepthLimit) {
        return false;
    }
    QStringList done;
    for (int i = 0; i < parts.size(); ++i) {
        const QString &part = parts.at(i);
        if (part.isEmpty() || part == QLatin1String(".")) {
            continue;
        }
        if (part == QLatin1String("..")) {
            if (done.isEmpty()) {
                return false;
            }
            done.removeLast();
            continue;
        }
        done.append(part);
        const QString current = done.join(QLatin1Char('/'));
        const Entry *e = find(current);
        if (!e) {
            // the last part may not exist yet; anything before it must
            if (i + 1 != parts.size()) {
                return false;
            }
            break;
        }
        if (e->type != Entry::Symlink) {
            continue;
        }
        if (e->linkTarget.startsWith(QLatin1Char('/'))) {
            return false;
        }
        done.removeLast();
        return resolveFrom(done + e->linkTarget.split(QLatin1Char('/')) + parts.mid(i + 1), depth + 1, out);
    }
    *out = done.join(QLatin1Char('/'));
    return true;
}

bool ArchiveIndex::resolve(const QString &path, QString *resolved) const
{
    return resolveFrom(path.split(QLatin1Char('/')), 0, resolved);
}

bool ArchiveIndex::canAddAt(const QString &path) const
{
    QString clean;
    if (cleanEntryPath(path, &clean) != PathVerdict::Ok || clean != path) {
        return false;
    }
    QString parent = parentOf(path);
    while (!parent.isEmpty()) {
        if (const Entry *e = find(parent)) {
            return e->type == Entry::Folder;
        }
        parent = parentOf(parent);
    }
    return true;
}

KIO::UDSEntry udsEntry(const ArchiveIndex &index, const Entry &entry, const QString &name)
{
    KIO::UDSEntry uds;
    uds.reserve(7);
    uds.fastInsert(KIO::UDSEntry::UDS_NAME, name.isEmpty() ? nameOf(entry.path) : name);
    mode_t type = S_IFREG;
    qint64 size = entry.size;
    if (entry.type == Entry::Folder) {
        type = S_IFDIR;
        size = 0;
    } else if (entry.type == Entry::Symlink) {
        uds.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, entry.linkTarget);
        QString target;
        const Entry *t = index.resolve(entry.path, &target) ? index.find(target) : nullptr;
        if (t && t->type == Entry::Folder) {
            type = S_IFDIR;
            size = 0;
        } else if (t) {
            size = t->size;
        }
    }
    uds.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, type);
    uds.fastInsert(KIO::UDSEntry::UDS_ACCESS, entry.permissions & 0777);
    if (size >= 0) {
        uds.fastInsert(KIO::UDSEntry::UDS_SIZE, size);
    }
    if (entry.modified.isValid()) {
        uds.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, entry.modified.toSecsSinceEpoch());
    }
    if (type == S_IFDIR) {
        uds.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    }
    return uds;
}

}
