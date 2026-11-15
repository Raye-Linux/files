/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// View modes, per-place defaults, Windows date groups and the [Raye] view
// keys (explorer spec 3.6, 3.18).

#include "viewdefaults/viewdefaults.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDir>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include <clocale>

using namespace Raye::ViewDefaults;

Q_DECLARE_METATYPE(Raye::ViewDefaults::DateGroup)

class ViewDefaultsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setLanguages({QStringLiteral("en")});
    }

    void modes()
    {
        QCOMPARE(spec(Mode::ExtraLargeIcons).iconSize, 256);
        QCOMPARE(spec(Mode::LargeIcons).iconSize, 96);
        QCOMPARE(spec(Mode::MediumIcons).iconSize, 48);
        QCOMPARE(spec(Mode::SmallIcons).iconSize, 16);
        QCOMPARE(spec(Mode::Details).layout, Layout::Details);
        QCOMPARE(spec(Mode::Details).rowHeight, 32);
        QCOMPARE(spec(Mode::Details, true).rowHeight, 24);
        QCOMPARE(spec(Mode::List).layout, Layout::Compact);
        QCOMPARE(spec(Mode::List, true).rowHeight, 24);
        QCOMPARE(spec(Mode::Tiles).cellWidth, 272);
        QCOMPARE(spec(Mode::Tiles).iconSize, 48);
        QCOMPARE(spec(Mode::Tiles).layout, Layout::Compact);
        QCOMPARE(spec(Mode::Tiles).rowHeight, 64);
        QVERIFY(flowsInRows(Mode::Tiles));
        QVERIFY(flowsInRows(Mode::SmallIcons));
        QVERIFY(!flowsInRows(Mode::List));
        QCOMPARE(spec(Mode::Content).rowHeight, 72);
        // Small icons flow in rows (fixed cells), List flows in columns.
        QVERIFY(spec(Mode::SmallIcons).cellWidth > 0);
        QCOMPARE(spec(Mode::List).cellWidth, 0);
    }

    void ids()
    {
        for (int i = 0; i < ModeCount; ++i) {
            Mode back = Mode::Details;
            QVERIFY(modeFromId(modeId(Mode(i)), &back));
            QCOMPARE(int(back), i);
        }
        QVERIFY(!modeFromId(QStringLiteral("huge"), nullptr));
        QCOMPARE(modeId(Mode::Details), QStringLiteral("details"));
    }

    void nearestMode()
    {
        QCOMPARE(modeFor(Layout::Icons, 256), Mode::ExtraLargeIcons);
        QCOMPARE(modeFor(Layout::Icons, 128), Mode::LargeIcons);
        QCOMPARE(modeFor(Layout::Icons, 96), Mode::LargeIcons);
        QCOMPARE(modeFor(Layout::Icons, 64), Mode::MediumIcons);
        QCOMPARE(modeFor(Layout::Icons, 48), Mode::MediumIcons);
        QCOMPARE(modeFor(Layout::Icons, 22), Mode::SmallIcons);
        QCOMPARE(modeFor(Layout::Compact, 16), Mode::List);
        QCOMPARE(modeFor(Layout::Compact, 48), Mode::Tiles);
        QCOMPARE(modeFor(Layout::Details, 16), Mode::Details);
        QCOMPARE(modeFor(Layout::Details, 48), Mode::Content);
    }

    void stepping()
    {
        QCOMPARE(step(Mode::MediumIcons, -1), Mode::LargeIcons);
        QCOMPARE(step(Mode::MediumIcons, 1), Mode::SmallIcons);
        QCOMPARE(step(Mode::ExtraLargeIcons, -3), Mode::ExtraLargeIcons);
        QCOMPARE(step(Mode::Content, 2), Mode::Content);
    }

    void places()
    {
        Places p;
        p.pictures = QStringLiteral("/home/ana/Pictures");
        p.videos = QStringLiteral("/home/ana/Videos");
        p.downloads = QStringLiteral("/home/ana/Downloads");
        auto at = [&](const QString &path) {
            return forUrl(QUrl::fromLocalFile(path), p);
        };
        QCOMPARE(at(QStringLiteral("/home/ana/Documents")).mode, Mode::Details);
        QCOMPARE(at(QStringLiteral("/home/ana/Pictures")).mode, Mode::MediumIcons);
        QCOMPARE(at(QStringLiteral("/home/ana/Pictures/Trip/")).mode, Mode::MediumIcons);
        QCOMPARE(at(QStringLiteral("/home/ana/Videos")).mode, Mode::MediumIcons);
        QCOMPARE(at(QStringLiteral("/home/ana/PicturesOld")).mode, Mode::Details);
        const Defaults dl = at(QStringLiteral("/home/ana/Downloads"));
        QCOMPARE(dl.mode, Mode::Details);
        QCOMPARE(dl.groupRole, QByteArrayLiteral("modificationtime"));
        QCOMPARE(dl.sortOrder, Qt::DescendingOrder);
        QVERIFY(at(QStringLiteral("/home/ana/Downloads/project")).groupRole.isEmpty());
        QCOMPARE(forUrl(QUrl(QStringLiteral("raye:gallery")), p).mode, Mode::MediumIcons);
        QCOMPARE(forUrl(QUrl(QStringLiteral("smb://nas/share")), p).mode, Mode::Details);
        // An XDG folder set to the home itself does not turn the home into Pictures.
        p.pictures = QDir::homePath();
        QCOMPARE(forUrl(QUrl::fromLocalFile(QDir::homePath() + QStringLiteral("/Documents")), p).mode, Mode::Details);
    }

    void dates_data()
    {
        QTest::addColumn<QDate>("date");
        QTest::addColumn<int>("firstDay");
        QTest::addColumn<DateGroup>("group");
        // Today is Wednesday 2026-10-28.
        const int mon = Qt::Monday;
        const int sun = Qt::Sunday;
        QTest::newRow("today") << QDate(2026, 10, 28) << mon << DateGroup::Today;
        QTest::newRow("yesterday") << QDate(2026, 10, 27) << mon << DateGroup::Yesterday;
        QTest::newRow("monday") << QDate(2026, 10, 26) << mon << DateGroup::EarlierThisWeek;
        QTest::newRow("sunday, week from monday") << QDate(2026, 10, 25) << mon << DateGroup::LastWeek;
        QTest::newRow("sunday, week from sunday") << QDate(2026, 10, 25) << sun << DateGroup::EarlierThisWeek;
        QTest::newRow("last monday") << QDate(2026, 10, 19) << mon << DateGroup::LastWeek;
        QTest::newRow("this month") << QDate(2026, 10, 5) << mon << DateGroup::EarlierThisMonth;
        QTest::newRow("last month") << QDate(2026, 9, 2) << mon << DateGroup::LastMonth;
        QTest::newRow("this year") << QDate(2026, 3, 1) << mon << DateGroup::EarlierThisYear;
        QTest::newRow("long ago") << QDate(2019, 3, 1) << mon << DateGroup::LongAgo;
        QTest::newRow("invalid") << QDate() << mon << DateGroup::LongAgo;
        QTest::newRow("tomorrow") << QDate(2026, 10, 29) << mon << DateGroup::Tomorrow;
        QTest::newRow("saturday") << QDate(2026, 10, 31) << mon << DateGroup::LaterThisWeek;
        QTest::newRow("next week") << QDate(2026, 11, 3) << mon << DateGroup::NextWeek;
        QTest::newRow("next month") << QDate(2026, 11, 20) << mon << DateGroup::NextMonth;
        QTest::newRow("this year, later") << QDate(2026, 12, 20) << mon << DateGroup::LaterThisYear;
        QTest::newRow("far") << QDate(2031, 1, 1) << mon << DateGroup::LongFromNow;
    }

    void dates()
    {
        QFETCH(QDate, date);
        QFETCH(int, firstDay);
        QFETCH(DateGroup, group);
        QCOMPARE(dateGroup(date, QDate(2026, 10, 28), Qt::DayOfWeek(firstDay)), group);
    }

    // Yesterday wins over the week: on a Monday, Sunday is still "Yesterday".
    void yesterdayAcrossWeeks()
    {
        QCOMPARE(dateGroup(QDate(2026, 10, 25), QDate(2026, 10, 26), Qt::Monday), DateGroup::Yesterday);
        // And on the 1st, the last day of the month before.
        QCOMPARE(dateGroup(QDate(2026, 9, 30), QDate(2026, 10, 1), Qt::Monday), DateGroup::Yesterday);
        // January: December is last month, not this year.
        QCOMPARE(dateGroup(QDate(2025, 12, 1), QDate(2026, 1, 20), Qt::Monday), DateGroup::LastMonth);
    }

    void dateNames()
    {
        QCOMPARE(dateGroupName(DateGroup::Today), QStringLiteral("Today"));
        QCOMPARE(dateGroupName(DateGroup::EarlierThisWeek), QStringLiteral("Earlier this week"));
        QCOMPARE(dateGroupName(DateGroup::LongAgo), QStringLiteral("A long time ago"));
        if (!std::setlocale(LC_MESSAGES, "fr_FR.UTF-8")) {
            if (qEnvironmentVariableIsSet("RAYE_TESTS_STRICT")) {
                QFAIL("no fr_FR locale: the French strings are not checked");
            }
            QSKIP("no fr_FR locale here");
        }
        KLocalizedString::addDomainLocaleDir("raye-files", QStringLiteral(RAYE_FILES_BINARY_DIR "/locale"));
        KLocalizedString::setLanguages({QStringLiteral("fr")});
        const QString today = dateGroupName(DateGroup::Today);
        const QString lastWeek = dateGroupName(DateGroup::LastWeek);
        KLocalizedString::setLanguages({QStringLiteral("en")});
        std::setlocale(LC_MESSAGES, "C.UTF-8");
        QCOMPARE(today, QStringLiteral("Aujourd'hui"));
        QCOMPARE(lastWeek, QStringLiteral("La semaine dernière"));
    }

    void settings()
    {
        KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("dolphinrc"));
        config->group(QStringLiteral("Raye")).deleteGroup();
        config->sync();
        Settings *s = Settings::instance();
        s->reload();
        QCOMPARE(s->showExtensions(), false);
        QCOMPARE(s->compact(), false);
        QSignalSpy spy(s, &Settings::showExtensionsChanged);
        s->setShowExtensions(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QCOMPARE(s->showExtensions(), true);
        QCOMPARE(config->group(QStringLiteral("Raye")).readEntry("ShowExtensions", false), true);
        // Another program writes the file: reload picks it up.
        KConfig other(QStringLiteral("dolphinrc"));
        other.group(QStringLiteral("Raye")).writeEntry("ShowExtensions", false);
        other.group(QStringLiteral("Raye")).writeEntry("Compact", true);
        other.sync();
        s->reload();
        QCOMPARE(s->showExtensions(), false);
        QCOMPARE(s->compact(), true);
        QCOMPARE(spy.count(), 2);
        s->setCompact(false);
        QCOMPARE(s->compact(), false);
    }
};

QTEST_GUILESS_MAIN(ViewDefaultsTest)

#include "test_viewdefaults.moc"
