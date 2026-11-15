/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The title row (explorer spec 1.1, 3.2): tab widths (240 at most, 112 at
// least, then the strip scrolls), the row layout (tabs, "+", drag space,
// 46 x 40 caption buttons), the close buttons, the tab menu and what each
// of its items asks for, middle click, the grow and shrink of tabs, Snap
// layouts after 400 ms on Maximise, and OwnTitleBar=false.

#include "dolphintabbar.h"
#include "titlebar/tabbar.h"
#include "titlebar/tabwidth.h"
#include "titlebar/titlebar.h"
#include "ui/motion.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QtMath>

#include <memory>

using namespace Raye::Title;

class RayeTitleBarTest : public QObject
{
    Q_OBJECT

public:
    // A home of its own: test mode writes under ~/.qttest, which other test
    // programs running at the same time share otherwise.
    static void initMain()
    {
        static QTemporaryDir home;
        qputenv("HOME", home.path().toLocal8Bit());
        qunsetenv("XDG_CONFIG_HOME");
        qunsetenv("XDG_DATA_HOME");
        qunsetenv("XDG_CACHE_HOME");
        QStandardPaths::setTestModeEnabled(true);
    }

private Q_SLOTS:
    void initTestCase();
    void preferredWidth_data();
    void preferredWidth();
    void stripLayout_data();
    void stripLayout();
    void stripShrinksWidestFirst();
    void textWidthRoundTrip();
    void rowLayout();
    void rowLayoutNarrow();
    void tabsReturnAfterBeingMoved();
    void tabHeightAndFont();
    void closeButtonsFollowSelectionAndHover();
    void closeButtonShrinksThenCloses();
    void closeRefusedKeepsTab();
    void middleClickCloses();
    void tabMenuItems();
    void tabMenuEnabledStates();
    void tabMenuCloseOthersAndRight();
    void tabMenuDuplicateAndMove();
    void dolphinTabBarRoutes();
    void progressAndDropTarget();
    void doubleClickMaximises();
    void snapLayoutsAfterDelay();
    void snapLayoutsCancelledOnLeave();
    void ownTitleBarOff();
    void ownTitleBarReadFromConfig();
};

void RayeTitleBarTest::initTestCase()
{
    Raye::Ui::Motion::setSpeedFactor(0);
}

void RayeTitleBarTest::preferredWidth_data()
{
    QTest::addColumn<int>("text");
    QTest::addColumn<int>("width");
    // chrome = 6 + 12 + 16 + 8 + 8 + 24 + 4 + 6 = 84
    QTest::newRow("empty") << 0 << Metrics::tabMinWidth;
    QTest::newRow("short") << 20 << Metrics::tabMinWidth;
    QTest::newRow("at min") << 28 << 112;
    QTest::newRow("medium") << 70 << 154;
    QTest::newRow("at max") << 156 << 240;
    QTest::newRow("long") << 400 << Metrics::tabMaxWidth;
    QTest::newRow("negative") << -5 << Metrics::tabMinWidth;
}

void RayeTitleBarTest::preferredWidth()
{
    QFETCH(int, text);
    QFETCH(int, width);
    QCOMPARE(preferredTabWidth(text), width);
}

void RayeTitleBarTest::stripLayout_data()
{
    QTest::addColumn<QList<int>>("texts");
    QTest::addColumn<int>("available");
    QTest::addColumn<QList<int>>("widths");
    QTest::addColumn<bool>("scrolls");

    QTest::newRow("fits") << QList<int>{70, 20, 400} << 1000 << QList<int>{154, 112, 240} << false;
    QTest::newRow("unbounded") << QList<int>{400, 400} << -1 << QList<int>{240, 240} << false;
    QTest::newRow("exact fit") << QList<int>{70, 70} << 308 << QList<int>{154, 154} << false;
    QTest::newRow("shrink evenly") << QList<int>{400, 400, 400} << 600 << QList<int>{200, 200, 200} << false;
    QTest::newRow("shrink, rounding to the left") << QList<int>{400, 400, 400} << 601 << QList<int>{201, 200, 200} << false;
    QTest::newRow("narrow tabs keep their width") << QList<int>{20, 400, 400} << 500 << QList<int>{112, 194, 194} << false;
    QTest::newRow("at minimum, no scroll") << QList<int>{400, 400} << 224 << QList<int>{112, 112} << false;
    QTest::newRow("below minimum scrolls") << QList<int>{400, 400, 400} << 300 << QList<int>{112, 112, 112} << true;
    QTest::newRow("none") << QList<int>{} << 300 << QList<int>{} << false;
}

void RayeTitleBarTest::stripLayout()
{
    QFETCH(QList<int>, texts);
    QFETCH(int, available);
    QFETCH(QList<int>, widths);
    QFETCH(bool, scrolls);

    const TabStripLayout layout = layoutTabs(texts, available);
    QCOMPARE(layout.widths, widths);
    QCOMPARE(layout.scrolls, scrolls);
    if (!scrolls && available >= 0) {
        QVERIFY(layout.total() <= available);
    }
    for (int w : layout.widths) {
        QVERIFY(w >= Metrics::tabMinWidth);
        QVERIFY(w <= Metrics::tabMaxWidth);
    }
}

void RayeTitleBarTest::stripShrinksWidestFirst()
{
    // 240 + 154 + 112 = 506 in 450: only the widest gives way first.
    const TabStripLayout a = layoutTabs({400, 70, 20}, 450);
    QCOMPARE(a.widths, (QList<int>{184, 154, 112}));
    // Tighter: the two wide ones meet and shrink together.
    const TabStripLayout b = layoutTabs({400, 70, 20}, 380);
    QCOMPARE(b.widths, (QList<int>{134, 134, 112}));
    QCOMPARE(b.total(), 380);
}

void RayeTitleBarTest::textWidthRoundTrip()
{
    for (int w : {112, 150, 240}) {
        QCOMPARE(preferredTabWidth(textWidthFor(w)), w);
    }
    QCOMPARE(textWidthFor(10), 0);
}

static TabBar *barWithTabs(const QStringList &names, QWidget *parent = nullptr)
{
    auto *bar = new TabBar(parent);
    for (const QString &n : names) {
        bar->addTab(n);
    }
    return bar;
}

void RayeTitleBarTest::rowLayout()
{
    TitleBar row;
    row.setOwnTitleBar(true);
    row.resize(1280, Metrics::rowHeight);
    TabBar *bar = barWithTabs({QStringLiteral("Home"), QStringLiteral("Documents")});
    row.setTabBar(bar);
    row.show();
    QVERIFY(QTest::qWaitForWindowExposed(&row));

    QCOMPARE(row.height(), 40);
    QCOMPARE(bar->parentWidget(), &row);
    QCOMPARE(bar->geometry().left(), 8);
    QCOMPARE(bar->geometry().top(), 8); // 32 tall on the bottom of 40
    QCOMPARE(bar->height(), 32);
    QCOMPARE(bar->width(), bar->tabSizeHint(0).width() + bar->tabSizeHint(1).width());

    QAbstractButton *add = row.button(QStringLiteral("newtab"));
    QVERIFY(add);
    QCOMPARE(add->size(), QSize(32, 32));
    QCOMPARE(add->geometry().left(), bar->geometry().right() + 1 + Metrics::newTabGap);

    QAbstractButton *min = row.button(QStringLiteral("minimize"));
    QAbstractButton *max = row.button(QStringLiteral("maximize"));
    QAbstractButton *close = row.button(QStringLiteral("close"));
    QVERIFY(min && max && close);
    QCOMPARE(close->geometry(), QRect(1280 - 46, 0, 46, 40));
    QCOMPARE(max->geometry(), QRect(1280 - 92, 0, 46, 40));
    QCOMPARE(min->geometry(), QRect(1280 - 138, 0, 46, 40));
    QVERIFY(close->isVisible());

    // A new tab moves the "+" along.
    const int before = add->geometry().left();
    bar->addTab(QStringLiteral("Downloads"));
    QTRY_VERIFY(add->geometry().left() > before);
    QCOMPARE(add->geometry().left(), bar->geometry().right() + 1 + Metrics::newTabGap);

    QSignalSpy newTab(&row, &TitleBar::newTabRequested);
    QTest::mouseClick(add, Qt::LeftButton);
    QCOMPARE(newTab.count(), 1);
}

void RayeTitleBarTest::rowLayoutNarrow()
{
    TitleBar row;
    row.setOwnTitleBar(true);
    row.resize(640, Metrics::rowHeight);
    QStringList names;
    for (int i = 0; i < 12; ++i) {
        names << QStringLiteral("A rather long folder name %1").arg(i);
    }
    TabBar *bar = barWithTabs(names);
    row.setTabBar(bar);
    row.show();
    QVERIFY(QTest::qWaitForWindowExposed(&row));

    // Room: 640 - 8 - 4 - 32 - 46 (drag space) - 138 (captions) = 412.
    QCOMPARE(bar->availableWidth(), 412);
    QCOMPARE(bar->width(), 412);
    for (int i = 0; i < bar->count(); ++i) {
        QCOMPARE(bar->tabSizeHint(i).width(), Metrics::tabMinWidth);
    }
    // The drag space stays free.
    QAbstractButton *add = row.button(QStringLiteral("newtab"));
    QVERIFY(row.button(QStringLiteral("minimize"))->geometry().left() - add->geometry().right() - 1 >= Metrics::minDragSpace);
}

void RayeTitleBarTest::tabHeightAndFont()
{
    TabBar *bar = barWithTabs({QStringLiteral("Documents")});
    QCOMPARE(bar->tabSizeHint(0).height(), 32);
    QCOMPARE(bar->sizeHint().height(), 32);
    QCOMPARE(bar->iconSize(), QSize(16, 16));
    QVERIFY(!bar->drawBase());
    QVERIFY(bar->isMovable());
    QVERIFY(!bar->tabsClosable()); // Raye's own buttons
    QVERIFY(bar->tabButton(0, QTabBar::RightSide));
    // "&&" is one "&" on screen: the width follows what is shown.
    bar->addTab(QStringLiteral("Tom && Ann"));
    QCOMPARE(bar->labelWidth(1), qCeil(QFontMetricsF(bar->font()).horizontalAdvance(QStringLiteral("Tom & Ann"))));
    delete bar;
}

void RayeTitleBarTest::closeButtonsFollowSelectionAndHover()
{
    TabBar *bar = barWithTabs({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});
    bar->resize(600, 32);
    bar->show();
    QVERIFY(QTest::qWaitForWindowExposed(bar));
    bar->setCurrentIndex(1);
    QVERIFY(!bar->isCloseButtonShown(0));
    QVERIFY(bar->isCloseButtonShown(1));
    QVERIFY(!bar->isCloseButtonShown(2));
    QVERIFY(bar->tabButton(0, QTabBar::RightSide)->testAttribute(Qt::WA_TransparentForMouseEvents));
    QVERIFY(!bar->tabButton(1, QTabBar::RightSide)->testAttribute(Qt::WA_TransparentForMouseEvents));

    QTest::mouseMove(bar, bar->tabRect(2).center());
    QTRY_COMPARE(bar->hoveredIndex(), 2);
    QVERIFY(bar->isCloseButtonShown(2));
    QVERIFY(bar->isCloseButtonShown(1));

    // The button sits on the right of its tab, 24 x 24, inside the body.
    const QRect tab = bar->tabRect(2);
    const QRect close = bar->tabButton(2, QTabBar::RightSide)->geometry();
    QCOMPARE(close.size(), QSize(24, 24));
    QCOMPARE(close.right(), tab.right() - Metrics::tabFoot - Metrics::closeMargin);
    QCOMPARE(close.center().y(), tab.center().y());
    delete bar;
}

void RayeTitleBarTest::closeButtonShrinksThenCloses()
{
    TabBar *bar = barWithTabs({QStringLiteral("A"), QStringLiteral("B")});
    bar->show();
    QVERIFY(QTest::qWaitForWindowExposed(bar));
    QObject::connect(bar, &QTabBar::tabCloseRequested, bar, &QTabBar::removeTab);
    QSignalSpy closing(bar, &QTabBar::tabCloseRequested);
    bar->setCurrentIndex(1);
    QTest::mouseClick(bar->tabButton(1, QTabBar::RightSide), Qt::LeftButton);
    QCOMPARE(closing.count(), 1);
    QCOMPARE(closing.first().first().toInt(), 1);
    QCOMPARE(bar->count(), 1);
    QCOMPARE(bar->tabText(0), QStringLiteral("A"));
    delete bar;
}

void RayeTitleBarTest::closeRefusedKeepsTab()
{
    TabBar *bar = barWithTabs({QStringLiteral("Only")});
    bar->show();
    QVERIFY(QTest::qWaitForWindowExposed(bar));
    QSignalSpy closing(bar, &QTabBar::tabCloseRequested);
    bar->requestClose(0); // nobody removes it
    QCOMPARE(closing.count(), 1);
    QCOMPARE(bar->count(), 1);
    QCOMPARE(bar->tabScale(0), 1.0);
    QVERIFY(bar->isCloseButtonShown(0));
    delete bar;
}

void RayeTitleBarTest::middleClickCloses()
{
    TabBar *bar = barWithTabs({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});
    bar->show();
    QVERIFY(QTest::qWaitForWindowExposed(bar));
    QSignalSpy closing(bar, &QTabBar::tabCloseRequested);
    QTest::mouseClick(bar, Qt::MiddleButton, Qt::NoModifier, bar->tabRect(2).center());
    QCOMPARE(closing.count(), 1);
    QCOMPARE(closing.first().first().toInt(), 2);
    delete bar;
}

static QStringList names(QMenu *menu)
{
    QStringList out;
    for (QAction *a : menu->actions()) {
        out << (a->isSeparator() ? QStringLiteral("-") : a->objectName());
    }
    return out;
}

void RayeTitleBarTest::tabMenuItems()
{
    TabBar *bar = barWithTabs({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});
    std::unique_ptr<QMenu> menu(bar->createTabMenu(1, nullptr));
    QCOMPARE(names(menu.get()),
             (QStringList{QStringLiteral("close_tab"), QStringLiteral("close_other_tabs"), QStringLiteral("close_tabs_right"), QStringLiteral("-"),
                          QStringLiteral("duplicate_tab"), QStringLiteral("move_tab_new_window")}));
    QCOMPARE(menu->actions().at(0)->text(), QStringLiteral("Close tab"));
    QCOMPARE(menu->actions().at(0)->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_W));
    QCOMPARE(menu->actions().at(0)->shortcutContext(), Qt::WidgetShortcut);
    QCOMPARE(menu->actions().at(5)->text(), QStringLiteral("Move tab to new window"));
    delete bar;
}

void RayeTitleBarTest::tabMenuEnabledStates()
{
    TabBar *one = barWithTabs({QStringLiteral("A")});
    std::unique_ptr<QMenu> m1(one->createTabMenu(0, nullptr));
    QVERIFY(m1->actions().at(0)->isEnabled());
    QVERIFY(!m1->actions().at(1)->isEnabled()); // no other tabs
    QVERIFY(!m1->actions().at(2)->isEnabled()); // nothing to the right
    QVERIFY(m1->actions().at(4)->isEnabled());
    QVERIFY(!m1->actions().at(5)->isEnabled()); // already alone
    delete one;

    TabBar *three = barWithTabs({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});
    std::unique_ptr<QMenu> last(three->createTabMenu(2, nullptr));
    QVERIFY(last->actions().at(1)->isEnabled());
    QVERIFY(!last->actions().at(2)->isEnabled());
    std::unique_ptr<QMenu> first(three->createTabMenu(0, nullptr));
    QVERIFY(first->actions().at(2)->isEnabled());
    QVERIFY(first->actions().at(5)->isEnabled());
    delete three;
}

void RayeTitleBarTest::tabMenuCloseOthersAndRight()
{
    TabBar *bar = barWithTabs({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D")});
    QObject::connect(bar, &QTabBar::tabCloseRequested, bar, &QTabBar::removeTab);
    std::unique_ptr<QMenu> right(bar->createTabMenu(1, nullptr));
    right->actions().at(2)->trigger();
    QCOMPARE(bar->count(), 2);
    QCOMPARE(bar->tabText(0), QStringLiteral("A"));
    QCOMPARE(bar->tabText(1), QStringLiteral("B"));

    bar->addTab(QStringLiteral("E"));
    std::unique_ptr<QMenu> others(bar->createTabMenu(1, nullptr));
    others->actions().at(1)->trigger();
    QCOMPARE(bar->count(), 1);
    QCOMPARE(bar->tabText(0), QStringLiteral("B"));

    // A menu that outlives a tab change still closes the tab it was opened on.
    bar->insertTab(0, QStringLiteral("Z"));
    std::unique_ptr<QMenu> stale(bar->createTabMenu(1, nullptr)); // "B"
    bar->insertTab(0, QStringLiteral("Y"));
    stale->actions().at(0)->trigger();
    QCOMPARE(bar->count(), 2);
    QCOMPARE(bar->tabText(0), QStringLiteral("Y"));
    QCOMPARE(bar->tabText(1), QStringLiteral("Z"));
    delete bar;
}

void RayeTitleBarTest::tabMenuDuplicateAndMove()
{
    TabBar *bar = barWithTabs({QStringLiteral("A"), QStringLiteral("B")});
    QSignalSpy duplicate(bar, &TabBar::duplicateTabRequested);
    QSignalSpy move(bar, &TabBar::moveTabToNewWindowRequested);
    std::unique_ptr<QMenu> menu(bar->createTabMenu(1, nullptr));
    menu->actions().at(4)->trigger();
    menu->actions().at(5)->trigger();
    QCOMPARE(duplicate.count(), 1);
    QCOMPARE(duplicate.first().first().toInt(), 1);
    QCOMPARE(move.count(), 1);
    QCOMPARE(move.first().first().toInt(), 1);
    delete bar;
}

void RayeTitleBarTest::dolphinTabBarRoutes()
{
    // Dolphin's tab bar is a Raye tab bar: Duplicate opens a new tab on the
    // same folder, Move to new window is Dolphin's detach.
    DolphinTabBar bar(nullptr);
    bar.addTab(QStringLiteral("A"));
    bar.addTab(QStringLiteral("B"));
    QSignalSpy newTab(&bar, &DolphinTabBar::openNewActivatedTab);
    QSignalSpy detach(&bar, &DolphinTabBar::tabDetachRequested);
    std::unique_ptr<QMenu> menu(bar.createTabMenu(0, nullptr));
    menu->actions().at(4)->trigger();
    menu->actions().at(5)->trigger();
    QCOMPARE(newTab.count(), 1);
    QCOMPARE(newTab.first().first().toInt(), 0);
    QCOMPARE(detach.count(), 1);
    QCOMPARE(detach.first().first().toInt(), 0);
    QVERIFY(bar.acceptDrops());
    QCOMPARE(bar.selectionBehaviorOnRemove(), QTabBar::SelectPreviousTab);
}

void RayeTitleBarTest::progressAndDropTarget()
{
    TabBar *bar = barWithTabs({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});
    QCOMPARE(bar->tabProgress(0), -1.0);
    bar->setTabProgress(1, 0.5);
    QCOMPARE(bar->tabProgress(1), 0.5);
    bar->setTabProgress(1, 3.0);
    QCOMPARE(bar->tabProgress(1), 1.0);
    bar->setTabProgress(9, 0.5); // ignored
    // The ring follows its tab when tabs move or one before it goes.
    bar->moveTab(1, 2);
    QCOMPARE(bar->tabProgress(2), 1.0);
    bar->removeTab(0);
    QCOMPARE(bar->tabProgress(1), 1.0);
    bar->setTabProgress(1, -1);
    QCOMPARE(bar->tabProgress(1), -1.0);

    bar->setDropTargetIndex(1);
    QCOMPARE(bar->dropTargetIndex(), 1);
    bar->setDropTargetIndex(7);
    QCOMPARE(bar->dropTargetIndex(), -1);
    bar->setDropTargetIndex(1);
    bar->removeTab(1);
    QCOMPARE(bar->dropTargetIndex(), -1);
    delete bar;
}

void RayeTitleBarTest::doubleClickMaximises()
{
    QWidget window;
    auto *row = new TitleBar(&window);
    row->setOwnTitleBar(true);
    TabBar *bar = barWithTabs({QStringLiteral("A")});
    row->setTabBar(bar);
    window.resize(900, 300);
    row->resize(900, 40);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(!window.isMaximized());
    QTest::mouseDClick(row, Qt::LeftButton, Qt::NoModifier, QPoint(700, 20));
    QTRY_VERIFY(window.windowState() & Qt::WindowMaximized);
    // A double click on a tab does the same.
    QSignalSpy spy(bar, &TabBar::titleAreaDoubleClicked);
    QTest::mouseDClick(bar, Qt::LeftButton, Qt::NoModifier, bar->tabRect(0).center());
    QCOMPARE(spy.count(), 1);
    QTRY_VERIFY(!(window.windowState() & Qt::WindowMaximized));
}

void RayeTitleBarTest::snapLayoutsAfterDelay()
{
    int calls = 0;
    TitleBar::setSnapLayoutsHandler([&calls] {
        ++calls;
    });
    TitleBar row;
    row.setOwnTitleBar(true);
    row.resize(900, 40);
    row.show();
    QVERIFY(QTest::qWaitForWindowExposed(&row));
    QAbstractButton *max = row.button(QStringLiteral("maximize"));
    QEnterEvent enter(QPointF(10, 10), QPointF(10, 10), max->mapToGlobal(QPointF(10, 10)));
    max->setAttribute(Qt::WA_UnderMouse, true);
    QApplication::sendEvent(max, &enter);
    QTest::qWait(Metrics::snapLayoutsDelayMs / 2);
    QCOMPARE(calls, 0);
    QTRY_COMPARE_WITH_TIMEOUT(calls, 1, 2000);
    TitleBar::setSnapLayoutsHandler({});
}

void RayeTitleBarTest::snapLayoutsCancelledOnLeave()
{
    int calls = 0;
    TitleBar::setSnapLayoutsHandler([&calls] {
        ++calls;
    });
    TitleBar row;
    row.setOwnTitleBar(true);
    row.resize(900, 40);
    row.show();
    QVERIFY(QTest::qWaitForWindowExposed(&row));
    QAbstractButton *max = row.button(QStringLiteral("maximize"));
    QEnterEvent enter(QPointF(10, 10), QPointF(10, 10), max->mapToGlobal(QPointF(10, 10)));
    max->setAttribute(Qt::WA_UnderMouse, true);
    QApplication::sendEvent(max, &enter);
    QTest::qWait(100);
    QEvent leave(QEvent::Leave);
    max->setAttribute(Qt::WA_UnderMouse, false);
    QApplication::sendEvent(max, &leave);
    QTest::qWait(Metrics::snapLayoutsDelayMs + 200);
    QCOMPARE(calls, 0);
    TitleBar::setSnapLayoutsHandler({});
}

void RayeTitleBarTest::ownTitleBarOff()
{
    QWidget window;
    auto *row = new TitleBar(&window);
    row->setOwnTitleBar(false);
    row->resize(900, 40);
    window.resize(900, 200);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(!row->button(QStringLiteral("close"))->isVisible());
    QVERIFY(!row->button(QStringLiteral("maximize"))->isVisible());
    QVERIFY(row->button(QStringLiteral("newtab"))->isVisible());
    // Under a normal title bar the row does not maximise.
    QTest::mouseDClick(row, Qt::LeftButton, Qt::NoModifier, QPoint(700, 20));
    QTest::qWait(50);
    QVERIFY(!(window.windowState() & Qt::WindowMaximized));
}

void RayeTitleBarTest::tabsReturnAfterBeingMoved()
{
    TitleBar row;
    row.setOwnTitleBar(true);
    row.resize(1280, Metrics::rowHeight);
    TabBar *bar = barWithTabs({QStringLiteral("Home"), QStringLiteral("Documents")});
    row.setTabBar(bar);
    row.show();
    QVERIFY(QTest::qWaitForWindowExposed(&row));
    const QRect placed = bar->geometry();
    // What QTabWidget does to the bar it still thinks it owns.
    bar->move(0, 0);
    QTRY_COMPARE(bar->geometry(), placed);
}

void RayeTitleBarTest::ownTitleBarReadFromConfig()
{
    KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("dolphinrc")), QStringLiteral("Raye"));
    group.deleteEntry("OwnTitleBar");
    QCOMPARE(TitleBar::readOwnTitleBar(), TitleBar::decorationHidesTitleBar());
    group.writeEntry("OwnTitleBar", false);
    QVERIFY(!TitleBar::readOwnTitleBar());
    TitleBar row;
    QVERIFY(!row.ownTitleBar());
    group.deleteEntry("OwnTitleBar");
    group.sync();
}

QTEST_MAIN(RayeTitleBarTest)

#include "test_titlebar.moc"
