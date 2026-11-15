/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "searchoptions.h"

#include <KLocalizedString>

#include <QMimeDatabase>
#include <QMimeType>
#include <QUrlQuery>

namespace Raye::Search
{
namespace
{
constexpr qint64 KiB = 1024;
constexpr qint64 MiB = 1024 * KiB;
constexpr qint64 GiB = 1024 * MiB;

bool inheritsAny(const QMimeType &mime, std::initializer_list<const char *> names)
{
    for (const char *name : names) {
        if (mime.inherits(QString::fromLatin1(name))) {
            return true;
        }
    }
    return false;
}

bool isArchive(const QMimeType &mime)
{
    return inheritsAny(mime,
                       {"application/zip", "application/x-7z-compressed", "application/x-tar", "application/x-compressed-tar", "application/x-bzip-compressed-tar",
                        "application/x-xz-compressed-tar", "application/x-zstd-compressed-tar", "application/vnd.rar", "application/x-rar",
                        "application/gzip", "application/x-bzip2", "application/x-xz", "application/zstd", "application/x-cd-image"})
        // Office files are ZIP files too, but they are documents.
        && !inheritsAny(mime, {"application/vnd.openxmlformats-officedocument.wordprocessingml.document", "application/vnd.oasis.opendocument.text"})
        && !mime.name().startsWith(QLatin1String("application/vnd.openxmlformats")) && !mime.name().startsWith(QLatin1String("application/vnd.oasis"))
        && !mime.name().startsWith(QLatin1String("application/vnd.ms-")) && mime.name() != QLatin1String("application/epub+zip")
        && !mime.name().startsWith(QLatin1String("application/x-java"));
}

bool isDocument(const QMimeType &mime)
{
    const QString name = mime.name();
    if (name.startsWith(QLatin1String("text/"))) {
        return true;
    }
    return inheritsAny(mime,
                       {"application/pdf", "application/rtf", "application/msword", "application/vnd.ms-excel", "application/vnd.ms-powerpoint",
                        "application/epub+zip", "application/x-mobipocket-ebook", "application/vnd.comicbook+zip"})
        || name.startsWith(QLatin1String("application/vnd.oasis.opendocument")) || name.startsWith(QLatin1String("application/vnd.openxmlformats-officedocument"))
        || name.startsWith(QLatin1String("application/vnd.ms-word")) || name.startsWith(QLatin1String("application/vnd.ms-excel"))
        || name.startsWith(QLatin1String("application/vnd.ms-powerpoint"));
}
}

QString placeholderText(const QString &folderName)
{
    if (folderName.trimmed().isEmpty()) {
        return i18nc("@info:placeholder the search box", "Search");
    }
    return i18nc("@info:placeholder the search box, %1 is the folder", "Search %1", folderName);
}

QStringList addRecent(const QStringList &recent, const QString &term)
{
    const QString words = term.simplified();
    if (words.isEmpty()) {
        return recent;
    }
    QStringList out{words};
    for (const QString &old : recent) {
        if (out.size() >= recentCount) {
            break;
        }
        if (old.compare(words, Qt::CaseInsensitive) != 0 && !old.trimmed().isEmpty()) {
            out.append(old);
        }
    }
    return out;
}

std::pair<QDate, QDate> dateBounds(DateRange range, const QDate &today, Qt::DayOfWeek firstDayOfWeek)
{
    if (!today.isValid()) {
        return {};
    }
    const int back = (today.dayOfWeek() - int(firstDayOfWeek) + 7) % 7;
    const QDate weekStart = today.addDays(-back);
    const QDate monthStart(today.year(), today.month(), 1);
    switch (range) {
    case DateRange::Any:
        return {};
    case DateRange::Today:
        return {today, today};
    case DateRange::Yesterday:
        return {today.addDays(-1), today.addDays(-1)};
    case DateRange::ThisWeek:
        return {weekStart, today};
    case DateRange::LastWeek:
        return {weekStart.addDays(-7), weekStart.addDays(-1)};
    case DateRange::ThisMonth:
        return {monthStart, today};
    case DateRange::LastMonth:
        return {monthStart.addMonths(-1), monthStart.addDays(-1)};
    case DateRange::ThisYear:
        return {QDate(today.year(), 1, 1), today};
    case DateRange::LastYear:
        return {QDate(today.year() - 1, 1, 1), QDate(today.year() - 1, 12, 31)};
    }
    return {};
}

std::pair<qint64, qint64> sizeBounds(SizeRange size)
{
    switch (size) {
    case SizeRange::Any:
        return {0, -1};
    case SizeRange::Empty:
        return {0, 1};
    case SizeRange::Tiny:
        return {1, 16 * KiB};
    case SizeRange::Small:
        return {16 * KiB, MiB};
    case SizeRange::Medium:
        return {MiB, 128 * MiB};
    case SizeRange::Large:
        return {128 * MiB, GiB};
    case SizeRange::Huge:
        return {GiB, 4 * GiB};
    case SizeRange::Gigantic:
        return {4 * GiB, -1};
    }
    return {0, -1};
}

bool kindMatches(Kind kind, const QMimeType &mime, bool isDir)
{
    switch (kind) {
    case Kind::Any:
        return true;
    case Kind::Folder:
        return isDir;
    default:
        break;
    }
    if (isDir || !mime.isValid()) {
        return false;
    }
    const QString name = mime.name();
    switch (kind) {
    case Kind::Picture:
        return name.startsWith(QLatin1String("image/"));
    case Kind::Music:
        return name.startsWith(QLatin1String("audio/"));
    case Kind::Video:
        return name.startsWith(QLatin1String("video/"));
    case Kind::Archive:
        return isArchive(mime);
    case Kind::Document:
        return isDocument(mime) && !isArchive(mime);
    default:
        return false;
    }
}

QStringList kindMimeTypes(Kind kind)
{
    if (kind == Kind::Any) {
        return {};
    }
    if (kind == Kind::Folder) {
        return {QStringLiteral("inode/directory")};
    }
    QStringList out;
    const QList<QMimeType> all = QMimeDatabase().allMimeTypes();
    for (const QMimeType &mime : all) {
        if (kindMatches(kind, mime, false)) {
            out.append(mime.name());
        }
    }
    out.sort();
    return out;
}

bool acceptsResult(const Options &options, const QMimeType &mime, bool isDir, const QDateTime &modified, qint64 size, const QDate &today,
                   Qt::DayOfWeek firstDayOfWeek)
{
    if (!kindMatches(options.kind, mime, isDir)) {
        return false;
    }
    if (options.date != DateRange::Any) {
        const auto [first, last] = dateBounds(options.date, today, firstDayOfWeek);
        const QDate day = modified.toLocalTime().date();
        if (!day.isValid() || day < first || day > last) {
            return false;
        }
    }
    if (options.size != SizeRange::Any) {
        // A folder has no size of its own: Windows leaves folders out.
        if (isDir || size < 0) {
            return false;
        }
        const auto [minimum, maximum] = sizeBounds(options.size);
        if (size < minimum || (maximum >= 0 && size >= maximum)) {
            return false;
        }
    }
    return true;
}

QUrl fileNameSearchUrl(const Options &options, const QUrl &folder, const QString &title)
{
    // The same form as Dolphin's own search URLs, so every part of Files that
    // reads them (the view, the address row, the tab title) sees a search.
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("search"), QString::fromUtf8(QUrl::toPercentEncoding(options.trimmedTerm())));
    if (options.fileContents) {
        query.addQueryItem(QStringLiteral("checkContent"), QStringLiteral("yes"));
    }
    const QUrl where = folder.isValid() && !folder.isEmpty() ? folder : QUrl(QStringLiteral("file:///"));
    query.addQueryItem(QStringLiteral("url"), QString::fromUtf8(QUrl::toPercentEncoding(where.url())));
    if (!title.isEmpty()) {
        query.addQueryItem(QStringLiteral("title"), QString::fromUtf8(QUrl::toPercentEncoding(title)));
    }
    QUrl url;
    url.setScheme(QStringLiteral("filenamesearch"));
    url.setQuery(query);
    return url;
}

bool isSearchUrl(const QUrl &url)
{
    const QString scheme = url.scheme();
    return scheme == QLatin1String("filenamesearch") || scheme == QLatin1String("baloosearch");
}

}
