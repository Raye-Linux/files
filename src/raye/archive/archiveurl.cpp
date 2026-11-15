/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archiveurl.h"

#include "archiveformat.h"

#include <QDir>
#include <QStringList>

namespace Raye::Archive
{

ArchiveLocation splitArchivePath(const QString &path, const std::function<bool(const QString &)> &isFile)
{
    if (!path.startsWith(QLatin1Char('/'))) {
        return {};
    }
    const QStringList parts = QDir::cleanPath(path).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString prefix;
    for (int i = 0; i < parts.size(); ++i) {
        prefix += QLatin1Char('/') + parts.at(i);
        if (isFile(prefix)) {
            return {prefix, parts.mid(i + 1).join(QLatin1Char('/'))};
        }
    }
    return {};
}

QUrl browseUrl(const QUrl &localUrl, const QString &mimeType)
{
    if (!localUrl.isLocalFile() || !isBrowsableMimeType(mimeType)) {
        return QUrl();
    }
    return entryUrl(localUrl.toLocalFile(), QString());
}

QUrl entryUrl(const QString &archivePath, const QString &innerPath)
{
    QUrl url;
    url.setScheme(QString::fromUtf16(urlScheme));
    url.setPath(innerPath.isEmpty() ? archivePath : archivePath + QLatin1Char('/') + innerPath);
    return url;
}

}
