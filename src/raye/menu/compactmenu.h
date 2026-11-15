/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"
#include "menuplan.h"

#include <QHash>
#include <QMenu>
#include <QPointer>

#include <functional>

class QWidgetAction;

namespace Raye::Menu
{

class IconRow;

// The right-click menu of Files (explorer spec 1.11, 3.11): a row of icon
// buttons with their names under them (Cut, Copy, Paste, Rename, Share,
// Delete), then the short list Windows shows, with "Show more options"
// last. Built from a Plan; what each item does comes from the hooks, so the
// window, the This PC page and the tests all use it. The menu itself is a
// plain QMenu: its look (acrylic, radius 8, 32 px items) is the Raye Qt
// style's.
class DOLPHIN_EXPORT CompactMenu : public QMenu
{
    Q_OBJECT
public:
    struct Hooks {
        // A window action that does this item (Cut, Copy, Rename,
        // Properties, Undo...). The menu shows Raye's words, follows the
        // action's enabled state and triggers it. Return nullptr for items
        // the caller handles in trigger().
        std::function<QAction *(const QString &id)> action;
        // Everything that is not a window action, submenu items included
        // ("compress_zip", "open_with_choose"...).
        std::function<void(const QString &id)> trigger;
        // The content of open_with, view, sort, group and new, made for this
        // menu (it is parented to it). nullptr leaves the item out, except
        // compress, which has its own content.
        std::function<QMenu *(const QString &id, QMenu *parent)> submenu;
        // Whether an item handled by trigger() can act now.
        std::function<bool(const QString &id)> enabled;
        // Whether the item exists here at all (no rotate service, no
        // terminal...): false leaves it out. Unset: every item exists.
        std::function<bool(const QString &id)> available;
        // Words that depend on the item ("Unpin from Quick access"); empty
        // keeps the usual words.
        std::function<QString(const QString &id)> text;
    };

    CompactMenu(const Plan &plan, const Hooks &hooks, QWidget *parent = nullptr);
    // An empty menu, filled later by build() (or not at all: the full menu
    // of the window is a plain list).
    explicit CompactMenu(QWidget *parent = nullptr);
    ~CompactMenu() override;

    // Fills the menu. Once.
    void build(const Plan &plan, const Hooks &hooks);

    // The usual words, icon name and shortcut hint of an item.
    static QString defaultText(const QString &id);
    static QString iconName(const QString &id);
    static QKeySequence shortcutHint(const QString &id);

    // What was built: the row's ids and the list's ids ("-" for separators,
    // submenu items by their own id), in order.
    QStringList rowIds() const;
    QStringList itemIds() const;
    // The action of an item or of a row button, by id.
    QAction *item(const QString &id) const;
    QWidget *iconRow() const;
    // Where the menu last opened (its top left corner, global).
    QPoint shownAt() const
    {
        return m_shownAt;
    }

protected:
    void showEvent(QShowEvent *event) override;

private:
    QAction *makeAction(const QString &id, QObject *owner);
    void addItem(const QString &id);
    QMenu *compressMenu();

    Hooks m_hooks;
    QHash<QString, QAction *> m_actions;
    QStringList m_rowIds;
    QStringList m_itemIds;
    QWidgetAction *m_rowAction = nullptr;
    IconRow *m_row = nullptr;
    QPoint m_shownAt;
};

}
