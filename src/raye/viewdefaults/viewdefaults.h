/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <KConfigWatcher>

#include <QByteArray>
#include <QDate>
#include <QObject>
#include <QString>
#include <QUrl>

// How a folder is shown when the user never chose (explorer spec 3.6): the
// eight view modes of Windows on top of Dolphin's three layouts, the
// default of each place, and the date groups of "Group by date".
namespace Raye::ViewDefaults
{

// Windows' View menu, in its order (Ctrl+Shift+1..8).
enum class Mode {
    ExtraLargeIcons,
    LargeIcons,
    MediumIcons,
    SmallIcons,
    List,
    Details,
    Tiles,
    Content,
};
inline constexpr int ModeCount = 8;

// Dolphin's layouts, numbered like DolphinView::Mode.
enum class Layout {
    Icons = 0,
    Details = 1,
    Compact = 2,
};

struct ModeSpec {
    Layout layout;
    int iconSize; // px at 100 %
    int rowHeight; // px; 0 where the row follows the icon and text
    int cellWidth; // px; 0 where the layout decides
};

// The layout and sizes of a mode. compact is Windows' "Compact view": rows
// of 24 instead of 32 in Details and List. Content uses the details layout
// with 48 px icons until a layout of its own exists (spec 3.6 allows it).
DOLPHIN_EXPORT ModeSpec spec(Mode mode, bool compact = false);

// Whether the compact layout of this mode flows in rows (Small icons,
// Tiles) rather than in columns (List).
DOLPHIN_EXPORT bool flowsInRows(Mode mode);

// The mode a Dolphin layout and icon size come back as (an old .directory,
// a zoom by Ctrl+wheel): the nearest Windows mode.
DOLPHIN_EXPORT Mode modeFor(Layout layout, int iconSize);

// The next or previous mode in the order Ctrl+wheel steps through them,
// stopping at the ends.
DOLPHIN_EXPORT Mode step(Mode mode, int steps);

// Name used in view properties and in the [Raye] keys ("details", "list").
DOLPHIN_EXPORT QString modeId(Mode mode);
DOLPHIN_EXPORT bool modeFromId(const QString &id, Mode *mode);

struct Defaults {
    Mode mode = Mode::Details;
    QByteArray groupRole; // empty: not grouped
    QByteArray sortRole = QByteArrayLiteral("text");
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
};

// What a folder looks like before the user changes anything: Medium icons in
// Pictures and Videos (and the Gallery), Downloads grouped by date with the
// newest first, Details everywhere else. homePath and the XDG folders are
// passed in so the rule can be tested; the overload reads them.
struct Places {
    QString pictures;
    QString videos;
    QString downloads;
};
DOLPHIN_EXPORT Defaults forUrl(const QUrl &url, const Places &places);
DOLPHIN_EXPORT Defaults forUrl(const QUrl &url);

// Windows' "Group by date" headers: Today, Yesterday, Earlier this week,
// Last week, Earlier this month, Last month, Earlier this year, A long time
// ago (and the same going forward for dates after today). The week starts
// on the locale's first day.
enum class DateGroup {
    Today,
    Yesterday,
    EarlierThisWeek,
    LastWeek,
    EarlierThisMonth,
    LastMonth,
    EarlierThisYear,
    LongAgo,
    Tomorrow,
    LaterThisWeek,
    NextWeek,
    LaterThisMonth,
    NextMonth,
    LaterThisYear,
    LongFromNow,
};
DOLPHIN_EXPORT DateGroup dateGroup(const QDate &date, const QDate &today, Qt::DayOfWeek firstDayOfWeek);
DOLPHIN_EXPORT QString dateGroupName(DateGroup group);
DOLPHIN_EXPORT QString dateGroupName(const QDate &date, const QDate &today);

// The [Raye] keys of dolphinrc the views follow (spec 3.18), watched so a
// change from Raye Settings or another window applies at once.
class DOLPHIN_EXPORT Settings : public QObject
{
    Q_OBJECT

public:
    static Settings *instance();

    bool showExtensions() const
    {
        return m_showExtensions;
    }
    bool compact() const
    {
        return m_compact;
    }
    bool itemCheckBoxes() const
    {
        return m_itemCheckBoxes;
    }

    // For the views' own toggles (View > Show): written to dolphinrc.
    void setShowExtensions(bool show);
    void setCompact(bool compact);
    void setItemCheckBoxes(bool show);

    // Reads the keys again; called by the watcher.
    void reload();

Q_SIGNALS:
    void showExtensionsChanged(bool show);
    void compactChanged(bool compact);
    void itemCheckBoxesChanged(bool show);

private:
    Settings();
    void write(const char *key, bool value);

    KConfigWatcher::Ptr m_watcher;
    bool m_showExtensions = false;
    bool m_compact = false;
    bool m_itemCheckBoxes = false;
};

}
