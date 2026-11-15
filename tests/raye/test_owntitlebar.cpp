/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// When Files draws its own title row: dolphinrc says so, or the Raye
// decoration hides its title bar for Files through a rayerc exception.

#include "titlebar/titlebar.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QStandardPaths>
#include <QTabBar>
#include <QTest>

using Raye::Title::TitleBar;

class OwnTitleBarTest : public QObject
{
    Q_OBJECT

private:
    void writeDecoration(const QString &library)
    {
        KSharedConfig::Ptr kwin = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
        kwin->group(QStringLiteral("org.kde.kdecoration2")).writeEntry("library", library);
        kwin->sync();
    }

    void writeException(int index, const QString &pattern, bool hide, bool enabled = true)
    {
        KSharedConfig::Ptr rc = KSharedConfig::openConfig(QStringLiteral("rayerc"));
        KConfigGroup g = rc->group(QStringLiteral("Windeco Exception %1").arg(index));
        g.writeEntry("Enabled", enabled);
        g.writeEntry("ExceptionPattern", pattern);
        g.writeEntry("ExceptionType", 0);
        g.writeEntry("HideTitleBar", hide);
        g.writeEntry("Mask", 0);
        rc->sync();
    }

    void clear()
    {
        for (const char *name : {"kwinrc", "rayerc"}) {
            KSharedConfig::Ptr c = KSharedConfig::openConfig(QString::fromLatin1(name));
            for (const QString &g : c->groupList()) {
                c->deleteGroup(g);
            }
            c->sync();
        }
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        clear();
    }

    void settingWins()
    {
        QVERIFY(TitleBar::ownTitleBarFor(QStringLiteral("true"), false));
        QVERIFY(!TitleBar::ownTitleBarFor(QStringLiteral("false"), true));
        QVERIFY(TitleBar::ownTitleBarFor(QString(), true));
        QVERIFY(!TitleBar::ownTitleBarFor(QString(), false));
    }

    void noExceptionNoOwnRow()
    {
        writeDecoration(QStringLiteral("org.raye.decoration"));
        QVERIFY(!TitleBar::decorationHidesTitleBar());
    }

    void exceptionHidesTitle()
    {
        writeDecoration(QStringLiteral("org.raye.decoration"));
        writeException(0, QStringLiteral("org\\.kde\\.dolphin"), true);
        QVERIFY(TitleBar::decorationHidesTitleBar());
        QVERIFY(!TitleBar::decorationHidesTitleBar(QStringLiteral("konsole org.kde.konsole")));
    }

    void otherDecorationIgnoresExceptions()
    {
        writeDecoration(QStringLiteral("org.kde.breeze"));
        writeException(0, QStringLiteral("org\\.kde\\.dolphin"), true);
        QVERIFY(!TitleBar::decorationHidesTitleBar());
    }

    void firstMatchDecides()
    {
        writeDecoration(QStringLiteral("org.raye.decoration"));
        writeException(0, QStringLiteral("dolphin"), false);
        writeException(1, QStringLiteral("org\\.kde\\.dolphin"), true);
        QVERIFY(!TitleBar::decorationHidesTitleBar());
    }

    void disabledExceptionSkipped()
    {
        writeDecoration(QStringLiteral("org.raye.decoration"));
        writeException(0, QStringLiteral("dolphin"), false, false);
        writeException(1, QStringLiteral("org\\.kde\\.dolphin"), true);
        QVERIFY(TitleBar::decorationHidesTitleBar());
    }
};

QTEST_GUILESS_MAIN(OwnTitleBarTest)

#include "test_owntitlebar.moc"
