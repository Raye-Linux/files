/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "viewdefaults.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDir>
#include <QLocale>
#include <QStandardPaths>

namespace
{
using Raye::ViewDefaults::Layout;
using Raye::ViewDefaults::Mode;

struct ModeRow {
    Mode mode;
    const char *id;
    Layout layout;
    int iconSize;
};

// Windows' sizes (spec 1.5 and 3.6). Small icons, List and Tiles share
// Dolphin's compact layout (a name beside the icon, more lines under it);
// Content (one wide row per item) stands on the details layout with big
// icons until a layout of its own lands.
constexpr ModeRow modes[] = {
    {Mode::ExtraLargeIcons, "extralarge", Layout::Icons, 256},
    {Mode::LargeIcons, "large", Layout::Icons, 96},
    {Mode::MediumIcons, "medium", Layout::Icons, 48},
    {Mode::SmallIcons, "small", Layout::Compact, 16},
    {Mode::List, "list", Layout::Compact, 16},
    {Mode::Details, "details", Layout::Details, 16},
    {Mode::Tiles, "tiles", Layout::Compact, 48},
    {Mode::Content, "content", Layout::Details, 48},
};

const ModeRow &row(Mode mode)
{
    return modes[int(mode)];
}

bool isUnder(const QString &path, const QString &folder)
{
    if (folder.isEmpty() || path.isEmpty()) {
        return false;
    }
    const QString clean = QDir::cleanPath(folder);
    // The home itself is never a "Pictures" folder, even when the XDG file
    // points the folder there.
    if (clean == QDir::cleanPath(QDir::homePath()) || clean == QLatin1String("/")) {
        return false;
    }
    const QString p = QDir::cleanPath(path);
    return p == clean || p.startsWith(clean + QLatin1Char('/'));
}

constexpr const char *settingsGroup = "Raye";
}

namespace Raye::ViewDefaults
{

ModeSpec spec(Mode mode, bool compact)
{
    const ModeRow &r = row(mode);
    ModeSpec s{r.layout, r.iconSize, 0, 0};
    switch (mode) {
    case Mode::SmallIcons:
        // Flows in rows, a name beside each icon, like Windows' Small icons.
        s.rowHeight = compact ? 24 : 32;
        s.cellWidth = 240;
        break;
    case Mode::List:
    case Mode::Details:
        s.rowHeight = compact ? 24 : 32;
        break;
    case Mode::Tiles:
        s.cellWidth = 272;
        s.rowHeight = 64;
        break;
    case Mode::Content:
        s.rowHeight = 72;
        break;
    default:
        break;
    }
    return s;
}

bool flowsInRows(Mode mode)
{
    return mode == Mode::SmallIcons || mode == Mode::Tiles;
}

Mode modeFor(Layout layout, int iconSize)
{
    switch (layout) {
    case Layout::Details:
        return iconSize >= 40 ? Mode::Content : Mode::Details;
    case Layout::Compact:
        return iconSize >= 40 ? Mode::Tiles : Mode::List;
    case Layout::Icons:
        break;
    }
    // The icon size Ctrl+wheel left: the nearest of the four icon modes,
    // measured on a log scale so 128 is Large, not Extra large.
    if (iconSize <= 0) {
        return Mode::MediumIcons;
    }
    if (iconSize < 32) {
        return Mode::SmallIcons;
    }
    if (iconSize < 69) { // sqrt(48 * 96)
        return Mode::MediumIcons;
    }
    if (iconSize < 157) { // sqrt(96 * 256)
        return Mode::LargeIcons;
    }
    return Mode::ExtraLargeIcons;
}

Mode step(Mode mode, int steps)
{
    int i = int(mode) + steps;
    if (i < 0) {
        i = 0;
    } else if (i >= ModeCount) {
        i = ModeCount - 1;
    }
    return Mode(i);
}

QString modeId(Mode mode)
{
    return QString::fromLatin1(row(mode).id);
}

bool modeFromId(const QString &id, Mode *mode)
{
    for (const ModeRow &r : modes) {
        if (id == QLatin1String(r.id)) {
            if (mode) {
                *mode = r.mode;
            }
            return true;
        }
    }
    return false;
}

Defaults forUrl(const QUrl &url, const Places &places)
{
    Defaults d;
    if (url.scheme() == QLatin1String("raye") && url.path() == QLatin1String("gallery")) {
        d.mode = Mode::MediumIcons;
        return d;
    }
    if (!url.isLocalFile()) {
        return d;
    }
    const QString path = url.toLocalFile();
    if (isUnder(path, places.pictures) || isUnder(path, places.videos)) {
        d.mode = Mode::MediumIcons;
    } else if (isUnder(path, places.downloads) && QDir::cleanPath(path) == QDir::cleanPath(places.downloads)) {
        // Only Downloads itself: a project unpacked inside it is a normal folder.
        d.groupRole = QByteArrayLiteral("modificationtime");
        d.sortRole = QByteArrayLiteral("modificationtime");
        d.sortOrder = Qt::DescendingOrder;
    }
    return d;
}

Defaults forUrl(const QUrl &url)
{
    Places places;
    places.pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    places.videos = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    places.downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return forUrl(url, places);
}

DateGroup dateGroup(const QDate &date, const QDate &today, Qt::DayOfWeek firstDayOfWeek)
{
    if (!date.isValid() || !today.isValid()) {
        return DateGroup::LongAgo;
    }
    const qint64 days = date.daysTo(today); // > 0: in the past
    if (days == 0) {
        return DateGroup::Today;
    }
    if (days == 1) {
        return DateGroup::Yesterday;
    }
    if (days == -1) {
        return DateGroup::Tomorrow;
    }

    const int offset = (today.dayOfWeek() - int(firstDayOfWeek) + 7) % 7;
    const QDate weekStart = today.addDays(-offset);
    const QDate nextWeekStart = weekStart.addDays(7);
    const bool sameMonth = date.year() == today.year() && date.month() == today.month();

    if (days > 0) {
        if (date >= weekStart) {
            return DateGroup::EarlierThisWeek;
        }
        if (date >= weekStart.addDays(-7)) {
            return DateGroup::LastWeek;
        }
        if (sameMonth) {
            return DateGroup::EarlierThisMonth;
        }
        const QDate lastMonth = today.addMonths(-1);
        if (date.year() == lastMonth.year() && date.month() == lastMonth.month()) {
            return DateGroup::LastMonth;
        }
        if (date.year() == today.year()) {
            return DateGroup::EarlierThisYear;
        }
        return DateGroup::LongAgo;
    }

    if (date < nextWeekStart) {
        return DateGroup::LaterThisWeek;
    }
    if (date < nextWeekStart.addDays(7)) {
        return DateGroup::NextWeek;
    }
    if (sameMonth) {
        return DateGroup::LaterThisMonth;
    }
    const QDate nextMonth = today.addMonths(1);
    if (date.year() == nextMonth.year() && date.month() == nextMonth.month()) {
        return DateGroup::NextMonth;
    }
    if (date.year() == today.year()) {
        return DateGroup::LaterThisYear;
    }
    return DateGroup::LongFromNow;
}

QString dateGroupName(DateGroup group)
{
    switch (group) {
    case DateGroup::Today:
        return i18nc("@title:group date", "Today");
    case DateGroup::Yesterday:
        return i18nc("@title:group date", "Yesterday");
    case DateGroup::EarlierThisWeek:
        return i18nc("@title:group date", "Earlier this week");
    case DateGroup::LastWeek:
        return i18nc("@title:group date", "Last week");
    case DateGroup::EarlierThisMonth:
        return i18nc("@title:group date", "Earlier this month");
    case DateGroup::LastMonth:
        return i18nc("@title:group date", "Last month");
    case DateGroup::EarlierThisYear:
        return i18nc("@title:group date", "Earlier this year");
    case DateGroup::LongAgo:
        return i18nc("@title:group date", "A long time ago");
    case DateGroup::Tomorrow:
        return i18nc("@title:group date", "Tomorrow");
    case DateGroup::LaterThisWeek:
        return i18nc("@title:group date", "Later this week");
    case DateGroup::NextWeek:
        return i18nc("@title:group date", "Next week");
    case DateGroup::LaterThisMonth:
        return i18nc("@title:group date", "Later this month");
    case DateGroup::NextMonth:
        return i18nc("@title:group date", "Next month");
    case DateGroup::LaterThisYear:
        return i18nc("@title:group date", "Later this year");
    case DateGroup::LongFromNow:
        return i18nc("@title:group date", "A long time from now");
    }
    return QString();
}

QString dateGroupName(const QDate &date, const QDate &today)
{
    return dateGroupName(dateGroup(date, today, QLocale().firstDayOfWeek()));
}

Settings *Settings::instance()
{
    static Settings *settings = new Settings();
    return settings;
}

Settings::Settings()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("dolphinrc"));
    m_watcher = KConfigWatcher::create(config);
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == QLatin1String(settingsGroup)) {
            reload();
        }
    });
    reload();
}

void Settings::reload()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("dolphinrc"));
    config->reparseConfiguration();
    const KConfigGroup group = config->group(QLatin1String(settingsGroup));
    const bool showExtensions = group.readEntry("ShowExtensions", false);
    const bool compact = group.readEntry("Compact", false);
    const bool checkBoxes = group.readEntry("ItemCheckBoxes", false);
    if (showExtensions != m_showExtensions) {
        m_showExtensions = showExtensions;
        Q_EMIT showExtensionsChanged(showExtensions);
    }
    if (compact != m_compact) {
        m_compact = compact;
        Q_EMIT compactChanged(compact);
    }
    if (checkBoxes != m_itemCheckBoxes) {
        m_itemCheckBoxes = checkBoxes;
        Q_EMIT itemCheckBoxesChanged(checkBoxes);
    }
}

void Settings::write(const char *key, bool value)
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("dolphinrc"));
    KConfigGroup group = config->group(QLatin1String(settingsGroup));
    // Notify: the other windows and Raye Settings follow through their watchers.
    group.writeEntry(key, value, KConfig::Notify);
    config->sync();
}

void Settings::setShowExtensions(bool show)
{
    write("ShowExtensions", show);
    reload();
}

void Settings::setCompact(bool compact)
{
    write("Compact", compact);
    reload();
}

void Settings::setItemCheckBoxes(bool show)
{
    write("ItemCheckBoxes", show);
    reload();
}

}
