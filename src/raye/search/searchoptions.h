/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <utility>

class QMimeType;

// What the search box of Files asks for (explorer spec 1.2, 3.3): the words,
// where to look, and the "Search options" of the command bar (Kind, Date
// modified, Size, "Search file contents", "Current folder / All
// subfolders"). Pure: the box, the URL builders and the tests share it.
namespace Raye::Search
{

enum class Scope {
    AllSubfolders, // the folder and everything under it: a real search
    CurrentFolder, // only what the folder shows: filters the view
};

enum class Kind {
    Any,
    Folder,
    Document,
    Picture,
    Music,
    Video,
    Archive,
};

enum class DateRange {
    Any,
    Today,
    Yesterday,
    ThisWeek,
    LastWeek,
    ThisMonth,
    LastMonth,
    ThisYear,
    LastYear,
};

// Windows' size groups.
enum class SizeRange {
    Any,
    Empty, // 0 KB
    Tiny, // up to 16 KB
    Small, // 16 KB to 1 MB
    Medium, // 1 to 128 MB
    Large, // 128 MB to 1 GB
    Huge, // 1 to 4 GB
    Gigantic, // more than 4 GB
};

struct DOLPHIN_EXPORT Options {
    QString term;
    Scope scope = Scope::AllSubfolders;
    bool fileContents = false;
    Kind kind = Kind::Any;
    DateRange date = DateRange::Any;
    SizeRange size = SizeRange::Any;

    // The words without the blanks around them.
    QString trimmedTerm() const
    {
        return term.trimmed();
    }
    // Some filter (kind, date or size) is set.
    bool hasFilters() const
    {
        return kind != Kind::Any || date != DateRange::Any || size != SizeRange::Any;
    }
    // There is something to search for: words or a filter.
    bool isActive() const
    {
        return !trimmedTerm().isEmpty() || hasFilters();
    }
    bool operator==(const Options &other) const = default;
};

// How long the box waits after the last key before it searches.
constexpr int debounceMs = 300;
// Recent searches kept (dolphinrc [Raye] RecentSearches).
constexpr int recentCount = 10;

// "Search Documents" / "Rechercher dans Documents"; "Search" alone without
// a name.
DOLPHIN_EXPORT QString placeholderText(const QString &folderName);

// The words go first, newest on top, once (case does not count), at most
// recentCount. Blank words leave the list as it is.
DOLPHIN_EXPORT QStringList addRecent(const QStringList &recent, const QString &term);

// Inclusive first and last day of a date range, for today; weeks start on
// firstDayOfWeek (QLocale's). Any: two invalid dates.
DOLPHIN_EXPORT std::pair<QDate, QDate> dateBounds(DateRange range, const QDate &today, Qt::DayOfWeek firstDayOfWeek);

// Byte bounds of a size group: [minimum, maximum), maximum -1 for none.
DOLPHIN_EXPORT std::pair<qint64, qint64> sizeBounds(SizeRange size);

// Whether a MIME type belongs to a kind.
DOLPHIN_EXPORT bool kindMatches(Kind kind, const QMimeType &mime, bool isDir);
// Every MIME type of a kind, for the view's type filter (Current folder);
// empty for Any.
DOLPHIN_EXPORT QStringList kindMimeTypes(Kind kind);

// The filters on one result (the search tools do not all apply them: the
// view checks them too). The words are not checked here.
DOLPHIN_EXPORT bool acceptsResult(const Options &options, const QMimeType &mime, bool isDir, const QDateTime &modified, qint64 size, const QDate &today,
                                  Qt::DayOfWeek firstDayOfWeek);

// A filenamesearch URL, the form Dolphin's search uses, for a search of the
// words in folder and below. Used when no other builder is set (see
// SearchBox::setUrlBuilder): it searches names, or contents too.
DOLPHIN_EXPORT QUrl fileNameSearchUrl(const Options &options, const QUrl &folder, const QString &title = QString());

// Whether a URL is a search (filenamesearch, baloosearch).
DOLPHIN_EXPORT bool isSearchUrl(const QUrl &url);

}
