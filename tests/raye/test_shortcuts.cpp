/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The keys of the window: File Explorer's, each key on one action only,
// and none left on an action that is cleared.

#include "window/shortcuts.h"

#include <QHash>
#include <QTest>

using Raye::Window::Shortcut;

class ShortcutsTest : public QObject
{
    Q_OBJECT

private:
    static QHash<QString, QList<QKeySequence>> table()
    {
        QHash<QString, QList<QKeySequence>> t;
        for (const Shortcut &s : Raye::Window::windowShortcuts()) {
            t.insert(s.action, s.keys);
        }
        return t;
    }

private Q_SLOTS:
    void windowsKeys_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("action");
        QTest::newRow("search ctrl+e") << "Ctrl+E" << "raye_focus_search";
        QTest::newRow("search ctrl+f") << "Ctrl+F" << "raye_focus_search";
        QTest::newRow("search f3") << "F3" << "raye_focus_search";
        QTest::newRow("address alt+d") << "Alt+D" << "raye_edit_location";
        QTest::newRow("address ctrl+l") << "Ctrl+L" << "raye_edit_location";
        QTest::newRow("address f4") << "F4" << "raye_edit_location";
        QTest::newRow("details pane") << "Alt+Shift+P" << "raye_details_pane";
        QTest::newRow("preview pane") << "Alt+P" << "raye_preview_pane";
        QTest::newRow("new folder") << "Ctrl+Shift+N" << "create_dir";
        QTest::newRow("copy as path") << "Ctrl+Shift+C" << "raye_copy_path";
        QTest::newRow("full screen") << "F11" << "raye_full_screen";
        QTest::newRow("extra large icons") << "Ctrl+Shift+1" << "raye_mode_extralarge";
        QTest::newRow("details view") << "Ctrl+Shift+6" << "raye_mode_details";
        QTest::newRow("content view") << "Ctrl+Shift+8" << "raye_mode_content";
        QTest::newRow("first tab") << "Ctrl+1" << "activate_tab_0";
        QTest::newRow("ninth tab") << "Ctrl+9" << "activate_tab_8";
    }

    void windowsKeys()
    {
        QFETCH(QString, key);
        QFETCH(QString, action);
        const QKeySequence seq(key);
        QString owner;
        const auto t = table();
        for (auto it = t.cbegin(); it != t.cend(); ++it) {
            if (it.value().contains(seq)) {
                owner = it.key();
            }
        }
        QCOMPARE(owner, action);
    }

    void noKeyTwice()
    {
        QHash<QKeySequence, QString> seen;
        for (const Shortcut &s : Raye::Window::windowShortcuts()) {
            for (const QKeySequence &k : s.keys) {
                QVERIFY2(!seen.contains(k), qPrintable(k.toString() + QLatin1String(" on ") + seen.value(k) + QLatin1String(" and ") + s.action));
                seen.insert(k, s.action);
            }
        }
    }

    void clearedActionsGetNoKey()
    {
        const auto t = table();
        for (const QString &name : Raye::Window::clearedShortcuts()) {
            QVERIFY2(!t.contains(name), qPrintable(name));
        }
    }
};

QTEST_GUILESS_MAIN(ShortcutsTest)

#include "test_shortcuts.moc"
