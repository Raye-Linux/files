/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archive/archiveentry.h"
#include "archive/archiveexport.h"

#include <KIO/UDSEntry>

#include <QHash>
#include <QList>
#include <QStringList>

namespace Raye::Archive
{

// The tree of an archive, as the rayearchive worker shows it: every entry
// by its clean path, the folders that only appear in paths added, and links
// followed inside the archive only.
class RAYE_ARCHIVE_EXPORT ArchiveIndex
{
public:
    void build(const QList<Entry> &entries);
    void clear();

    // The entry at path ("" is the root, a folder); nullptr when absent.
    const Entry *find(const QString &path) const;
    // What is directly inside the folder at path (a resolved path).
    QList<Entry> children(const QString &folder) const;

    // path with every link on the way followed ("" is the root). False
    // when a link leads out of the archive, nowhere, or round in circles.
    // The last part may be missing (something about to be made).
    bool resolve(const QString &path, QString *resolved) const;

    // Where something new at path may go: its parent is the root, missing
    // (made on the way) or a real folder, never a file or a link.
    bool canAddAt(const QString &path) const;

    int size() const
    {
        return m_entries.size();
    }

private:
    bool resolveFrom(const QStringList &parts, int depth, QString *out) const;

    QHash<QString, Entry> m_entries;
    QHash<QString, QStringList> m_children;
    Entry m_root;
};

// What KIO lists for an entry. A link shows the kind of what it points to
// (so a link to a folder opens like one) and its target.
RAYE_ARCHIVE_EXPORT KIO::UDSEntry udsEntry(const ArchiveIndex &index, const Entry &entry, const QString &name);

}
