/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "previewloader.h"

#include <KPluginMetaData>

#include <QImageReader>
#include <QMimeDatabase>
#include <QSet>

namespace
{
const QString okular = QStringLiteral("okularpart");
const QString kate = QStringLiteral("katepart");

bool inherits(const QMimeType &type, const char *name)
{
    return type.inherits(QString::fromLatin1(name));
}

// Pictures Qt can decode here (its image plugins), by MIME type.
const QSet<QString> &imageTypes()
{
    static const QSet<QString> set = [] {
        QSet<QString> s;
        const auto types = QImageReader::supportedMimeTypes();
        for (const QByteArray &t : types) {
            s.insert(QString::fromLatin1(t));
        }
        return s;
    }();
    return set;
}

// Okular's types: documents and e-books. Pictures go to Files' own view.
bool isDocument(const QMimeType &type)
{
    static const char *const names[] = {
        "application/pdf",
        "application/epub+zip",
        "image/vnd.djvu",
        "application/x-cbz",
        "application/x-cbr",
        "application/vnd.comicbook+zip",
        "application/vnd.comicbook-rar",
        "application/postscript",
        "application/oxps",
        "application/vnd.ms-xpsdocument",
        "application/x-fictionbook+xml",
    };
    for (const char *n : names) {
        if (type.name() == QLatin1String(n) || inherits(type, n)) {
            return true;
        }
    }
    return false;
}

bool isOffice(const QMimeType &type)
{
    const QString n = type.name();
    return n.startsWith(QLatin1String("application/vnd.oasis.opendocument")) || n.startsWith(QLatin1String("application/vnd.openxmlformats-officedocument"))
        || n == QLatin1String("application/msword") || n == QLatin1String("application/vnd.ms-excel")
        || n == QLatin1String("application/vnd.ms-powerpoint") || n == QLatin1String("application/rtf");
}

bool isFont(const QMimeType &type)
{
    const QString n = type.name();
    return n == QLatin1String("font/ttf") || n == QLatin1String("font/otf") || n == QLatin1String("font/collection")
        || inherits(type, "font/ttf") || inherits(type, "font/otf");
}

bool isText(const QMimeType &type)
{
    // SVG and HTML are text underneath, but people expect the picture or the page.
    if (type.name() == QLatin1String("image/svg+xml") || type.name() == QLatin1String("text/html")) {
        return false;
    }
    return inherits(type, "text/plain") || type.name() == QLatin1String("application/json")
        || type.name() == QLatin1String("application/x-shellscript") || inherits(type, "application/xml");
}
}

namespace Raye::Preview
{

Loader loaderFor(const QString &mimeType, const QStringList &installed)
{
    Loader loader;
    if (mimeType.isEmpty() || mimeType == QLatin1String("inode/directory")) {
        return loader;
    }
    QMimeDatabase db;
    const QMimeType type = db.mimeTypeForName(mimeType);
    if (!type.isValid()) {
        return loader;
    }

    if (isDocument(type)) {
        if (installed.contains(okular)) {
            return {Kind::Document, okular, QStringLiteral("document")};
        }
        return {Kind::Thumbnail, QString(), QString()};
    }
    if (imageTypes().contains(type.name()) || (type.name().startsWith(QLatin1String("image/")) && imageTypes().contains(type.name()))) {
        return {Kind::Image, QString(), QString()};
    }
    if (isFont(type)) {
        return {Kind::Font, QString(), QString()};
    }
    if (type.name().startsWith(QLatin1String("audio/"))) {
        return {Kind::Audio, QString(), QStringLiteral("media")};
    }
    if (type.name().startsWith(QLatin1String("video/"))) {
        return {Kind::Video, QString(), QStringLiteral("media")};
    }
    if (isOffice(type)) {
        return {Kind::Thumbnail, QString(), QString()};
    }
    if (isText(type)) {
        if (installed.contains(kate)) {
            return {Kind::Text, kate, QStringLiteral("text")};
        }
        return {Kind::Text, QString(), QStringLiteral("text")};
    }
    // Something the image plugins read though its name says otherwise.
    if (type.name().startsWith(QLatin1String("image/"))) {
        return {Kind::Thumbnail, QString(), QString()};
    }
    return loader;
}

Loader loaderFor(const QString &mimeType)
{
    static const QStringList parts = installedParts();
    return loaderFor(mimeType, parts);
}

QStringList installedParts()
{
    QStringList ids;
    const QList<KPluginMetaData> plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/parts"));
    for (const KPluginMetaData &md : plugins) {
        ids.append(md.pluginId());
    }
    return ids;
}

}
