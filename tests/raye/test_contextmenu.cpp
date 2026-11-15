/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The right-click menu (explorer spec 1.11, 3.11): the menu is built for
// eight fixture selections (a file, a folder with files on the clipboard,
// a ZIP, a picture, a program, a USB drive, the folder background, an item
// in the Recycle Bin) and more, and its ids are compared with the table of
// 3.11. Also: Raye's words and shortcut hints, the icon row, window actions
// followed and triggered, items left out, "Copy as path" quoting, shortcut
// names, item kinds, the submenus built on the window's actions, and the
// plugins the full menu leaves out.

#include "dolphincontextmenu.h"
#include "dolphinmainwindow.h"
#include "menu/compactmenu.h"
#include "menu/menuactions.h"
#include "menu/menuplan.h"
#include "ui/carddialog.h"
#include "ui/motion.h"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QWidgetAction>

#include <KActionCollection>
#include <KFileItem>
#include <KStandardAction>
#include <KFileItemActions>
#include <KIO/SimpleJob>

#include <memory>
#include <vector>

using namespace Raye::Menu;

Q_DECLARE_METATYPE(Raye::Menu::Selection)

namespace
{
QStringList ids(std::initializer_list<const char *> list)
{
    QStringList out;
    for (const char *s : list) {
        out << QString::fromLatin1(s);
    }
    return out;
}

Selection one(ItemKind kind)
{
    Selection s;
    s.items = {kind};
    return s;
}

// The "Open with" dialogs this window has made, and whether they are on
// the screen: one that was built but never shown is invisible here.
QList<Raye::Ui::CardDialog *> openWithDialogs(QWidget *window)
{
    return window->findChildren<Raye::Ui::CardDialog *>();
}

// Hooks that give every submenu one item and record what is triggered.
struct Recorder {
    QStringList triggered;
    CompactMenu::Hooks hooks()
    {
        CompactMenu::Hooks h;
        h.trigger = [this](const QString &id) {
            triggered << id;
        };
        h.submenu = [](const QString &id, QMenu *parent) {
            if (id == QLatin1String("compress")) {
                return static_cast<QMenu *>(nullptr); // the menu's own
            }
            auto *m = new QMenu(parent);
            m->addAction(id + QStringLiteral(" item"));
            return m;
        };
        return h;
    }
};
}

class RayeContextMenuTest : public QObject
{
    Q_OBJECT

public:
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
    void fixtures_data();
    void fixtures();
    void builtMenuMatchesPlan_data();
    void builtMenuMatchesPlan();
    void wordsAndHints();
    void iconRow();
    void windowActionsFollowed();
    void ownItemsTriggerHooks();
    void compressSubmenu();
    void itemsLeftOut();
    void textHook();
    void openIsBold();
    void kinds_data();
    void kinds();
    void copyAsPathQuoting();
    void copyAsPathClipboard();
    void shortcutNames_data();
    void shortcutNames();
    void createShortcutsMakesLinks();
    void groupMenuOnWindowActions();
    void openWithMenu();
    void chooseAnotherAppShowsTheDialog();
    void excludedPlugins();
    void adminUrl();
    void filesWindowMenus();
};

void RayeContextMenuTest::initTestCase()
{
    Raye::Ui::Motion::setSpeedFactor(0);
}

void RayeContextMenuTest::fixtures_data()
{
    QTest::addColumn<Selection>("selection");
    QTest::addColumn<QStringList>("row");
    QTest::addColumn<QStringList>("items");

    // 1. a text file
    QTest::newRow("file") << one(ItemKind::File) << ids({"cut", "copy", "rename", "share", "delete"})
                          << ids({"open", "open_with", "-", "favorite", "-", "compress", "-", "copy_path", "create_shortcut", "-", "properties", "-",
                                  "more_options"});
    // 2. a folder, files on the clipboard: Paste joins the row
    Selection folder = one(ItemKind::Folder);
    folder.clipboardHasFiles = true;
    QTest::newRow("folder") << folder << ids({"cut", "copy", "paste", "rename", "share", "delete"})
                            << ids({"open", "open_new_tab", "open_new_window", "-", "pin_quick_access", "-", "compress", "-", "copy_path",
                                    "create_shortcut", "open_terminal", "-", "properties", "-", "more_options"});
    // 3. a ZIP
    QTest::newRow("archive") << one(ItemKind::Archive) << ids({"cut", "copy", "rename", "share", "delete"})
                             << ids({"open", "open_with", "-", "favorite", "-", "compress", "extract_all", "extract_here", "extract_to", "-", "copy_path",
                                     "create_shortcut", "-", "properties", "-", "more_options"});
    // 4. a picture
    QTest::newRow("picture") << one(ItemKind::Picture) << ids({"cut", "copy", "rename", "share", "delete"})
                             << ids({"open", "open_with", "-", "favorite", "-", "compress", "-", "set_background", "rotate_right", "rotate_left", "-",
                                     "copy_path", "create_shortcut", "-", "properties", "-", "more_options"});
    // 5. a program: no Open with
    QTest::newRow("executable") << one(ItemKind::Executable) << ids({"cut", "copy", "rename", "share", "delete"})
                                << ids({"open", "-", "favorite", "-", "compress", "-", "copy_path", "create_shortcut", "-", "properties", "-",
                                        "more_options"});
    // 6. a USB drive in This PC
    Selection drive = one(ItemKind::Drive);
    drive.driveRemovable = true;
    QTest::newRow("removable drive") << drive << QStringList()
                                     << ids({"open", "open_new_tab", "open_new_window", "-", "pin_quick_access", "-", "eject", "format", "-", "properties",
                                             "-", "more_options"});
    // 7. the folder background, files on the clipboard
    Selection background;
    background.background = true;
    background.clipboardHasFiles = true;
    QTest::newRow("background") << background << ids({"paste"})
                                << ids({"view", "sort", "group", "refresh", "-", "undo", "new", "-", "open_terminal", "properties", "-", "more_options"});
    // 8. an item in the Recycle Bin
    Selection binItem = one(ItemKind::File);
    binItem.place = Place::RecycleBin;
    QTest::newRow("recycle bin item") << binItem << ids({"cut", "delete"}) << ids({"restore", "-", "properties"});

    // and more
    Selection drive2 = one(ItemKind::Drive);
    QTest::newRow("fixed drive") << drive2 << QStringList()
                                 << ids({"open", "open_new_tab", "open_new_window", "-", "pin_quick_access", "-", "format", "-", "properties", "-",
                                         "more_options"});
    Selection emptyClip;
    emptyClip.background = true;
    QTest::newRow("background, nothing to paste") << emptyClip << QStringList()
                                                  << ids({"view", "sort", "group", "refresh", "-", "undo", "new", "-", "open_terminal", "properties", "-",
                                                          "more_options"});
    Selection readOnlyBackground;
    readOnlyBackground.background = true;
    readOnlyBackground.clipboardHasFiles = true;
    readOnlyBackground.writable = false;
    QTest::newRow("background, read only") << readOnlyBackground << QStringList()
                                           << ids({"view", "sort", "group", "refresh", "-", "undo", "-", "open_terminal", "properties", "-", "more_options"});
    Selection binBackground;
    binBackground.background = true;
    binBackground.place = Place::RecycleBin;
    QTest::newRow("recycle bin background") << binBackground << QStringList()
                                            << ids({"view", "sort", "group", "refresh", "-", "empty_bin", "-", "properties"});
    Selection found = one(ItemKind::File);
    found.place = Place::Search;
    QTest::newRow("search result") << found << ids({"cut", "copy", "rename", "share", "delete"})
                                   << ids({"open", "open_with", "open_file_location", "-", "favorite", "-", "copy_path", "-", "properties", "-",
                                           "more_options"});
    Selection mixed;
    mixed.items = {ItemKind::File, ItemKind::Folder};
    QTest::newRow("file and folder") << mixed << ids({"cut", "copy", "rename", "share", "delete"})
                                     << ids({"open", "-", "compress", "-", "copy_path", "create_shortcut", "-", "properties", "-", "more_options"});
    Selection folders;
    folders.items = {ItemKind::Folder, ItemKind::Folder};
    folders.clipboardHasFiles = true;
    QTest::newRow("two folders") << folders << ids({"cut", "copy", "rename", "share", "delete"})
                                 << ids({"open", "open_new_tab", "open_new_window", "-", "compress", "-", "copy_path", "create_shortcut", "-", "properties",
                                         "-", "more_options"});
    Selection remote = one(ItemKind::Picture);
    remote.local = false;
    QTest::newRow("picture on a network share") << remote << ids({"cut", "copy", "rename", "share", "delete"})
                                                << ids({"open", "open_with", "-", "copy_path", "-", "properties", "-", "more_options"});
    Selection readOnly = one(ItemKind::Folder);
    readOnly.writable = false;
    readOnly.clipboardHasFiles = true;
    QTest::newRow("folder in a read-only place") << readOnly << ids({"cut", "copy", "rename", "share", "delete"})
                                                 << ids({"open", "open_new_tab", "open_new_window", "-", "pin_quick_access", "-", "copy_path", "open_terminal",
                                                         "-", "properties", "-", "more_options"});
    // a shared folder seen on the network: Map network drive
    Selection share = one(ItemKind::Folder);
    share.local = false;
    share.writable = false;
    share.networkShare = true;
    QTest::newRow("network share") << share << ids({"cut", "copy", "rename", "share", "delete"})
                                   << ids({"open", "open_new_tab", "open_new_window", "-", "map_drive", "-", "pin_quick_access", "-", "copy_path", "-",
                                           "properties", "-", "more_options"});
    // the folder of a mapped network drive: Disconnect
    Selection mapped = one(ItemKind::Folder);
    mapped.networkDrive = true;
    QTest::newRow("network drive") << mapped << ids({"cut", "copy", "rename", "share", "delete"})
                                   << ids({"open", "open_new_tab", "open_new_window", "-", "disconnect_drive", "-", "pin_quick_access", "-", "compress",
                                           "-", "copy_path", "create_shortcut", "open_terminal", "-", "properties", "-", "more_options"});
    Selection readOnlyZip = one(ItemKind::Archive);
    readOnlyZip.writable = false;
    QTest::newRow("archive in a read-only place") << readOnlyZip << ids({"cut", "copy", "rename", "share", "delete"})
                                                  << ids({"open", "open_with", "-", "favorite", "-", "extract_all", "-", "copy_path", "-",
                                                          "properties", "-", "more_options"});
}

void RayeContextMenuTest::fixtures()
{
    QFETCH(Selection, selection);
    QFETCH(QStringList, row);
    QFETCH(QStringList, items);
    const Plan p = plan(selection);
    QCOMPARE(p.row, row);
    QCOMPARE(p.items, items);
}

void RayeContextMenuTest::builtMenuMatchesPlan_data()
{
    fixtures_data();
}

void RayeContextMenuTest::builtMenuMatchesPlan()
{
    QFETCH(Selection, selection);
    QFETCH(QStringList, row);
    QFETCH(QStringList, items);
    Recorder recorder;
    CompactMenu menu(plan(selection), recorder.hooks());
    QCOMPARE(menu.rowIds(), row);
    QCOMPARE(menu.itemIds(), items);

    // The menu holds them in that order: the row first, then the list.
    QStringList shown;
    for (QAction *a : menu.actions()) {
        if (a->objectName() == QLatin1String("icon_row")) {
            continue;
        }
        shown << (a->isSeparator() ? QStringLiteral("-") : a->objectName());
    }
    if (!row.isEmpty() && !items.isEmpty()) {
        QCOMPARE(shown.first(), QStringLiteral("-")); // between the row and the list
        shown.removeFirst();
    }
    QCOMPARE(shown, items);
    QCOMPARE(menu.iconRow() != nullptr, !row.isEmpty());
}

void RayeContextMenuTest::wordsAndHints()
{
    QCOMPARE(CompactMenu::defaultText(QStringLiteral("more_options")), QStringLiteral("Show more options"));
    QCOMPARE(CompactMenu::defaultText(QStringLiteral("copy_path")), QStringLiteral("Copy as path"));
    QCOMPARE(CompactMenu::defaultText(QStringLiteral("pin_quick_access")), QStringLiteral("Pin to Quick access"));
    QCOMPARE(CompactMenu::defaultText(QStringLiteral("empty_bin")), QStringLiteral("Empty Recycle Bin"));
    QCOMPARE(CompactMenu::defaultText(QStringLiteral("compress")), QStringLiteral("Compress to"));
    QCOMPARE(CompactMenu::defaultText(QStringLiteral("extract_all")), QString::fromUtf8("Extract all…"));
    QCOMPARE(CompactMenu::defaultText(QStringLiteral("open_admin")), QStringLiteral("Open as administrator"));
    QCOMPARE(CompactMenu::shortcutHint(QStringLiteral("copy_path")), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    QCOMPARE(CompactMenu::shortcutHint(QStringLiteral("properties")), QKeySequence(Qt::ALT | Qt::Key_Return));
    QCOMPARE(CompactMenu::shortcutHint(QStringLiteral("more_options")), QKeySequence(Qt::SHIFT | Qt::Key_F10));
    QCOMPARE(CompactMenu::shortcutHint(QStringLiteral("rename")), QKeySequence(Qt::Key_F2));

    Recorder r;
    CompactMenu menu(plan(one(ItemKind::File)), r.hooks());
    QAction *copyPath = menu.item(QStringLiteral("copy_path"));
    QVERIFY(copyPath);
    QCOMPARE(copyPath->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    // A hint only: it never fires outside the menu.
    QCOMPARE(copyPath->shortcutContext(), Qt::WidgetShortcut);
    // Every item of every fixture has words and an icon name.
    const QStringList all = ids({"cut", "copy", "paste", "rename", "share", "delete", "open", "open_with", "open_new_tab", "open_new_window",
                                 "open_file_location", "pin_quick_access", "favorite", "compress", "extract_all", "set_background", "rotate_right",
                                 "rotate_left", "copy_path", "create_shortcut", "open_terminal", "eject", "format", "restore", "empty_bin", "view", "sort",
                                 "group", "refresh", "undo", "new", "properties", "more_options"});
    for (const QString &id : all) {
        QVERIFY2(!CompactMenu::defaultText(id).isEmpty(), qPrintable(id));
        QVERIFY2(!CompactMenu::iconName(id).isEmpty(), qPrintable(id));
    }
}

void RayeContextMenuTest::iconRow()
{
    Recorder r;
    Selection s = one(ItemKind::Folder);
    s.clipboardHasFiles = true;
    CompactMenu menu(plan(s), r.hooks());
    QWidget *row = menu.iconRow();
    QVERIFY(row);
    const QList<QAbstractButton *> buttons = row->findChildren<QAbstractButton *>();
    QCOMPARE(buttons.size(), 6);
    QStringList names;
    for (QAbstractButton *b : buttons) {
        // 48 x 48, wider when a name needs it, one width for the row.
        QCOMPARE(b->height(), 48);
        QVERIFY(b->width() >= 48 && b->width() <= 80);
        QCOMPARE(b->width(), buttons.first()->width());
        names << b->objectName();
    }
    QCOMPARE(names, ids({"cut", "copy", "paste", "rename", "share", "delete"}));
    QCOMPARE(buttons.first()->accessibleName(), QStringLiteral("Cut"));
    QVERIFY(buttons.first()->toolTip().startsWith(QStringLiteral("Cut")));
    QVERIFY(row->sizeHint().height() >= 48);

    // A click runs the item and closes the menu.
    menu.popup(QPoint(10, 10));
    QVERIFY(QTest::qWaitForWindowExposed(&menu));
    QTest::mouseClick(buttons.at(4), Qt::LeftButton); // Share
    QCOMPARE(r.triggered, QStringList{QStringLiteral("share")});
    QTRY_VERIFY(!menu.isVisible());
}

void RayeContextMenuTest::windowActionsFollowed()
{
    QAction windowCut(QStringLiteral("KDE Cut"));
    windowCut.setEnabled(false);
    QSignalSpy cut(&windowCut, &QAction::triggered);
    Recorder r;
    CompactMenu::Hooks h = r.hooks();
    h.action = [&windowCut](const QString &id) {
        return id == QLatin1String("cut") ? &windowCut : nullptr;
    };
    CompactMenu menu(plan(one(ItemKind::File)), h);
    QAction *a = menu.item(QStringLiteral("cut"));
    QVERIFY(a);
    QCOMPARE(a->text(), QStringLiteral("Cut")); // Raye's words, not the window's
    QVERIFY(!a->isEnabled()); // the window's state
    windowCut.setEnabled(true);
    QVERIFY(a->isEnabled());
    a->trigger();
    QCOMPARE(cut.count(), 1);
    QVERIFY(!r.triggered.contains(QStringLiteral("cut")));
}

void RayeContextMenuTest::ownItemsTriggerHooks()
{
    Recorder r;
    CompactMenu menu(plan(one(ItemKind::Picture)), r.hooks());
    for (const char *id : {"open", "favorite", "set_background", "copy_path", "create_shortcut", "more_options"}) {
        QAction *a = menu.item(QString::fromLatin1(id));
        QVERIFY2(a, id);
        a->trigger();
    }
    QCOMPARE(r.triggered, ids({"open", "favorite", "set_background", "copy_path", "create_shortcut", "more_options"}));
}

void RayeContextMenuTest::compressSubmenu()
{
    Recorder r;
    CompactMenu menu(plan(one(ItemKind::File)), r.hooks());
    QAction *compress = menu.item(QStringLiteral("compress"));
    QVERIFY(compress && compress->menu());
    QStringList sub;
    for (QAction *a : compress->menu()->actions()) {
        sub << (a->isSeparator() ? QStringLiteral("-") : a->text());
    }
    QCOMPARE(sub, (QStringList{QStringLiteral("ZIP file"), QStringLiteral("7z file"), QStringLiteral("tar.zst file"), QStringLiteral("-"),
                               QString::fromUtf8("Additional options…")}));
    compress->menu()->actions().at(1)->trigger();
    QCOMPARE(r.triggered, QStringList{QStringLiteral("compress_7z")});
}

void RayeContextMenuTest::itemsLeftOut()
{
    Recorder r;
    CompactMenu::Hooks h = r.hooks();
    // No rotate service and no submenu for Open with: those items go, and
    // no separator is left doubled or at the end.
    h.available = [](const QString &id) {
        return !id.startsWith(QLatin1String("rotate_"));
    };
    h.submenu = [](const QString &, QMenu *) {
        return static_cast<QMenu *>(nullptr);
    };
    CompactMenu menu(plan(one(ItemKind::Picture)), h);
    QCOMPARE(menu.itemIds(),
             ids({"open", "-", "favorite", "-", "compress", "-", "set_background", "-", "copy_path", "create_shortcut", "-", "properties", "-",
                  "more_options"}));

    // A whole group gone: still a single separator.
    h.available = [](const QString &id) {
        return id != QLatin1String("set_background") && !id.startsWith(QLatin1String("rotate_"));
    };
    CompactMenu menu2(plan(one(ItemKind::Picture)), h);
    QCOMPARE(menu2.itemIds(),
             ids({"open", "-", "favorite", "-", "compress", "-", "copy_path", "create_shortcut", "-", "properties", "-", "more_options"}));
}

void RayeContextMenuTest::textHook()
{
    Recorder r;
    CompactMenu::Hooks h = r.hooks();
    h.text = [](const QString &id) {
        return id == QLatin1String("pin_quick_access") ? CompactMenu::defaultText(QStringLiteral("unpin_quick_access")) : QString();
    };
    CompactMenu menu(plan(one(ItemKind::Folder)), h);
    QCOMPARE(menu.item(QStringLiteral("pin_quick_access"))->text(), QStringLiteral("Unpin from Quick access"));
    QCOMPARE(menu.item(QStringLiteral("open"))->text(), QStringLiteral("Open"));
}

void RayeContextMenuTest::openIsBold()
{
    Recorder r;
    CompactMenu menu(plan(one(ItemKind::File)), r.hooks());
    QVERIFY(menu.item(QStringLiteral("open"))->font().bold());
    QVERIFY(!menu.item(QStringLiteral("properties"))->font().bold());
}

void RayeContextMenuTest::kinds_data()
{
    QTest::addColumn<QString>("mime");
    QTest::addColumn<bool>("dir");
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("exec");
    QTest::addColumn<int>("kind");
    QTest::newRow("folder") << QStringLiteral("inode/directory") << true << QStringLiteral("Work") << false << int(ItemKind::Folder);
    QTest::newRow("text") << QStringLiteral("text/plain") << false << QStringLiteral("a.txt") << false << int(ItemKind::File);
    QTest::newRow("zip") << QStringLiteral("application/zip") << false << QStringLiteral("a.zip") << false << int(ItemKind::Archive);
    QTest::newRow("tar.gz") << QStringLiteral("application/x-compressed-tar") << false << QStringLiteral("a.tar.gz") << false << int(ItemKind::Archive);
    QTest::newRow("docx is a file") << QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document") << false
                                    << QStringLiteral("a.docx") << false << int(ItemKind::File);
    QTest::newRow("epub is a file") << QStringLiteral("application/epub+zip") << false << QStringLiteral("a.epub") << false << int(ItemKind::File);
    QTest::newRow("png") << QStringLiteral("image/png") << false << QStringLiteral("a.png") << false << int(ItemKind::Picture);
    QTest::newRow("exe") << QStringLiteral("application/x-ms-dos-executable") << false << QStringLiteral("setup.EXE") << false << int(ItemKind::Executable);
    QTest::newRow("msi") << QStringLiteral("application/x-msi") << false << QStringLiteral("tool.msi") << false << int(ItemKind::Executable);
    QTest::newRow("script with x bit") << QStringLiteral("application/x-shellscript") << false << QStringLiteral("run.sh") << true << int(ItemKind::Executable);
    QTest::newRow("script without x bit") << QStringLiteral("application/x-shellscript") << false << QStringLiteral("run.sh") << false << int(ItemKind::File);
    QTest::newRow("text with x bit") << QStringLiteral("text/plain") << false << QStringLiteral("notes.txt") << true << int(ItemKind::File);
}

void RayeContextMenuTest::kinds()
{
    QFETCH(QString, mime);
    QFETCH(bool, dir);
    QFETCH(QString, name);
    QFETCH(bool, exec);
    QFETCH(int, kind);
    QCOMPARE(int(kindOf(mime, dir, name, exec)), kind);
}

void RayeContextMenuTest::copyAsPathQuoting()
{
    QCOMPARE(copyAsPathText({QUrl::fromLocalFile(QStringLiteral("/home/ann/a b.txt"))}), QStringLiteral("\"/home/ann/a b.txt\""));
    QCOMPARE(copyAsPathText({QUrl::fromLocalFile(QStringLiteral("/home/ann/a.txt")), QUrl::fromLocalFile(QStringLiteral("/home/ann/Work"))}),
             QStringLiteral("\"/home/ann/a.txt\"\n\"/home/ann/Work\""));
    // An address keeps no password.
    QCOMPARE(copyAsPathText({QUrl(QStringLiteral("smb://ann:secret@nas/share/x.pdf"))}), QStringLiteral("\"smb://ann@nas/share/x.pdf\""));
    QCOMPARE(copyAsPathText({}), QString());
}

void RayeContextMenuTest::copyAsPathClipboard()
{
    copyAsPath({QUrl::fromLocalFile(QStringLiteral("/tmp/x y"))});
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("\"/tmp/x y\""));
}

void RayeContextMenuTest::shortcutNames_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("dir");
    QTest::addColumn<QStringList>("taken");
    QTest::addColumn<QString>("expected");
    QTest::newRow("file") << QStringLiteral("report.pdf") << false << QStringList() << QStringLiteral("report - Shortcut.pdf");
    QTest::newRow("folder") << QStringLiteral("Work") << true << QStringList() << QStringLiteral("Work - Shortcut");
    QTest::newRow("folder with a dot") << QStringLiteral("v1.2") << true << QStringList() << QStringLiteral("v1.2 - Shortcut");
    QTest::newRow("double suffix") << QStringLiteral("photos.tar.gz") << false << QStringList() << QStringLiteral("photos - Shortcut.tar.gz");
    QTest::newRow("no suffix") << QStringLiteral("README") << false << QStringList() << QStringLiteral("README - Shortcut");
    QTest::newRow("taken") << QStringLiteral("report.pdf") << false << QStringList{QStringLiteral("report - Shortcut.pdf")}
                           << QStringLiteral("report - Shortcut (2).pdf");
    QTest::newRow("taken twice") << QStringLiteral("Work") << true << QStringList{QStringLiteral("Work - Shortcut"), QStringLiteral("Work - Shortcut (2)")}
                                 << QStringLiteral("Work - Shortcut (3)");
    QTest::newRow("hidden file") << QStringLiteral(".bashrc") << false << QStringList() << QStringLiteral(".bashrc - Shortcut");
}

void RayeContextMenuTest::shortcutNames()
{
    QFETCH(QString, name);
    QFETCH(bool, dir);
    QFETCH(QStringList, taken);
    QFETCH(QString, expected);
    QCOMPARE(shortcutName(name, dir, taken), expected);
}

void RayeContextMenuTest::createShortcutsMakesLinks()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("a b.txt"));
    QFile f(file);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("Work")));
    const QList<KJob *> jobs = createShortcuts({QUrl::fromLocalFile(file), QUrl::fromLocalFile(dir.filePath(QStringLiteral("Work"))),
                                                QUrl::fromLocalFile(file), QUrl(QStringLiteral("smb://nas/x"))},
                                               nullptr);
    QCOMPARE(jobs.size(), 3); // the network item is skipped
    // Watch every job before the event loop runs: a finished job deletes
    // itself.
    std::vector<std::unique_ptr<QSignalSpy>> done;
    for (KJob *job : jobs) {
        done.push_back(std::make_unique<QSignalSpy>(job, &KJob::result));
    }
    for (const auto &spy : done) {
        QVERIFY(spy->count() > 0 || spy->wait(10000));
    }
    // What the jobs made says whether they worked (they are gone by now).
    const QFileInfo first(dir.filePath(QStringLiteral("a b - Shortcut.txt")));
    QVERIFY(first.isSymLink());
    QCOMPARE(first.symLinkTarget(), file);
    QVERIFY(QFileInfo(dir.filePath(QStringLiteral("a b - Shortcut (2).txt"))).isSymLink()); // the same file twice
    QVERIFY(QFileInfo(dir.filePath(QStringLiteral("Work - Shortcut"))).isSymLink());
}

void RayeContextMenuTest::groupMenuOnWindowActions()
{
    QObject owner;
    QHash<QString, QAction *> actions;
    auto add = [&](const char *name, bool checked) {
        auto *a = new QAction(QString::fromLatin1(name), &owner);
        a->setObjectName(QString::fromLatin1(name));
        a->setCheckable(true);
        a->setChecked(checked);
        actions.insert(a->objectName(), a);
        return a;
    };
    auto *sortGroup = new QActionGroup(&owner);
    for (const char *n : {"sort_by_text", "sort_by_modificationtime", "sort_by_type", "sort_by_size"}) {
        add(n, false)->setActionGroup(sortGroup);
    }
    actions.value(QStringLiteral("sort_by_text"))->setChecked(true);
    QAction *inGroups = add("show_in_groups", false);
    const ActionLookup lookup = [&actions](const QString &n) {
        return actions.value(n);
    };

    QMenu parent;
    QMenu *group = Raye::Menu::groupMenu(&parent, lookup);
    QVERIFY(group);
    QStringList names;
    for (QAction *a : group->actions()) {
        names << (a->isSeparator() ? QStringLiteral("-") : a->objectName());
    }
    QCOMPARE(names, ids({"group_none", "-", "group_text", "group_modificationtime", "group_type", "group_size"}));
    QVERIFY(group->actions().at(0)->isChecked()); // not grouped: (None)
    QCOMPARE(group->actions().at(0)->text(), QStringLiteral("(None)"));

    // Group by Type: sorts by type and groups.
    group->actions().at(4)->trigger();
    QVERIFY(actions.value(QStringLiteral("sort_by_type"))->isChecked());
    QVERIFY(inGroups->isChecked());
    // (None): grouping off, the sort stays.
    QMenu *again = Raye::Menu::groupMenu(&parent, lookup);
    QVERIFY(again->actions().at(4)->isChecked());
    again->actions().at(0)->trigger();
    QVERIFY(!inGroups->isChecked());
    QVERIFY(actions.value(QStringLiteral("sort_by_type"))->isChecked());

    // No such window actions: no menu.
    QVERIFY(!Raye::Menu::groupMenu(&parent, [](const QString &) {
        return static_cast<QAction *>(nullptr);
    }));
    QVERIFY(!Raye::Menu::viewMenu(&parent, lookup)); // no view_mode action here
}

void RayeContextMenuTest::openWithMenu()
{
    QMenu parent;
    int chose = 0;
    QMenu *menu = Raye::Menu::openWithMenu(&parent, {QUrl::fromLocalFile(QStringLiteral("/tmp/a.zzz"))}, QStringLiteral("application/x-raye-nothing"), [&chose] {
        ++chose;
    });
    QVERIFY(menu);
    // No app for the type: only "Choose another app".
    QCOMPARE(menu->actions().size(), 1);
    QCOMPARE(menu->actions().first()->objectName(), QStringLiteral("open_with_choose"));
    QCOMPARE(menu->actions().first()->text(), QStringLiteral("Choose another app"));
    menu->actions().first()->trigger();
    QCOMPARE(chose, 1);
    // Never more than four apps, and never Files itself.
    QMenu *text = Raye::Menu::openWithMenu(&parent, {QUrl::fromLocalFile(QStringLiteral("/tmp/a.txt"))}, QStringLiteral("text/plain"), {});
    int apps = 0;
    for (QAction *a : text->actions()) {
        if (a->objectName() == QLatin1String("open_with_app")) {
            ++apps;
            QVERIFY(!a->data().toString().startsWith(QLatin1String("org.kde.dolphin")));
        }
    }
    QVERIFY(apps <= openWithApps);
    QCOMPARE(text->actions().last()->objectName(), QStringLiteral("open_with_choose"));
}

void RayeContextMenuTest::chooseAnotherAppShowsTheDialog()
{
    // The menu above builds openWithMenu() with a callback of its own, so it
    // says nothing about what "Choose another app" does in a Files window.
    // Here the window builds the menu, and the item has to end with the
    // dialog on the screen.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("notes.txt"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    DolphinMainWindow window;
    window.openDirectories({QUrl::fromLocalFile(dir.path())}, false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    KFileItemActions actions;

    const KFileItem file(QUrl::fromLocalFile(path));
    DolphinContextMenu menu(&window, file, {file}, QUrl::fromLocalFile(dir.path()), &actions, DolphinContextMenu::Style::Compact);

    QAction *openWith = menu.item(QStringLiteral("open_with"));
    QVERIFY(openWith);
    QVERIFY(openWith->menu());
    QAction *choose = nullptr;
    for (QAction *a : openWith->menu()->actions()) {
        if (a->objectName() == QLatin1String("open_with_choose")) {
            choose = a;
        }
    }
    QVERIFY(choose);

    QVERIFY(openWithDialogs(&window).isEmpty());
    choose->trigger();

    // One dialog, for this file, and visible: building it is not enough.
    const QList<Raye::Ui::CardDialog *> dialogs = openWithDialogs(&window);
    QCOMPARE(dialogs.size(), 1);
    QVERIFY2(dialogs.first()->title().contains(QLatin1String("txt")), qPrintable(dialogs.first()->title()));
    QTRY_VERIFY(dialogs.first()->isVisible());
    // quickPage() hands back a view even when the page fails to load, so a
    // visible dialog can still be an empty frame: ask for the page itself.
    auto *page = qobject_cast<QQuickWidget *>(dialogs.first()->contentWidget());
    QVERIFY(page);
    QVERIFY(page->rootObject());
    dialogs.first()->close();
}

void RayeContextMenuTest::excludedPlugins()
{
    const QStringList out = Raye::Menu::excludedPlugins();
    QVERIFY(out.contains(QStringLiteral("compressfileitemaction")));
    QVERIFY(out.contains(QStringLiteral("extractfileitemaction")));
    QVERIFY(out.contains(QStringLiteral("kio-admin")));
}

void RayeContextMenuTest::adminUrl()
{
    QCOMPARE(Raye::Menu::adminUrl(QUrl::fromLocalFile(QStringLiteral("/etc/fstab"))), QUrl(QStringLiteral("admin:/etc/fstab")));
    QVERIFY(Raye::Menu::adminUrl(QUrl(QStringLiteral("smb://nas/x"))).isEmpty());
}

static QStringList visibleTexts(QMenu *menu)
{
    QStringList out;
    for (QAction *a : menu->actions()) {
        if (a->isVisible() && !a->isSeparator()) {
            out << a->text();
        }
    }
    return out;
}

void RayeContextMenuTest::filesWindowMenus()
{
    // The real thing: a Files window on a temporary folder.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    for (const char *name : {"notes.txt", "photo.png"}) {
        QFile f(dir.filePath(QString::fromLatin1(name)));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
    }
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("Work")));
    QApplication::clipboard()->clear();

    DolphinMainWindow window;
    window.openDirectories({QUrl::fromLocalFile(dir.path())}, false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    KFileItemActions actions;
    const QUrl base = QUrl::fromLocalFile(dir.path());

    const KFileItem file(QUrl::fromLocalFile(dir.filePath(QStringLiteral("notes.txt"))));
    DolphinContextMenu fileMenu(&window, file, {file}, base, &actions, DolphinContextMenu::Style::Compact);
    QCOMPARE(fileMenu.rowIds(), ids({"cut", "copy", "rename", "share", "delete"}));
    QCOMPARE(fileMenu.itemIds(),
             ids({"open", "open_with", "-", "favorite", "-", "compress", "-", "copy_path", "create_shortcut", "-", "properties", "-", "more_options"}));
    // Delete goes to the Recycle Bin here: it is the window's "Move to Trash".
    QAction *trash = window.actionCollection()->action(KStandardAction::name(KStandardAction::MoveToTrash));
    QVERIFY(trash);
    QCOMPARE(fileMenu.item(QStringLiteral("delete"))->text(), QStringLiteral("Delete"));
    QCOMPARE(fileMenu.item(QStringLiteral("delete"))->isEnabled(), trash->isEnabled());

    const KFileItem folder(QUrl::fromLocalFile(dir.filePath(QStringLiteral("Work"))));
    DolphinContextMenu folderMenu(&window, folder, {folder}, base, &actions, DolphinContextMenu::Style::Compact);
    QVERIFY(folderMenu.itemIds().contains(QStringLiteral("open_new_tab")));
    QVERIFY(folderMenu.itemIds().contains(QStringLiteral("pin_quick_access")));
    QVERIFY(!folderMenu.rowIds().contains(QStringLiteral("paste"))); // nothing on the clipboard

    // The background: View, Sort by, Group by from the window's actions.
    DolphinContextMenu background(&window, KFileItem(), {}, base, &actions, DolphinContextMenu::Style::Compact);
    QCOMPARE(background.itemIds().mid(0, 4), ids({"view", "sort", "group", "refresh"}));
    QVERIFY(background.itemIds().contains(QStringLiteral("new")));
    QCOMPARE(background.itemIds().last(), QStringLiteral("more_options"));

    // The full menu: Dolphin's list, without the hamburger menu nor Ark's
    // entries, and no Raye row.
    DolphinContextMenu full(&window, file, {file}, base, &actions, DolphinContextMenu::Style::Full);
    QVERIFY(full.rowIds().isEmpty());
    QVERIFY(full.actions().size() > 3);
    QAction *hamburger = window.actionCollection()->action(KStandardAction::name(KStandardAction::HamburgerMenu));
    QVERIFY(!hamburger || !full.actions().contains(hamburger));
    for (const QString &text : visibleTexts(&full)) {
        QVERIFY2(!text.contains(QLatin1String("Compress"), Qt::CaseInsensitive), qPrintable(text));
    }
}

QTEST_MAIN(RayeContextMenuTest)

#include "test_contextmenu.moc"
