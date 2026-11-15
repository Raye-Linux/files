/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// What a new Files window opens: Home by default, This PC when chosen.

#include "global.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QStandardPaths>
#include <QTest>

class StartPageTest : public QObject
{
    Q_OBJECT

private:
    static void setStartPage(const QString &page)
    {
        KSharedConfig::Ptr rc = KSharedConfig::openConfig(QStringLiteral("dolphinrc"));
        KConfigGroup g = rc->group(QStringLiteral("Raye"));
        if (page.isEmpty()) {
            g.deleteEntry("StartPage");
        } else {
            g.writeEntry("StartPage", page);
        }
        rc->sync();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void homeByDefault()
    {
        setStartPage(QString());
        QCOMPARE(Dolphin::homeUrl(), QUrl(QStringLiteral("raye:home")));
    }

    void thisPc()
    {
        setStartPage(QStringLiteral("thispc"));
        QCOMPARE(Dolphin::homeUrl(), QUrl(QStringLiteral("raye:thispc")));
    }

    void anythingElseIsTheHomeFolder()
    {
        setStartPage(QStringLiteral("folder"));
        QVERIFY(Dolphin::homeUrl().isLocalFile());
        setStartPage(QString());
    }
};

QTEST_GUILESS_MAIN(StartPageTest)

#include "test_startpage.moc"
