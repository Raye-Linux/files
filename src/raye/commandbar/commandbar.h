/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include "context.h"
#include "rules.h"

#include <QHash>
#include <QPointer>
#include <QWidget>

#include <functional>

class QAction;
class QMenu;

namespace Raye::Ui
{
class CommandButton;
class Separator;
}

namespace Raye::Commands
{

class PasteGlow;

// The command bar of Files (explorer spec 1.3, 3.4), the window's only
// toolbar: New ⌄ | Cut Copy Paste Rename Share Delete | Sort ⌄ View ⌄ |
// context buttons | "…" and, at the right end, Details. 48 tall, buttons 32,
// icon buttons 40 wide, 1 x 24 separators.
//
// It is built from the window's actions, found by name (commandids.h), but
// shows Raye's words: the buttons trigger Dolphin's actions and follow their
// enabled and checked state, plus the rules of rules.h. A narrow window
// folds the right end of the row into "…".
class DOLPHIN_EXPORT CommandBar : public QWidget
{
    Q_OBJECT
public:
    static constexpr int rowHeight = 48;
    static constexpr int margin = 8;
    static constexpr int buttonGap = 2;
    static constexpr int separatorMargin = 4;

    using ActionLookup = std::function<QAction *(const QString &name)>;
    // Opens the share flyout for the selection, anchored to the button.
    using ShareHandler = std::function<void(QWidget *anchor, const QList<QUrl> &urls)>;

    explicit CommandBar(QWidget *parent = nullptr);
    ~CommandBar() override;

    // Usually [collection](const QString &name) { return collection->action(name); }.
    void setActionLookup(const ActionLookup &lookup);
    QAction *sourceAction(const QString &name) const;

    void setContext(const CommandContext &context);
    const CommandContext &context() const
    {
        return m_context;
    }

    // Context sets. A set registered again under the same id replaces it.
    // The actions stay the caller's.
    void registerSet(const QString &id, const QList<QAction *> &actions, const Predicate &predicate, Placement placement = Placement::Join);
    bool unregisterSet(const QString &id);
    QStringList setIds() const;

    void setShareHandler(const ShareHandler &handler);

    // The buttons by id: "new", "cut", "copy", "paste", "rename", "share",
    // "delete", "sort", "view", "seemore", "details", and context buttons by
    // their action's object name.
    QWidget *button(const QString &id) const;
    QStringList visibleIds() const;
    QStringList foldedIds() const;

    // Opens "…" under its button.
    void showSeeMore();

    // The menus, filled as they would be on opening.
    QMenu *seeMoreMenu();
    QMenu *sortMenu();
    QMenu *viewMenu();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct Entry {
        QString id;
        Ui::CommandButton *button = nullptr;
        Command command = Command::Other;
        int group = 0;
        int priority = 0;
        bool fixed = false; // "…" and Details never fold
        QPointer<QAction> contextAction; // a context set's own action
    };

    void createButtons();
    Ui::CommandButton *makeButton(const QString &id, const QString &text, const char *icon, bool iconOnly);
    QAction *deleteSource() const;
    void triggerSource(const QString &name);
    void watchSources();
    void scheduleUpdate();
    void updateStates();
    void rebuildContextButtons();
    void relayout();
    void clipboardChanged();

    void fillSeeMore(QMenu *menu);
    void fillSort(QMenu *menu);
    void fillView(QMenu *menu);
    void fillNewExtras();
    QAction *proxy(QMenu *menu, const char *name, const QString &text, const char *icon = nullptr);
    void addFoldedEntry(QMenu *menu, const Entry &entry);

    ActionLookup m_lookup;
    CommandContext m_context;
    State m_state;
    QList<ContextSet> m_sets;
    QList<Entry> m_entries; // in display order, context buttons included
    QList<Entry> m_contextEntries;
    QList<Ui::Separator *> m_separators;
    QHash<QString, bool> m_visible;
    ShareHandler m_share;
    QMenu *m_seeMore;
    QMenu *m_sort;
    QMenu *m_view;
    QList<QPointer<QAction>> m_newExtras;
    QPointer<QMenu> m_newMenu;
    QList<QPointer<QAction>> m_watched;
    PasteGlow *m_pasteGlow = nullptr;
    bool m_updateQueued = false;
    bool m_replaceEditing = false;
};

}
