/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The command bar (explorer spec 1.3, 3.4): the Windows order built from
// Files' actions, the enable rules, folding into "…" from the right, the
// Sort, View and See more menus, context sets (Recycle Bin, pictures,
// drives...), and the Share and Details buttons. A fake action collection
// stands in for Dolphin's.

#include "commandbar/commandbar.h"
#include "commandbar/commandids.h"
#include "commandbar/defaultsets.h"
#include "commandbar/fold.h"
#include "commandbar/rules.h"
#include "ui/commandbutton.h"
#include "ui/motion.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace Raye::Commands;

namespace
{
// Stands in for Dolphin's KActionCollection: actions by name, with KDE's
// own texts, so the test sees that the bar shows Raye's words instead.
class FakeActions
{
public:
    FakeActions()
    {
        for (const char *id : {"new_menu", "edit_cut", "edit_copy", "edit_paste", "renamefile", "movetotrash", "deletefile", "sort", "view_mode",
                               "edit_undo", "edit_redo", "edit_select_all", "invert_selection", "properties", "add_to_places", "options_configure",
                               "open_terminal", "ascending", "descending", "sort_by_text", "sort_by_modificationtime", "sort_by_type", "sort_by_size",
                               "sort_by_owner", "show_in_groups", "show_hidden_files", "raye_details_pane", "raye_preview_pane", "icons", "compact",
                               "details"}) {
            auto *a = new QAction(QString::fromLatin1("KDE ") + QString::fromLatin1(id), &owner);
            a->setObjectName(QString::fromLatin1(id));
            actions.insert(QString::fromLatin1(id), a);
        }
        action("movetotrash")->setText(QStringLiteral("Move to Trash"));
        action("edit_copy")->setShortcut(QKeySequence::Copy);
        for (const char *id : {"show_in_groups", "show_hidden_files", "raye_details_pane", "raye_preview_pane"}) {
            action(id)->setCheckable(true);
        }
        auto *sortGroup = new QActionGroup(&owner);
        for (const char *id : {"sort_by_text", "sort_by_modificationtime", "sort_by_type", "sort_by_size", "sort_by_owner"}) {
            action(id)->setCheckable(true);
            action(id)->setActionGroup(sortGroup);
        }
        action("sort_by_text")->setChecked(true);
        auto *orderGroup = new QActionGroup(&owner);
        for (const char *id : {"ascending", "descending"}) {
            action(id)->setCheckable(true);
            action(id)->setActionGroup(orderGroup);
        }
        action("ascending")->setChecked(true);
        newMenu = new QMenu;
        newMenu->addAction(QStringLiteral("Folder…"));
        action("new_menu")->setMenu(newMenu);
        auto *sortMenu = new QMenu;
        for (const char *id : {"sort_by_text", "sort_by_modificationtime", "sort_by_type", "sort_by_size", "sort_by_owner"}) {
            sortMenu->addAction(action(id));
        }
        action("sort")->setMenu(sortMenu);
        auto *modeMenu = new QMenu;
        auto *modeGroup = new QActionGroup(&owner);
        for (const char *id : {"icons", "compact", "details"}) {
            action(id)->setCheckable(true);
            action(id)->setActionGroup(modeGroup);
            modeMenu->addAction(action(id));
        }
        action("details")->setChecked(true);
        action("view_mode")->setMenu(modeMenu);
    }
    ~FakeActions()
    {
        delete action("new_menu")->menu();
        delete action("sort")->menu();
        delete action("view_mode")->menu();
    }

    QAction *action(const char *id) const
    {
        return actions.value(QString::fromLatin1(id));
    }
    CommandBar::ActionLookup lookup() const
    {
        return [this](const QString &id) {
            return actions.value(id);
        };
    }

    QObject owner;
    QHash<QString, QAction *> actions;
    QMenu *newMenu = nullptr;
};

CommandContext folderWith(int selected, const QString &mime = QStringLiteral("text/plain"))
{
    CommandContext c;
    c.url = QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents"));
    for (int i = 0; i < selected; ++i) {
        c.selection << QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents/f%1").arg(i));
        c.selectionMimeTypes << mime;
    }
    return c;
}

QStringList actionTexts(const QMenu *menu)
{
    QStringList out;
    for (QAction *a : menu->actions()) {
        if (!a->isSeparator()) {
            out << a->text();
        }
    }
    return out;
}

QAction *byText(const QMenu *menu, const QString &text)
{
    for (QAction *a : menu->actions()) {
        if (a->text() == text) {
            return a;
        }
    }
    return nullptr;
}
}

class RayeCommandBarTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    // pure rules
    void rules_data();
    void rules();
    void foldKeepsWhatFits();
    void foldGoesFromTheRight();
    void foldHonoursPriorities();
    void foldDropsEmptyGroups();

    // the bar
    void windowsOrder();
    void sizes();
    void rayeWordsNotKdes();
    void disabledWithoutSelection();
    void enabledWithSelection();
    void sourceActionStillDecides();
    void pasteFollowsTheClipboard();
    void readOnlyFolder();
    void buttonsTriggerTheirAction();
    void deleteFallsBackToDeleteFile();
    void narrowWindowsFoldFromTheRight();
    void foldedButtonsSitInSeeMore();
    void seeMoreMenu();
    void sortMenu();
    void groupBy();
    void viewMenu();
    void detailsToggle();
    void shareButton();
    void contextSetJoinsAndLeaves();
    void contextSetCanReplaceEditing();
    void contextSetWithAMenu();
    void unregisterSet();

    // default sets
    void picturesSet();
    void rotateNeedsAServiceMenu();
    void rotateActionsAreFound();
    void driveSet();

private:
    FakeActions *m_actions = nullptr;
    CommandBar *m_bar = nullptr;
};

void RayeCommandBarTest::initTestCase()
{
    Raye::Ui::Motion::setSpeedFactor(0);
}

void RayeCommandBarTest::init()
{
    QGuiApplication::clipboard()->clear();
    m_actions = new FakeActions;
    m_bar = new CommandBar;
    m_bar->setActionLookup(m_actions->lookup());
    m_bar->setContext(folderWith(0));
    m_bar->resize(1280, CommandBar::rowHeight);
    m_bar->show();
    QCoreApplication::processEvents();
}

void RayeCommandBarTest::cleanup()
{
    delete m_bar;
    delete m_actions;
}

void RayeCommandBarTest::rules_data()
{
    QTest::addColumn<int>("command");
    QTest::addColumn<int>("selected");
    QTest::addColumn<bool>("clipboard");
    QTest::addColumn<bool>("writable");
    QTest::addColumn<bool>("allowed");
    auto row = [](const char *name, Command c, int sel, bool clip, bool writable, bool allowed) {
        QTest::newRow(name) << int(c) << sel << clip << writable << allowed;
    };
    row("cut none", Command::Cut, 0, false, true, false);
    row("cut one", Command::Cut, 1, false, true, true);
    row("cut read only", Command::Cut, 1, false, false, false);
    row("copy read only", Command::Copy, 1, false, false, true);
    row("copy none", Command::Copy, 0, true, true, false);
    row("paste empty", Command::Paste, 0, false, true, false);
    row("paste files", Command::Paste, 0, true, true, true);
    row("paste read only", Command::Paste, 0, true, false, false);
    row("rename two", Command::Rename, 2, false, true, true);
    row("rename read only", Command::Rename, 1, false, false, false);
    row("share none", Command::Share, 0, false, true, false);
    row("share read only", Command::Share, 3, false, false, true);
    row("delete one", Command::Delete, 1, false, true, true);
    row("delete read only", Command::Delete, 1, false, false, false);
    row("new", Command::New, 0, false, true, true);
    row("new read only", Command::New, 0, false, false, false);
    row("other", Command::Other, 0, false, false, true);
}

void RayeCommandBarTest::rules()
{
    QFETCH(int, command);
    QFETCH(int, selected);
    QFETCH(bool, clipboard);
    QFETCH(bool, writable);
    QFETCH(bool, allowed);
    State s;
    s.selected = selected;
    s.clipboardHasFiles = clipboard;
    s.writable = writable;
    QCOMPARE(allows(Command(command), s), allowed);
}

void RayeCommandBarTest::foldKeepsWhatFits()
{
    const QList<Slot> slots_ = {{50, 3, 0}, {40, 2, 1}, {40, 1, 1}};
    QCOMPARE(fold(slots_, 1000, 9), (QList<bool>{true, true, true}));
    // exactly: 50 + 9 + 40 + 40
    QCOMPARE(fold(slots_, 139, 9), (QList<bool>{true, true, true}));
}

void RayeCommandBarTest::foldGoesFromTheRight()
{
    // Default priorities: the rightmost folds first.
    const QList<Slot> slots_ = {{50, 3, 0}, {40, 2, 1}, {40, 1, 1}};
    QCOMPARE(fold(slots_, 138, 9), (QList<bool>{true, true, false}));
    QCOMPARE(fold(slots_, 98, 9), (QList<bool>{true, false, false}));
    QCOMPARE(fold(slots_, 10, 9), (QList<bool>{false, false, false}));
}

void RayeCommandBarTest::foldHonoursPriorities()
{
    const QList<Slot> slots_ = {{50, 1, 0}, {40, 3, 1}, {40, 2, 1}};
    QCOMPARE(fold(slots_, 100, 9), (QList<bool>{false, true, true}));
}

void RayeCommandBarTest::foldDropsEmptyGroups()
{
    // Group 1 folds whole: its separator goes with it, which lets group 2
    // (one separator) fit where it would not with two.
    const QList<Slot> slots_ = {{50, 10, 0}, {40, 1, 1}, {30, 9, 2}};
    QCOMPARE(fold(slots_, 89, 9), (QList<bool>{true, false, true}));
}

void RayeCommandBarTest::windowsOrder()
{
    QCOMPARE(m_bar->visibleIds(),
             (QStringList{"new", "cut", "copy", "paste", "rename", "share", "delete", "sort", "view", "seemore", "details"}));
    QVERIFY(m_bar->foldedIds().isEmpty());
    // Details sits at the right end of the row.
    QWidget *details = m_bar->button(QStringLiteral("details"));
    QCOMPARE(details->geometry().right(), m_bar->width() - 1 - CommandBar::margin);
}

void RayeCommandBarTest::sizes()
{
    QCOMPARE(CommandBar::rowHeight, 48);
    QCOMPARE(m_bar->sizeHint().height(), 48);
    QCOMPARE(m_bar->button(QStringLiteral("cut"))->size(), QSize(40, 32));
    QCOMPARE(m_bar->button(QStringLiteral("cut"))->y(), 8);
    QVERIFY(m_bar->button(QStringLiteral("new"))->width() > 40);
    auto *sortButton = qobject_cast<Raye::Ui::CommandButton *>(m_bar->button(QStringLiteral("sort")));
    QVERIFY(sortButton->showsChevron());
    QVERIFY(!sortButton->iconOnly());
}

void RayeCommandBarTest::rayeWordsNotKdes()
{
    auto text = [this](const char *id) {
        return qobject_cast<QAbstractButton *>(m_bar->button(QString::fromLatin1(id)))->text();
    };
    QCOMPARE(text("delete"), QStringLiteral("Delete"));
    QCOMPARE(text("new"), QStringLiteral("New"));
    QCOMPARE(text("cut"), QStringLiteral("Cut"));
    QCOMPARE(text("share"), QStringLiteral("Share"));
    QCOMPARE(text("seemore"), QStringLiteral("See more"));
    QCOMPARE(text("details"), QStringLiteral("Details"));
    auto *copy = qobject_cast<Raye::Ui::CommandButton *>(m_bar->button(QStringLiteral("copy")));
    QCOMPARE(copy->shortcutHint(), QKeySequence(QKeySequence::Copy));
}

void RayeCommandBarTest::disabledWithoutSelection()
{
    for (const char *id : {"cut", "copy", "rename", "share", "delete", "paste"}) {
        QVERIFY2(!m_bar->button(QString::fromLatin1(id))->isEnabled(), id);
        QVERIFY2(m_bar->button(QString::fromLatin1(id))->isVisible(), id); // disabled, not hidden
    }
    QVERIFY(m_bar->button(QStringLiteral("new"))->isEnabled());
    QVERIFY(m_bar->button(QStringLiteral("sort"))->isEnabled());
}

void RayeCommandBarTest::enabledWithSelection()
{
    m_bar->setContext(folderWith(1));
    for (const char *id : {"cut", "copy", "rename", "share", "delete"}) {
        QVERIFY2(m_bar->button(QString::fromLatin1(id))->isEnabled(), id);
    }
}

void RayeCommandBarTest::sourceActionStillDecides()
{
    m_bar->setContext(folderWith(1));
    m_actions->action("edit_cut")->setEnabled(false);
    QVERIFY(!m_bar->button(QStringLiteral("cut"))->isEnabled());
    m_actions->action("edit_cut")->setEnabled(true);
    QVERIFY(m_bar->button(QStringLiteral("cut"))->isEnabled());
}

void RayeCommandBarTest::pasteFollowsTheClipboard()
{
    QVERIFY(!m_bar->button(QStringLiteral("paste"))->isEnabled());
    auto *mime = new QMimeData;
    mime->setUrls({QUrl::fromLocalFile(QStringLiteral("/tmp/a"))});
    QGuiApplication::clipboard()->setMimeData(mime);
    QTRY_VERIFY(m_bar->button(QStringLiteral("paste"))->isEnabled());
    QGuiApplication::clipboard()->setText(QStringLiteral("only text"));
    QTRY_VERIFY(!m_bar->button(QStringLiteral("paste"))->isEnabled());
}

void RayeCommandBarTest::readOnlyFolder()
{
    CommandContext c = folderWith(1);
    c.writable = false;
    m_bar->setContext(c);
    QVERIFY(!m_bar->button(QStringLiteral("cut"))->isEnabled());
    QVERIFY(m_bar->button(QStringLiteral("copy"))->isEnabled());
    QVERIFY(!m_bar->button(QStringLiteral("delete"))->isEnabled());
    QVERIFY(!m_bar->button(QStringLiteral("new"))->isEnabled());
}

void RayeCommandBarTest::buttonsTriggerTheirAction()
{
    m_bar->setContext(folderWith(1));
    for (const auto &[button, id] : {std::pair{"cut", "edit_cut"}, std::pair{"copy", "edit_copy"}, std::pair{"rename", "renamefile"}, std::pair{"delete", "movetotrash"}}) {
        QSignalSpy spy(m_actions->action(id), &QAction::triggered);
        qobject_cast<QAbstractButton *>(m_bar->button(QString::fromLatin1(button)))->click();
        QVERIFY2(spy.count() == 1, button);
    }
}

void RayeCommandBarTest::deleteFallsBackToDeleteFile()
{
    m_bar->setContext(folderWith(1));
    m_actions->action("movetotrash")->setVisible(false); // a place with no Recycle Bin
    QSignalSpy spy(m_actions->action("deletefile"), &QAction::triggered);
    qobject_cast<QAbstractButton *>(m_bar->button(QStringLiteral("delete")))->click();
    QCOMPARE(spy.count(), 1);
}

void RayeCommandBarTest::narrowWindowsFoldFromTheRight()
{
    const QStringList order = {"new", "cut", "copy", "paste", "rename", "share", "delete", "sort", "view"};
    QStringList previous;
    for (int width : {1280, 1000, 640, 420}) {
        m_bar->resize(width, CommandBar::rowHeight);
        QCoreApplication::processEvents();
        const QStringList folded = m_bar->foldedIds();
        // What folds is the right end of the row, and more as it narrows.
        for (const QString &id : folded) {
            QVERIFY2(order.indexOf(id) >= order.size() - folded.size(), qPrintable(id));
        }
        for (const QString &id : std::as_const(previous)) {
            QVERIFY(folded.contains(id));
        }
        previous = folded;
        // "…" and Details never fold, and nothing overlaps them.
        QWidget *more = m_bar->button(QStringLiteral("seemore"));
        QWidget *details = m_bar->button(QStringLiteral("details"));
        QVERIFY(more->isVisible());
        QVERIFY(details->isVisible());
        QVERIFY(more->geometry().right() < details->geometry().left());
        for (const QString &id : m_bar->visibleIds()) {
            if (id != QLatin1String("seemore") && id != QLatin1String("details")) {
                QVERIFY2(m_bar->button(id)->geometry().right() < more->geometry().left(), qPrintable(id));
            }
        }
    }
    QVERIFY(!previous.isEmpty());
}

void RayeCommandBarTest::foldedButtonsSitInSeeMore()
{
    m_bar->resize(560, CommandBar::rowHeight);
    QCoreApplication::processEvents();
    const QStringList folded = m_bar->foldedIds();
    QVERIFY(folded.contains(QStringLiteral("view")));
    QMenu *menu = m_bar->seeMoreMenu();
    const QStringList texts = actionTexts(menu);
    // folded buttons come first, in the order of the row
    QVERIFY(texts.indexOf(QStringLiteral("View")) >= 0);
    QVERIFY(texts.indexOf(QStringLiteral("View")) < texts.indexOf(QStringLiteral("Undo")));
    QCOMPARE(texts.indexOf(QStringLiteral("View")), folded.size() - 1);
    QAction *view = byText(menu, QStringLiteral("View"));
    QVERIFY(view->menu()); // still a menu once folded
}

void RayeCommandBarTest::seeMoreMenu()
{
    QMenu *menu = m_bar->seeMoreMenu();
    const QStringList texts = actionTexts(menu);
    for (const char *t : {"Undo", "Redo", "Pin to Quick access", "Select all", "Invert selection", "Properties", "Options", "Open in Terminal"}) {
        QVERIFY2(texts.contains(QString::fromLatin1(t)), t);
    }
    QVERIFY(!texts.contains(QStringLiteral("Select none"))); // no such action: left out
    QSignalSpy spy(m_actions->action("properties"), &QAction::triggered);
    byText(menu, QStringLiteral("Properties"))->trigger();
    QCOMPARE(spy.count(), 1);
}

void RayeCommandBarTest::sortMenu()
{
    QMenu *menu = m_bar->sortMenu();
    const QStringList texts = actionTexts(menu);
    QCOMPARE(texts.mid(0, 4), (QStringList{"Name", "Date modified", "Type", "Size"}));
    QVERIFY(texts.contains(QStringLiteral("Ascending")));
    QVERIFY(texts.contains(QStringLiteral("Descending")));
    QVERIFY(byText(menu, QStringLiteral("Name"))->isChecked());
    QVERIFY(byText(menu, QStringLiteral("Ascending"))->isChecked());
    QSignalSpy spy(m_actions->action("sort_by_size"), &QAction::triggered);
    byText(menu, QStringLiteral("Size"))->trigger();
    QCOMPARE(spy.count(), 1);
    // the view's other columns come under More
    QAction *more = byText(menu, QStringLiteral("More"));
    QVERIFY(more && more->menu());
    QCOMPARE(actionTexts(more->menu()), (QStringList{"KDE sort_by_owner"}));
}

void RayeCommandBarTest::groupBy()
{
    QMenu *menu = m_bar->sortMenu();
    QAction *groupBy = byText(menu, QStringLiteral("Group by"));
    QVERIFY(groupBy && groupBy->menu());
    QMenu *sub = groupBy->menu();
    QCOMPARE(actionTexts(sub), (QStringList{"(None)", "Name", "Date modified", "Type", "Size"}));
    QVERIFY(byText(sub, QStringLiteral("(None)"))->isChecked());
    byText(sub, QStringLiteral("Type"))->trigger();
    QVERIFY(m_actions->action("show_in_groups")->isChecked());
    QVERIFY(m_actions->action("sort_by_type")->isChecked());
    menu = m_bar->sortMenu();
    sub = byText(menu, QStringLiteral("Group by"))->menu();
    QVERIFY(byText(sub, QStringLiteral("Type"))->isChecked());
    byText(sub, QStringLiteral("(None)"))->trigger();
    QVERIFY(!m_actions->action("show_in_groups")->isChecked());
}

void RayeCommandBarTest::viewMenu()
{
    QMenu *menu = m_bar->viewMenu();
    const QStringList texts = actionTexts(menu);
    QCOMPARE(texts.mid(0, 3), (QStringList{"Icons", "List", "Details"}));
    QVERIFY(byText(menu, QStringLiteral("Details"))->isChecked());
    QAction *show = byText(menu, QStringLiteral("Show"));
    QVERIFY(show && show->menu());
    const QStringList shown = actionTexts(show->menu());
    QCOMPARE(shown, (QStringList{"Details pane", "Preview pane", "Hidden items"}));
    byText(show->menu(), QStringLiteral("Hidden items"))->trigger();
    QVERIFY(m_actions->action("show_hidden_files")->isChecked());
}

void RayeCommandBarTest::detailsToggle()
{
    auto *details = qobject_cast<QAbstractButton *>(m_bar->button(QStringLiteral("details")));
    QVERIFY(details->isCheckable());
    QVERIFY(!details->isChecked());
    details->click();
    QVERIFY(m_actions->action("raye_details_pane")->isChecked());
    m_actions->action("raye_details_pane")->setChecked(false);
    QVERIFY(!details->isChecked());

    // without the action (no details pane yet) there is no button
    FakeActions others;
    others.actions.remove(QStringLiteral("raye_details_pane"));
    CommandBar bar;
    bar.setActionLookup(others.lookup());
    bar.resize(1280, CommandBar::rowHeight);
    bar.show();
    QCoreApplication::processEvents();
    QVERIFY(!bar.visibleIds().contains(QStringLiteral("details")));
}

void RayeCommandBarTest::shareButton()
{
    QList<QUrl> shared;
    QWidget *anchor = nullptr;
    m_bar->setShareHandler([&](QWidget *a, const QList<QUrl> &urls) {
        anchor = a;
        shared = urls;
    });
    m_bar->setContext(folderWith(2));
    qobject_cast<QAbstractButton *>(m_bar->button(QStringLiteral("share")))->click();
    QCOMPARE(shared, folderWith(2).selection);
    QCOMPARE(anchor, m_bar->button(QStringLiteral("share")));
}

void RayeCommandBarTest::contextSetJoinsAndLeaves()
{
    QAction empty(QStringLiteral("Empty Recycle Bin"));
    empty.setObjectName(QStringLiteral("raye_empty_trash"));
    m_bar->registerSet(QStringLiteral("recyclebin"), {&empty}, [](const CommandContext &c) {
        return c.url.scheme() == QLatin1String("trash");
    });
    QVERIFY(!m_bar->visibleIds().contains(QStringLiteral("raye_empty_trash")));
    CommandContext trash;
    trash.url = QUrl(QStringLiteral("trash:/"));
    m_bar->setContext(trash);
    const QStringList ids = m_bar->visibleIds();
    QVERIFY(ids.contains(QStringLiteral("raye_empty_trash")));
    // after View, before "…"
    QVERIFY(ids.indexOf(QStringLiteral("raye_empty_trash")) > ids.indexOf(QStringLiteral("view")));
    QVERIFY(ids.indexOf(QStringLiteral("raye_empty_trash")) < ids.indexOf(QStringLiteral("seemore")));
    auto *b = qobject_cast<QAbstractButton *>(m_bar->button(QStringLiteral("raye_empty_trash")));
    QCOMPARE(b->text(), QStringLiteral("Empty Recycle Bin"));
    QSignalSpy spy(&empty, &QAction::triggered);
    b->click();
    QCOMPARE(spy.count(), 1);
    m_bar->setContext(folderWith(0));
    QVERIFY(!m_bar->visibleIds().contains(QStringLiteral("raye_empty_trash")));
}

void RayeCommandBarTest::contextSetCanReplaceEditing()
{
    QAction collection(QStringLiteral("Collection"));
    collection.setObjectName(QStringLiteral("raye_collection"));
    m_bar->registerSet(
        QStringLiteral("gallery"),
        {&collection},
        [](const CommandContext &c) {
            return c.url == QUrl(QStringLiteral("raye:gallery"));
        },
        Placement::ReplaceEditing);
    CommandContext gallery;
    gallery.url = QUrl(QStringLiteral("raye:gallery"));
    m_bar->setContext(gallery);
    const QStringList ids = m_bar->visibleIds();
    QVERIFY(ids.contains(QStringLiteral("raye_collection")));
    for (const char *id : {"new", "cut", "copy", "paste", "rename", "share", "delete"}) {
        QVERIFY2(!ids.contains(QString::fromLatin1(id)), id);
    }
    QVERIFY(ids.contains(QStringLiteral("sort")));
}

void RayeCommandBarTest::contextSetWithAMenu()
{
    QAction filter(QStringLiteral("Filter"));
    filter.setObjectName(QStringLiteral("raye_filter"));
    QMenu filterMenu;
    filterMenu.addAction(QStringLiteral("Pictures"));
    filter.setMenu(&filterMenu);
    m_bar->registerSet(QStringLiteral("search"), {&filter}, [](const CommandContext &c) {
        return c.searching;
    });
    CommandContext search = folderWith(0);
    search.searching = true;
    m_bar->setContext(search);
    auto *b = qobject_cast<Raye::Ui::CommandButton *>(m_bar->button(QStringLiteral("raye_filter")));
    QVERIFY(b);
    QCOMPARE(b->menu(), &filterMenu);
    QVERIFY(b->showsChevron());
}

void RayeCommandBarTest::unregisterSet()
{
    QAction a(QStringLiteral("X"));
    a.setObjectName(QStringLiteral("raye_x"));
    m_bar->registerSet(QStringLiteral("x"), {&a}, [](const CommandContext &) {
        return true;
    });
    m_bar->setContext(folderWith(0));
    QVERIFY(m_bar->visibleIds().contains(QStringLiteral("raye_x")));
    QVERIFY(m_bar->unregisterSet(QStringLiteral("x")));
    QVERIFY(!m_bar->visibleIds().contains(QStringLiteral("raye_x")));
    QVERIFY(!m_bar->unregisterSet(QStringLiteral("x")));
}

void RayeCommandBarTest::picturesSet()
{
    m_bar->setContext(folderWith(1, QStringLiteral("image/png")));
    QVERIFY(m_bar->visibleIds().contains(QString::fromLatin1(Ids::setAsBackground)));
    auto *b = qobject_cast<QAbstractButton *>(m_bar->button(QString::fromLatin1(Ids::setAsBackground)));
    QCOMPARE(b->text(), QStringLiteral("Set as background"));
    m_bar->setContext(folderWith(2, QStringLiteral("image/png")));
    QVERIFY(!m_bar->visibleIds().contains(QString::fromLatin1(Ids::setAsBackground)));
    m_bar->setContext(folderWith(1, QStringLiteral("application/pdf")));
    QVERIFY(!m_bar->visibleIds().contains(QString::fromLatin1(Ids::setAsBackground)));
    CommandContext remote = folderWith(0);
    remote.selection = {QUrl(QStringLiteral("smb://nas/a.png"))};
    remote.selectionMimeTypes = {QStringLiteral("image/png")};
    m_bar->setContext(remote);
    QVERIFY(!m_bar->visibleIds().contains(QString::fromLatin1(Ids::setAsBackground)));
}

void RayeCommandBarTest::rotateNeedsAServiceMenu()
{
    // No service menu here rotates pictures: no Rotate buttons.
    QTemporaryDir empty;
    const RotateActions found = findRotateActions({empty.path()});
    QVERIFY(!found.left.isValid());
    QVERIFY(!found.right.isValid());
    m_bar->setContext(folderWith(1, QStringLiteral("image/jpeg")));
    if (!findRotateActions(serviceMenuDirs()).left.isValid()) {
        QVERIFY(!m_bar->visibleIds().contains(QString::fromLatin1(Ids::rotateLeft)));
    }
}

void RayeCommandBarTest::rotateActionsAreFound()
{
    QTemporaryDir dir;
    // Service menus outside the system folders count only when executable.
    auto write = [&dir](const char *name, const char *text) {
        QFile f(QDir(dir.path()).filePath(QString::fromLatin1(name)));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(text);
        f.close();
        QVERIFY(f.setPermissions(f.permissions() | QFileDevice::ExeOwner));
    };
    write("pictures.desktop",
          "[Desktop Entry]\nType=Service\nMimeType=image/jpeg;image/png;\nActions=rotateLeft;rotateRight;\n"
          "[Desktop Action rotateLeft]\nName=Rotate Left\nExec=rotate -l %F\n"
          "[Desktop Action rotateRight]\nName=Rotate Right\nExec=rotate -r %F\n");
    write("text.desktop",
          "[Desktop Entry]\nType=Service\nMimeType=text/plain;\nActions=rotateRight;\n"
          "[Desktop Action rotateRight]\nName=Rotate text\nExec=x %F\n");
    const RotateActions found = findRotateActions({dir.path()});
    QVERIFY(found.left.isValid());
    QVERIFY(found.right.isValid());
    QCOMPARE(found.left.action, QStringLiteral("rotateLeft"));
    QCOMPARE(found.right.action, QStringLiteral("rotateRight"));
    QVERIFY(found.right.file.endsWith(QLatin1String("pictures.desktop")));

    QTemporaryDir explicitDir;
    QFile f(QDir(explicitDir.path()).filePath(QStringLiteral("a.desktop")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[Desktop Entry]\nType=Service\nMimeType=image/*;\nActions=ccw;\n[Desktop Action ccw]\nName=Turn\nX-Raye-Rotate=left\nExec=t %F\n");
    f.close();
    // not executable: someone else's file dropped in, refused (files root
    // owns count as system ones, so this only holds for a normal account)
    if (::geteuid() != 0) {
        QVERIFY(!findRotateActions({explicitDir.path()}).left.isValid());
    }
    QVERIFY(f.setPermissions(f.permissions() | QFileDevice::ExeOwner));
    QCOMPARE(findRotateActions({explicitDir.path()}).left.action, QStringLiteral("ccw"));
}

void RayeCommandBarTest::driveSet()
{
    CommandContext usb;
    usb.url = QUrl(QStringLiteral("raye:thispc"));
    usb.driveUdi = QStringLiteral("/org/freedesktop/UDisks2/block_devices/sdb1");
    usb.driveRemovable = true;
    m_bar->setContext(usb);
    QVERIFY(m_bar->visibleIds().contains(QString::fromLatin1(Ids::eject)));
    auto *b = qobject_cast<QAbstractButton *>(m_bar->button(QString::fromLatin1(Ids::eject)));
    QCOMPARE(b->text(), QStringLiteral("Eject"));
    usb.driveRemovable = false;
    m_bar->setContext(usb);
    QVERIFY(!m_bar->visibleIds().contains(QString::fromLatin1(Ids::eject)));
    m_bar->setContext(folderWith(0));
    QVERIFY(!m_bar->visibleIds().contains(QString::fromLatin1(Ids::format)));
}

QTEST_MAIN(RayeCommandBarTest)

#include "test_commandbar.moc"
