/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The QML pages of a window (Home in every tab, This PC, the dialogs) share
// one engine, and it goes with the last of them.

#include "raye/jobs/quickhost.h"

#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTest>

class RayeQuickHostTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pagesShareOneEngine();
    void engineGoesWithTheLastPage();
};

void RayeQuickHostTest::pagesShareOneEngine()
{
    QQuickWidget *home = Raye::Jobs::quickPage(QStringLiteral("HomePage"));
    QQuickWidget *errors = Raye::Jobs::quickPage(QStringLiteral("ErrorPage"));
    QVERIFY(home->rootObject());
    QVERIFY(errors->rootObject());
    QCOMPARE(home->engine(), errors->engine());
    delete home;
    delete errors;
}

void RayeQuickHostTest::engineGoesWithTheLastPage()
{
    QQuickWidget *first = Raye::Jobs::quickPage(QStringLiteral("HomePage"));
    QQuickWidget *second = Raye::Jobs::quickPage(QStringLiteral("HomePage"));
    QPointer<QQmlEngine> engine = first->engine();
    delete first;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(engine);
    QVERIFY(second->rootObject());
    delete second;
    QTRY_VERIFY(!engine);

    // and the next page has an engine of its own again
    QQuickWidget *again = Raye::Jobs::quickPage(QStringLiteral("HomePage"));
    QVERIFY(again->rootObject());
    delete again;
}

QTEST_MAIN(RayeQuickHostTest)

#include "test_quickhost.moc"
