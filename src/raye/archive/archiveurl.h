/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archiveexport.h"

#include <QString>
#include <QUrl>

#include <functional>

// rayearchive: URLs. "rayearchive:/home/ana/Holiday.zip/photos/1.jpg" is the
// entry "photos/1.jpg" of the local file /home/ana/Holiday.zip.
namespace Raye::Archive
{

inline constexpr char16_t urlScheme[] = u"rayearchive";

struct RAYE_ARCHIVE_EXPORT ArchiveLocation {
    QString archivePath;
    // A clean path inside the archive, "" for its root.
    QString innerPath;

    bool isValid() const
    {
        return !archivePath.isEmpty();
    }
};

// Splits an absolute path at the first component that is a file (asked to
// isFile, which the worker answers with a stat). Invalid when no part of
// it is a file.
RAYE_ARCHIVE_EXPORT ArchiveLocation splitArchivePath(const QString &path, const std::function<bool(const QString &)> &isFile);

// The URL Files opens instead of a local archive it browses, or an empty
// URL when the file is not one (DolphinView::openItemAsFolderUrl calls it).
RAYE_ARCHIVE_EXPORT QUrl browseUrl(const QUrl &localUrl, const QString &mimeType);

// The rayearchive: URL of an entry.
RAYE_ARCHIVE_EXPORT QUrl entryUrl(const QString &archivePath, const QString &innerPath);

}
