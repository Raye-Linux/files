/*
 * SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz19@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DOLPHINCONTEXTMENU_H
#define DOLPHINCONTEXTMENU_H

#include <KFileCopyToMenu>
#include <KFileItem>
#include <KFileItemActions>

#include <QMenu>
#include <QUrl>

// Raye: the compact Windows menu, with Dolphin's menu as "Show more options".
#include "menu/compactmenu.h"

class QAction;
class DolphinMainWindow;
class KFileItemListProperties;
class DolphinRemoveAction;

/**
 * @brief Represents the context menu which appears when doing a right
 *        click on an item or the viewport of the file manager.
 *
 * Beside static menu entries (e. g. 'Paste' or 'Properties') two
 * dynamic sub menus are shown when opening a context menu above
 * an item:
 * - 'Open With': Contains all applications which are registered to
 *                open items of the given MIME type.
 * - 'Actions':   Contains all actions which can be applied to the
 *                given item.
 */
class DolphinContextMenu : public Raye::Menu::CompactMenu
{
    Q_OBJECT

public:
    // Raye: Compact is the Windows menu (icon row, short list, "Show more
    // options" last). Full is Dolphin's long menu, restyled: it opens from
    // "Show more options", and at once with Shift+right click or Shift+F10.
    enum class Style {
        Automatic, // Compact, or Full when Shift is held
        Compact,
        Full,
    };

    /**
     * @parent        Pointer to the main window the context menu
     *                belongs to.
     * @fileInfo      Pointer to the file item the context menu
     *                is applied. If 0 is passed, the context menu
     *                is above the viewport.
     * @selectedItems The selected items for which the context menu
     *                is opened. This list generally includes \a fileInfo.
     * @baseUrl       Base URL of the viewport where the context menu
     *                should be opened.
     */
    DolphinContextMenu(DolphinMainWindow *parent,
                       const KFileItem &fileInfo,
                       const KFileItemList &selectedItems,
                       const QUrl &baseUrl,
                       KFileItemActions *fileItemActions,
                       Style style = Style::Automatic);

    ~DolphinContextMenu() override;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    // Raye: the compact menu, and what its items do.
    void addCompactActions();
    Raye::Menu::Selection rayeSelection();
    QAction *rayeWindowAction(const QString &id);
    void rayeTrigger(const QString &id);
    QMenu *rayeSubmenu(const QString &id, QMenu *parent);
    bool rayeAvailable(const QString &id);
    QString rayeText(const QString &id);
    QList<QUrl> selectedUrls() const;
    void openSelection();
    void openFullMenu();

    /**
     * Adds all the actions and menus to this menu based on all given information.
     * This method calls the other helper methods for adding actions
     * based on the context given in the constructor.
     */
    void addAllActions();

    void addTrashContextMenu();
    void addTrashItemContextMenu();
    void addItemContextMenu();
    void addViewportContextMenu();

    void insertDefaultItemActions(const KFileItemListProperties &);

    bool placeExists(const QUrl &url) const;

    QAction *createPasteAction();

    KFileItemListProperties &selectedItemsProperties() const;

    /**
     * Returns the file item for m_baseUrl.
     */
    KFileItem baseFileItem();

    /**
     * Adds "Open With" actions
     */
    void addOpenWithActions();

    /**
     * Add services, custom actions, plugins and version control items to the menu
     */
    void addAdditionalActions(const KFileItemListProperties &props);

private:
    void addDirectoryItemContextMenu();
    void addOpenParentFolderActions();
    void configureTrash();

    struct Entry {
        int type;
        QString name;
        QString filePath; // empty for separator
        QString templatePath; // same as filePath for template
        QString icon;
        QString comment;
    };

    enum ContextType {
        NoContext = 0,
        ItemContext = 1,
        TrashContext = 2,
        TimelineContext = 4,
        SearchContext = 8,
        RecentlyUsedContext = 16,
    };

    DolphinMainWindow *m_mainWindow;

    KFileItem m_fileInfo;

    QUrl m_baseUrl;
    KFileItem *m_baseFileItem; /// File item for m_baseUrl

    KFileItemList m_selectedItems;
    mutable KFileItemListProperties *m_selectedItemsProperties;

    int m_context;
    KFileCopyToMenu m_copyToMenu;

    DolphinRemoveAction *m_removeAction; // Action that represents either 'Move To Trash' or 'Delete'
    KFileItemActions *m_fileItemActions;

    // Raye
    bool m_full = false;
};

#endif
