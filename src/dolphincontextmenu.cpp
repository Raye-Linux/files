/*
 * SPDX-FileCopyrightText: 2006 Peter Penz (peter.penz@gmx.at) and Cvetoslav Ludmiloff
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphincontextmenu.h"
#include "raye/jobs/adminaccess.h"

#include "dolphin_contextmenusettings.h"
#include "dolphin_generalsettings.h"
#include "dolphinmainwindow.h"
#include "dolphinnewfilemenu.h"
#include "dolphinplacesmodelsingleton.h"
#include "dolphinremoveaction.h"
#include "dolphinviewcontainer.h"
#include "global.h"
#include "settings/dolphinsettingsdialog.h"
#include "trash/dolphintrash.h"
#include "views/dolphinview.h"

// Raye
#include "archive/archiveformat.h"
#include "archive/archivewizardmodels.h"
#include "archive/archivewizards.h"
#include "menu/menuactions.h"
#include "menu/menuplan.h"
#include "netdrive/mapdrivedialog.h"
#include "openwith/openwithdialog.h"
#include "pages/homemodel.h"
#include "ui/carddialog.h"

#include <KActionCollection>
#include <KFileItemListProperties>
#include <KHamburgerMenu>
#include <KIO/EmptyTrashJob>
#include <KIO/ApplicationLauncherJob>
#include <KIO/OpenUrlJob>
#include <KIO/JobUiDelegateFactory>
#include <KIO/JobUiDelegate>
#include <KIO/Paste>
#include <KIO/RestoreJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNewFileMenu>
#include <KStandardAction>

#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QKeyEvent>
#include <QAction>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>

DolphinContextMenu::DolphinContextMenu(DolphinMainWindow *parent,
                                       const KFileItem &fileInfo,
                                       const KFileItemList &selectedItems,
                                       const QUrl &baseUrl,
                                       KFileItemActions *fileItemActions,
                                       Style style)
    : Raye::Menu::CompactMenu(parent)
    , m_mainWindow(parent)
    , m_fileInfo(fileInfo)
    , m_baseUrl(baseUrl)
    , m_baseFileItem(nullptr)
    , m_selectedItems(selectedItems)
    , m_selectedItemsProperties(nullptr)
    , m_context(NoContext)
    , m_copyToMenu(parent)
    , m_removeAction(nullptr)
    , m_fileItemActions(fileItemActions)
{
    QApplication::instance()->installEventFilter(this);

    // Raye: Shift+right click and Shift+F10 open Dolphin's full menu at once,
    // like Windows opens its old menu.
    m_full = style == Style::Full || (style == Style::Automatic && (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier));
    if (m_full) {
        addAllActions();
    } else {
        addCompactActions();
    }
}

DolphinContextMenu::~DolphinContextMenu()
{
    delete m_baseFileItem;
    m_baseFileItem = nullptr;
    delete m_selectedItemsProperties;
    m_selectedItemsProperties = nullptr;
}

void DolphinContextMenu::addAllActions()
{
    // Raye: no hamburger menu on top: Files has no menu bar it stands in for.

    // get the context information
    const auto scheme = m_baseUrl.scheme();
    if (scheme == QLatin1String("trash")) {
        m_context |= TrashContext;
    } else if (scheme.contains(QLatin1String("search"))) {
        m_context |= SearchContext;
    } else if (scheme.contains(QLatin1String("timeline"))) {
        m_context |= TimelineContext;
    } else if (scheme == QStringLiteral("recentlyused")) {
        m_context |= RecentlyUsedContext;
    }

    if (!m_fileInfo.isNull() && !m_selectedItems.isEmpty()) {
        m_context |= ItemContext;
        // TODO: handle other use cases like devices + desktop files
    }

    // open the corresponding popup for the context
    if (m_context & TrashContext) {
        if (m_context & ItemContext) {
            addTrashItemContextMenu();
        } else {
            addTrashContextMenu();
        }
    } else if (m_context & ItemContext) {
        addItemContextMenu();
    } else {
        addViewportContextMenu();
    }
}

bool DolphinContextMenu::eventFilter(QObject *object, QEvent *event)
{
    Q_UNUSED(object)

    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (m_removeAction && keyEvent->key() == Qt::Key_Shift) {
            if (event->type() == QEvent::KeyPress) {
                m_removeAction->update(DolphinRemoveAction::ShiftState::Pressed);
            } else {
                m_removeAction->update(DolphinRemoveAction::ShiftState::Released);
            }
        }
    }

    return false;
}

void DolphinContextMenu::addTrashContextMenu()
{
    Q_ASSERT(m_context & TrashContext);

    QAction *emptyTrashAction = addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action:inmenu", "Empty Trash"), this, [this]() {
        Trash::empty(m_mainWindow);
    });
    emptyTrashAction->setEnabled(!Trash::isEmpty());

    connect(&Trash::instance(), &Trash::emptinessChanged, this, [emptyTrashAction](bool isEmpty) {
        emptyTrashAction->setEnabled(!isEmpty);
    });

    // Insert 'Sort By' and 'View Mode'
    if (ContextMenuSettings::showSortBy() || ContextMenuSettings::showViewMode()) {
        addSeparator();
    }
    if (ContextMenuSettings::showSortBy()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("sort")));
    }
    if (ContextMenuSettings::showViewMode()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("view_mode")));
    }

    addSeparator();

    auto *configureTrashAction = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18nc("@action:inmenu", "Configure Trash…"), this);
    connect(configureTrashAction, &QAction::triggered, this, &DolphinContextMenu::configureTrash);
    addAction(configureTrashAction);
}

void DolphinContextMenu::configureTrash()
{
    DolphinSettingsDialog *settingsDialog = new DolphinSettingsDialog(m_baseUrl, m_mainWindow);
    settingsDialog->setCurrentPage(settingsDialog->trashSettings);
    settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
    settingsDialog->show();
}

void DolphinContextMenu::addTrashItemContextMenu()
{
    Q_ASSERT(m_context & TrashContext);
    Q_ASSERT(m_context & ItemContext);

    addAction(QIcon::fromTheme(QStringLiteral("edit-reset")),
              i18ncp("@action:inmenu Restore the selected files that are in the trash to the place they lived at the moment they were trashed. Minimize the "
                     "length of this string if possible.",
                     "Restore to Former Location",
                     "Restore to Former Locations",
                     m_selectedItems.count()),
              this,
              [this]() {
                  QList<QUrl> selectedUrls;
                  selectedUrls.reserve(m_selectedItems.count());
                  for (const KFileItem &item : std::as_const(m_selectedItems)) {
                      selectedUrls.append(item.url());
                  }

                  KIO::RestoreJob *job = KIO::restoreFromTrash(selectedUrls);
                  KJobWidgets::setWindow(job, m_mainWindow);
                  job->uiDelegate()->setAutoErrorHandlingEnabled(true);
              });

    addSeparator();

    addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Cut)));
    addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));

    addSeparator();

    QAction *deleteAction = m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::DeleteFile));
    addAction(deleteAction);

    addSeparator();

    addAction(m_mainWindow->actionCollection()->action(QStringLiteral("properties")));
}

void DolphinContextMenu::addDirectoryItemContextMenu()
{
    // insert 'Open in new window' and 'Open in new tab' entries
    const KFileItemListProperties &selectedItemsProps = selectedItemsProperties();
    if (ContextMenuSettings::showOpenInNewTab()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("open_in_new_tab")));
    }
    if (ContextMenuSettings::showOpenInNewWindow()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("open_in_new_window")));
    }

    if (ContextMenuSettings::showOpenInSplitView()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("open_in_split_view")));
    }

    // Insert 'Open With' entries
    addOpenWithActions();

    // set up 'Create New' menu
    QAction *newDirAction = m_mainWindow->actionCollection()->action(QStringLiteral("create_dir"));
    QAction *newFileAction = m_mainWindow->actionCollection()->action(QStringLiteral("create_file"));
    // Do not parent this to the menu, it has to outlive it. It is deleted manually below once a file has been created.
    DolphinNewFileMenu *newFileMenu = new DolphinNewFileMenu(newDirAction, newFileAction, m_mainWindow);
    newFileMenu->checkUpToDate();
    newFileMenu->setWorkingDirectory(m_fileInfo.url());
    newFileMenu->setEnabled(selectedItemsProps.supportsWriting());
    connect(newFileMenu, &DolphinNewFileMenu::fileCreated, newFileMenu, &DolphinNewFileMenu::deleteLater);
    connect(newFileMenu, &DolphinNewFileMenu::fileCreationRejected, newFileMenu, &DolphinNewFileMenu::deleteLater);
    connect(newFileMenu, &DolphinNewFileMenu::directoryCreated, newFileMenu, &DolphinNewFileMenu::deleteLater);
    connect(newFileMenu, &DolphinNewFileMenu::directoryCreationRejected, newFileMenu, &DolphinNewFileMenu::deleteLater);

    QMenu *menu = newFileMenu->menu();
    menu->setTitle(i18nc("@title:menu Create new folder, file, link, etc.", "Create New"));
    menu->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    addMenu(menu);

    addSeparator();
}

void DolphinContextMenu::addOpenParentFolderActions()
{
    addAction(QIcon::fromTheme(QStringLiteral("document-open-folder")), i18nc("@action:inmenu", "Open Path"), [this]() {
        const QUrl url = m_fileInfo.targetUrl();
        const QUrl parentUrl = KIO::upUrl(url);
        m_mainWindow->changeUrl(parentUrl);
        m_mainWindow->activeViewContainer()->view()->markUrlsAsSelected({url});
        m_mainWindow->activeViewContainer()->view()->markUrlAsCurrent(url);
    });

    addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "Open Path in New Tab"), [this]() {
        const QUrl url = m_fileInfo.targetUrl();
        const QUrl parentUrl = KIO::upUrl(url);
        DolphinTabPage *tabPage = m_mainWindow->openNewTab(parentUrl);
        tabPage->activeViewContainer()->view()->markUrlsAsSelected({url});
        tabPage->activeViewContainer()->view()->markUrlAsCurrent(url);
    });

    addAction(QIcon::fromTheme(QStringLiteral("window-new")), i18nc("@action:inmenu", "Open Path in New Window"), [this]() {
        Dolphin::openNewWindow({m_fileInfo.targetUrl()}, m_mainWindow, Dolphin::OpenNewWindowFlag::Select);
    });
}

void DolphinContextMenu::addItemContextMenu()
{
    Q_ASSERT(!m_fileInfo.isNull());

    const KFileItemListProperties &selectedItemsProps = selectedItemsProperties();

    m_fileItemActions->setItemListProperties(selectedItemsProps);

    if (m_selectedItems.count() == 1) {
        // single files
        if (m_fileInfo.isDir()) {
            addDirectoryItemContextMenu();
        } else if (m_context & TimelineContext || m_context & SearchContext || m_context & RecentlyUsedContext) {
            addOpenWithActions();

            addOpenParentFolderActions();

            addSeparator();
        } else {
            // Insert 'Open With" entries
            addOpenWithActions();
        }
        if (m_fileInfo.isLink()) {
            addAction(m_mainWindow->actionCollection()->action(QStringLiteral("show_target")));
            addSeparator();
        }
    } else {
        // multiple files
        bool selectionHasOnlyDirs = true;
        for (const auto &item : std::as_const(m_selectedItems)) {
            const QUrl &url = DolphinView::openItemAsFolderUrl(item);
            if (url.isEmpty()) {
                selectionHasOnlyDirs = false;
                break;
            }
        }

        if (selectionHasOnlyDirs && ContextMenuSettings::showOpenInNewTab()) {
            // insert 'Open in new tab' entry
            addAction(m_mainWindow->actionCollection()->action(QStringLiteral("open_in_new_tabs")));
        }
        // Insert 'Open With" entries
        addOpenWithActions();
    }

    insertDefaultItemActions(selectedItemsProps);

    addAdditionalActions(selectedItemsProps);

    // insert 'Copy To' and 'Move To' sub menus
    if (ContextMenuSettings::showCopyMoveMenu()) {
        m_copyToMenu.setUrls(m_selectedItems.urlList());
        m_copyToMenu.setReadOnly(!selectedItemsProps.supportsWriting());
        m_copyToMenu.setAutoErrorHandlingEnabled(true);
        m_copyToMenu.addActionsTo(this);
    }

    if (m_mainWindow->isSplitViewEnabledInCurrentTab()) {
        if (ContextMenuSettings::showCopyToOtherSplitView()) {
            addAction(m_mainWindow->actionCollection()->action(QStringLiteral("copy_to_inactive_split_view")));
        }

        if (ContextMenuSettings::showMoveToOtherSplitView()) {
            addAction(m_mainWindow->actionCollection()->action(QStringLiteral("move_to_inactive_split_view")));
        }
    }

    // insert 'Properties...' entry
    addSeparator();
    QAction *propertiesAction = m_mainWindow->actionCollection()->action(QStringLiteral("properties"));
    addAction(propertiesAction);
}

void DolphinContextMenu::addViewportContextMenu()
{
    const KFileItemListProperties baseUrlProperties(KFileItemList() << baseFileItem());
    m_fileItemActions->setItemListProperties(baseUrlProperties);

    // Set up and insert 'Create New' menu
    KNewFileMenu *newFileMenu = m_mainWindow->newFileMenu();
    newFileMenu->checkUpToDate();
    newFileMenu->setWorkingDirectory(m_baseUrl);
    addMenu(newFileMenu->menu());

    // Show "open with" menu items even if the dir is empty, because there are legitimate
    // use cases for this, such as opening an empty dir in Kate or VSCode or something
    addOpenWithActions();

    QAction *pasteAction = createPasteAction();
    if (pasteAction) {
        addAction(pasteAction);
    }

    // Insert 'Add to Places' entry if it's not already in the places panel
    if (ContextMenuSettings::showAddToPlaces() && !placeExists(m_mainWindow->activeViewContainer()->url())) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("add_to_places")));
    }
    addSeparator();

    // Insert 'Sort By' and 'View Mode'
    if (ContextMenuSettings::showSortBy()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("sort")));
    }
    if (ContextMenuSettings::showViewMode()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("view_mode")));
    }
    if (ContextMenuSettings::showSortBy() || ContextMenuSettings::showViewMode()) {
        addSeparator();
    }

    addAdditionalActions(baseUrlProperties);

    addSeparator();

    QAction *propertiesAction = m_mainWindow->actionCollection()->action(QStringLiteral("properties"));
    addAction(propertiesAction);
}

void DolphinContextMenu::insertDefaultItemActions(const KFileItemListProperties &properties)
{
    const KActionCollection *collection = m_mainWindow->actionCollection();

    // Insert 'Cut', 'Copy', 'Copy Location' and 'Paste'
    addAction(collection->action(KStandardAction::name(KStandardAction::Cut)));
    addAction(collection->action(KStandardAction::name(KStandardAction::Copy)));
    if (ContextMenuSettings::showCopyLocation()) {
        QAction *copyPathAction = collection->action(QStringLiteral("copy_location"));
        copyPathAction->setEnabled(m_selectedItems.size() == 1);
        addAction(copyPathAction);
    }
    QAction *pasteAction = createPasteAction();
    if (pasteAction) {
        addAction(pasteAction);
    }

    // Insert 'Duplicate Here'
    if (ContextMenuSettings::showDuplicateHere()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("duplicate")));
    }

    // Insert 'Rename'
    addAction(collection->action(KStandardAction::name(KStandardAction::RenameFile)));

    // Insert 'Add to Places' entry if appropriate
    if (ContextMenuSettings::showAddToPlaces() && m_selectedItems.count() == 1 && m_fileInfo.isDir() && !placeExists(m_fileInfo.url())) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("add_to_places")));
    }

    addSeparator();

    // Insert 'Move to Trash' and/or 'Delete'
    const bool showDeleteAction = (KSharedConfig::openConfig()->group(QStringLiteral("KDE")).readEntry("ShowDeleteCommand", false) || !properties.isLocal());
    const bool showMoveToTrashAction = (properties.isLocal() && properties.supportsMoving());

    if (showDeleteAction && showMoveToTrashAction) {
        delete m_removeAction;
        m_removeAction = nullptr;
        addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::MoveToTrash)));
        addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::DeleteFile)));
    } else if (showDeleteAction && !showMoveToTrashAction) {
        addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::DeleteFile)));
    } else {
        if (!m_removeAction) {
            m_removeAction = new DolphinRemoveAction(this, m_mainWindow->actionCollection());
        }
        addAction(m_removeAction);
        m_removeAction->update();
    }
}

bool DolphinContextMenu::placeExists(const QUrl &url) const
{
    const KFilePlacesModel *placesModel = DolphinPlacesModelSingleton::instance().placesModel();

    QModelIndex url_index = placesModel->closestItem(url);
    return url_index.isValid() && placesModel->url(url_index).matches(url, QUrl::StripTrailingSlash);
}

QAction *DolphinContextMenu::createPasteAction()
{
    QAction *action = nullptr;
    KFileItem destItem;
    if (!m_fileInfo.isNull() && m_selectedItems.count() <= 1) {
        destItem = m_fileInfo;
    } else {
        destItem = baseFileItem();
    }

    if (!destItem.isNull() && destItem.isDir()) {
        const QMimeData *mimeData = QApplication::clipboard()->mimeData();
        bool canPaste;
        const QString text = KIO::pasteActionText(mimeData, &canPaste, destItem);
        if (canPaste) {
            if (destItem == m_fileInfo) {
                // if paste destination is a selected folder
                action = new QAction(QIcon::fromTheme(QStringLiteral("edit-paste")), text, this);
                connect(action, &QAction::triggered, m_mainWindow, &DolphinMainWindow::pasteIntoFolder);
            } else {
                action = m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Paste));
            }
        }
    }

    return action;
}

KFileItemListProperties &DolphinContextMenu::selectedItemsProperties() const
{
    if (!m_selectedItemsProperties) {
        m_selectedItemsProperties = new KFileItemListProperties(m_selectedItems);
    }
    return *m_selectedItemsProperties;
}

KFileItem DolphinContextMenu::baseFileItem()
{
    if (!m_baseFileItem) {
        const DolphinView *view = m_mainWindow->activeViewContainer()->view();
        KFileItem baseItem = view->rootItem();
        if (baseItem.isNull() || baseItem.url() != m_baseUrl) {
            m_baseFileItem = new KFileItem(m_baseUrl);
        } else {
            m_baseFileItem = new KFileItem(baseItem);
        }
    }
    return *m_baseFileItem;
}

void DolphinContextMenu::addOpenWithActions()
{
    // insert 'Open With...' action or sub menu
    m_fileItemActions->insertOpenWithActionsTo(nullptr, this, QStringList{qApp->desktopFileName()});

    // For a single file, hint in "Open with" menu that middle-clicking would open it in the secondary app.
    // (Unless middle-clicking would open it as a folder in a new tab (e.g. archives).)
    const QUrl &url = DolphinView::openItemAsFolderUrl(m_fileInfo, GeneralSettings::browseThroughArchives());
    if (m_selectedItems.count() == 1 && url.isEmpty()) {
        if (QAction *openWithSubMenu = findChild<QAction *>(QStringLiteral("openWith_submenu"))) {
            Q_ASSERT(openWithSubMenu->menu());
            Q_ASSERT(!openWithSubMenu->menu()->isEmpty());

            auto *secondaryApp = openWithSubMenu->menu()->actions().first();
            // Add it like a keyboard shortcut, Qt uses \t as a separator.
            if (!secondaryApp->text().contains(QLatin1Char('\t'))) {
                secondaryApp->setText(secondaryApp->text() + QLatin1Char('\t')
                                      + i18nc("@action:inmenu Shortcut, middle click to trigger menu item, keep short", "Middle Click"));
            }
        }
    }
}

void DolphinContextMenu::addAdditionalActions(const KFileItemListProperties &props)
{
    addSeparator();

    QList<QAction *> additionalActions;
    if (props.isLocal() && ContextMenuSettings::showOpenTerminal()) {
        additionalActions << m_mainWindow->actionCollection()->action(QStringLiteral("open_terminal_here"));
    }
    // Raye: Files compresses and extracts by itself (Ark's entries are left
    // out), and "Open as administrator" comes back in Raye's words.
    m_fileItemActions->addActionsTo(this, KFileItemActions::MenuActionSource::All, additionalActions, Raye::Menu::excludedPlugins());
    if (props.isLocal() && Raye::Menu::adminAvailable()) {
        const KFileItemList items = props.items();
        const bool oneFolder = items.count() == 1 && items.first().isDir();
        const QUrl target = oneFolder ? items.first().url() : (items.isEmpty() ? m_baseUrl : QUrl());
        if (!target.isEmpty() || (items.count() == 1 && !items.first().isDir())) {
            QAction *admin = addAction(QIcon::fromTheme(Raye::Menu::CompactMenu::iconName(QStringLiteral("open_admin"))),
                                       Raye::Menu::CompactMenu::defaultText(QStringLiteral("open_admin")));
            admin->setObjectName(QStringLiteral("open_admin"));
            const QUrl item = items.count() == 1 ? items.first().url() : m_baseUrl;
            const bool folder = oneFolder || items.isEmpty();
            connect(admin, &QAction::triggered, this, [this, item, folder]() {
                const QUrl url = Raye::Menu::adminUrl(item);
                if (url.isEmpty()) {
                    return;
                }
                if (folder) {
                    m_mainWindow->changeUrl(url);
                } else {
                    auto *job = new KIO::OpenUrlJob(url);
                    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_mainWindow));
                    job->start();
                }
            });
        }
    }

    const DolphinView *view = m_mainWindow->activeViewContainer()->view();
    const QList<QAction *> versionControlActions = view->versionControlActions(m_selectedItems);
    if (!versionControlActions.isEmpty()) {
        addSeparator();
        addActions(versionControlActions);
        addSeparator();
    }
}

// Raye: the compact menu. The table of what shows for which selection is
// Raye::Menu::plan() (src/raye/menu); this part says what each item does in
// a Files window.

QList<QUrl> DolphinContextMenu::selectedUrls() const
{
    return m_selectedItems.urlList();
}

Raye::Menu::Selection DolphinContextMenu::rayeSelection()
{
    Raye::Menu::Selection selection;
    const QString scheme = m_baseUrl.scheme();
    if (scheme == QLatin1String("trash")) {
        selection.place = Raye::Menu::Place::RecycleBin;
    } else if (scheme.contains(QLatin1String("search")) || scheme.contains(QLatin1String("timeline")) || scheme == QLatin1String("recentlyused")) {
        selection.place = Raye::Menu::Place::Search;
    }
    selection.background = m_fileInfo.isNull() || m_selectedItems.isEmpty();

    const KFileItem base = baseFileItem();
    if (selection.background) {
        selection.local = base.isLocalFile() || !base.localPath().isEmpty();
        // Raye: in a folder that needs admin, Paste and New ask for it.
        selection.writable = Raye::Jobs::AdminAccess::allowsWrite(base.isWritable(), selection.local, Raye::Jobs::AdminAccess::available());
    } else {
        for (const KFileItem &item : std::as_const(m_selectedItems)) {
            const QString local = item.localPath();
            const bool exec = !local.isEmpty() && !item.isDir() && QFileInfo(local).isExecutable();
            Raye::Menu::ItemKind kind = Raye::Menu::kindOf(item.mimetype(), item.isDir(), item.name(), exec);
            // A ZIP saved without its extension is still an archive: the
            // content says so (an Office file is not: it has its own type).
            if (kind == Raye::Menu::ItemKind::File && m_selectedItems.count() == 1 && !local.isEmpty()
                && item.mimetype() == QLatin1String("application/octet-stream")
                && !Raye::Archive::detectFormat(local).isEmpty()) {
                kind = Raye::Menu::ItemKind::Archive;
            }
            selection.items.append(kind);
        }
        selection.writable = base.isWritable() && selectedItemsProperties().supportsWriting();
        selection.local = selectedItemsProperties().isLocal();
        if (m_selectedItems.count() == 1) {
            const QUrl url = m_fileInfo.targetUrl();
            selection.networkShare = url.scheme() == QLatin1String("smb") && !url.host().isEmpty()
                && url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts).size() <= 1;
            const QString local = m_fileInfo.localPath();
            selection.networkDrive = !local.isEmpty() && !Raye::NetDrives::mappingAt(local).mountPoint.isEmpty();
        }
    }

    // Files on the clipboard that can go into this folder (or the folder
    // that was right-clicked).
    KFileItem destination = (!selection.background && m_selectedItems.count() == 1 && m_fileInfo.isDir()) ? m_fileInfo : base;
    if (!destination.isNull() && destination.isDir()) {
        bool canPaste = false;
        KIO::pasteActionText(QApplication::clipboard()->mimeData(), &canPaste, destination);
        selection.clipboardHasFiles = canPaste;
    }
    return selection;
}

QAction *DolphinContextMenu::rayeWindowAction(const QString &id)
{
    const KActionCollection *collection = m_mainWindow->actionCollection();
    auto named = [collection](const QString &name) {
        return collection->action(name);
    };
    const bool background = m_fileInfo.isNull() || m_selectedItems.isEmpty();
    if (id == QLatin1String("cut")) {
        return named(KStandardAction::name(KStandardAction::Cut));
    }
    if (id == QLatin1String("copy")) {
        return named(KStandardAction::name(KStandardAction::Copy));
    }
    if (id == QLatin1String("rename")) {
        return named(KStandardAction::name(KStandardAction::RenameFile));
    }
    if (id == QLatin1String("paste")) {
        // Into the folder itself; a right-clicked folder is pasted into by
        // pasteIntoFolder() (rayeTrigger).
        return background ? named(KStandardAction::name(KStandardAction::Paste)) : nullptr;
    }
    if (id == QLatin1String("delete")) {
        if (m_baseUrl.scheme() == QLatin1String("trash")) {
            return named(KStandardAction::name(KStandardAction::DeleteFile));
        }
        const KFileItemListProperties &props = selectedItemsProperties();
        // To the Recycle Bin when it can go there; Shift+Delete still asks
        // to delete for good (the Raye delete dialog).
        if (props.isLocal() && props.supportsMoving()) {
            return named(KStandardAction::name(KStandardAction::MoveToTrash));
        }
        return named(KStandardAction::name(KStandardAction::DeleteFile));
    }
    if (id == QLatin1String("properties")) {
        return named(QStringLiteral("properties"));
    }
    if (id == QLatin1String("undo")) {
        return named(KStandardAction::name(KStandardAction::Undo));
    }
    if (id == QLatin1String("refresh")) {
        return named(KStandardAction::name(KStandardAction::Redisplay));
    }
    if (id == QLatin1String("open_new_tab")) {
        return named(m_selectedItems.count() > 1 ? QStringLiteral("open_in_new_tabs") : QStringLiteral("open_in_new_tab"));
    }
    if (id == QLatin1String("open_new_window")) {
        return named(QStringLiteral("open_in_new_window"));
    }
    if (id == QLatin1String("open_terminal")) {
        return named(background ? QStringLiteral("open_terminal") : QStringLiteral("open_terminal_here"));
    }
    return nullptr;
}

bool DolphinContextMenu::rayeAvailable(const QString &id)
{
    if (id == QLatin1String("rotate_left")) {
        return Raye::Menu::canRotate(false);
    }
    if (id == QLatin1String("rotate_right")) {
        return Raye::Menu::canRotate(true);
    }
    if (id == QLatin1String("open_terminal") || id == QLatin1String("undo") || id == QLatin1String("refresh") || id == QLatin1String("properties")) {
        return rayeWindowAction(id) != nullptr;
    }
    if (id == QLatin1String("open_new_window")) {
        // Several folders open as tabs; one window per folder is not offered.
        return m_selectedItems.count() <= 1;
    }
    if (id == QLatin1String("set_background")) {
        return !QStandardPaths::findExecutable(QStringLiteral("plasma-apply-wallpaperimage")).isEmpty();
    }
    return true;
}

QString DolphinContextMenu::rayeText(const QString &id)
{
    if (m_selectedItems.count() != 1) {
        return QString();
    }
    const QUrl url = m_fileInfo.url();
    if (id == QLatin1String("pin_quick_access") && Raye::Pages::isPinnedToQuickAccess(url)) {
        return Raye::Menu::CompactMenu::defaultText(QStringLiteral("unpin_quick_access"));
    }
    if (id == QLatin1String("favorite") && Raye::Pages::isFavorite(url)) {
        return Raye::Menu::CompactMenu::defaultText(QStringLiteral("unfavorite"));
    }
    if (id == QLatin1String("extract_to") && url.isLocalFile()) {
        const QString folder = QFileInfo(Raye::Archive::ExtractAllModel::defaultDestination(url.toLocalFile())).fileName();
        return i18ndc("raye-files", "@action:inmenu %1 is a folder name", "Extract to “%1/”", folder);
    }
    return QString();
}

QMenu *DolphinContextMenu::rayeSubmenu(const QString &id, QMenu *parent)
{
    const KActionCollection *collection = m_mainWindow->actionCollection();
    const Raye::Menu::ActionLookup lookup = [collection](const QString &name) {
        return collection->action(name);
    };
    if (id == QLatin1String("view")) {
        return Raye::Menu::viewMenu(parent, lookup);
    }
    if (id == QLatin1String("sort")) {
        return Raye::Menu::sortMenu(parent, lookup);
    }
    if (id == QLatin1String("group")) {
        return Raye::Menu::groupMenu(parent, lookup);
    }
    if (id == QLatin1String("new")) {
        KNewFileMenu *newFileMenu = m_mainWindow->newFileMenu();
        newFileMenu->checkUpToDate();
        newFileMenu->setWorkingDirectory(Raye::Jobs::AdminAccess::newItemFolder(m_baseUrl));
        if (!newFileMenu->menu() || newFileMenu->menu()->isEmpty()) {
            return nullptr;
        }
        auto *menu = new QMenu(parent);
        menu->addActions(newFileMenu->menu()->actions());
        return menu;
    }
    if (id == QLatin1String("open_with")) {
        const QString mime = selectedItemsProperties().mimeType();
        const QList<QUrl> urls = selectedUrls();
        QPointer<DolphinMainWindow> window = m_mainWindow;
        return Raye::Menu::openWithMenu(parent, urls, mime, [window, urls, mime]() {
            if (!window) {
                return;
            }
            // openWithDialog() hands the dialog over for the caller to put on
            // the screen, the way configureTrash() does with the settings one.
            Raye::Ui::CardDialog *dialog = Raye::OpenWith::openWithDialog(window, urls, mime, [urls](const KService::Ptr &service, bool) {
                if (service) {
                    auto *job = new KIO::ApplicationLauncherJob(service);
                    job->setUrls(urls);
                    job->start();
                }
            });
            dialog->show();
        });
    }
    return nullptr;
}

void DolphinContextMenu::openSelection()
{
    if (m_selectedItems.isEmpty()) {
        return;
    }
    if (m_selectedItems.count() == 1) {
        const KFileItem &item = m_selectedItems.first();
        // Folders, and archives Files opens as folders, open here.
        const QUrl asFolder = DolphinView::openItemAsFolderUrl(item, GeneralSettings::browseThroughArchives());
        if (!asFolder.isEmpty()) {
            m_mainWindow->changeUrl(asFolder);
            return;
        }
        auto *job = new KIO::OpenUrlJob(item.targetUrl(), item.mimetype());
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_mainWindow));
        job->setShowOpenOrExecuteDialog(true);
        job->start();
        return;
    }
    // Several: folders in new tabs, files in their apps, like a double click on each.
    KFileItemList files;
    for (const KFileItem &item : std::as_const(m_selectedItems)) {
        const QUrl asFolder = DolphinView::openItemAsFolderUrl(item, GeneralSettings::browseThroughArchives());
        if (!asFolder.isEmpty()) {
            m_mainWindow->openNewTab(asFolder);
        } else {
            files.append(item);
        }
    }
    if (!files.isEmpty()) {
        m_fileItemActions->runPreferredApplications(files);
    }
}

void DolphinContextMenu::openFullMenu()
{
    // "Show more options": Dolphin's whole menu, where this one was.
    QPointer<DolphinMainWindow> window = m_mainWindow;
    const KFileItem item = m_fileInfo;
    const KFileItemList items = m_selectedItems;
    const QUrl base = m_baseUrl;
    KFileItemActions *actions = m_fileItemActions;
    const QPoint at = shownAt();
    QTimer::singleShot(0, window, [window, item, items, base, actions, at]() {
        if (!window) {
            return;
        }
        auto *full = new DolphinContextMenu(window, item, items, base, actions, Style::Full);
        full->setAttribute(Qt::WA_DeleteOnClose);
        full->popup(at);
    });
}

void DolphinContextMenu::rayeTrigger(const QString &id)
{
    const QList<QUrl> urls = selectedUrls();
    const QUrl single = m_selectedItems.count() == 1 ? m_fileInfo.url() : QUrl();
    QWidget *window = m_mainWindow;

    if (id == QLatin1String("open")) {
        openSelection();
    } else if (id == QLatin1String("map_drive")) {
        QPointer<DolphinMainWindow> main(m_mainWindow);
        Raye::NetDrives::showMapDriveDialog(m_fileInfo.targetUrl(), window, [main](const QUrl &drive) {
            if (main) {
                main->changeUrl(drive);
            }
        });
    } else if (id == QLatin1String("disconnect_drive")) {
        QPointer<QWidget> guard(window);
        Raye::NetDrives::disconnectDrive(m_fileInfo.localPath(), m_mainWindow, [guard](Raye::NetDrives::Failure failure, const QString &text) {
            if (failure != Raye::NetDrives::Failure::None && guard) {
                KMessageBox::error(guard, text);
            }
        });
    } else if (id == QLatin1String("paste")) {
        m_mainWindow->pasteIntoFolder();
    } else if (id == QLatin1String("share")) {
        QStringList mimeTypes;
        for (const KFileItem &item : std::as_const(m_selectedItems)) {
            mimeTypes.append(item.mimetype());
        }
        Raye::Menu::shareAt(shownAt(), urls, mimeTypes);
    } else if (id == QLatin1String("open_file_location")) {
        const QUrl target = m_fileInfo.targetUrl();
        m_mainWindow->changeUrl(KIO::upUrl(target));
        m_mainWindow->activeViewContainer()->view()->markUrlsAsSelected({target});
        m_mainWindow->activeViewContainer()->view()->markUrlAsCurrent(target);
    } else if (id == QLatin1String("pin_quick_access") && !single.isEmpty()) {
        if (Raye::Pages::isPinnedToQuickAccess(single)) {
            Raye::Pages::unpinFromQuickAccess(single);
        } else {
            Raye::Pages::pinToQuickAccess(single);
        }
    } else if (id == QLatin1String("favorite")) {
        const bool remove = !single.isEmpty() && Raye::Pages::isFavorite(single);
        for (const QUrl &url : urls) {
            if (remove) {
                Raye::Pages::removeFromFavorites(url);
            } else {
                Raye::Pages::addToFavorites(url);
            }
        }
    } else if (id.startsWith(QLatin1String("compress_"))) {
        QStringList paths;
        for (const QUrl &url : urls) {
            if (url.isLocalFile()) {
                paths.append(url.toLocalFile());
            }
        }
        if (paths.isEmpty()) {
            return;
        }
        if (id == QLatin1String("compress_options")) {
            Raye::Archive::Wizards::compressWithOptions(window, paths);
            return;
        }
        const Raye::Archive::Format format = id == QLatin1String("compress_7z") ? Raye::Archive::Format::SevenZip
            : id == QLatin1String("compress_tar")                              ? Raye::Archive::Format::Tar
                                                                                : Raye::Archive::Format::Zip;
        QPointer<DolphinMainWindow> mainWindow = m_mainWindow;
        Raye::Archive::Wizards::compressNow(window, paths, format, [mainWindow](const QString &created) {
            // The new archive is selected, ready to be renamed.
            if (mainWindow && mainWindow->activeViewContainer()) {
                const QUrl url = QUrl::fromLocalFile(created);
                mainWindow->activeViewContainer()->view()->markUrlsAsSelected({url});
                mainWindow->activeViewContainer()->view()->markUrlAsCurrent(url);
            }
        });
    } else if (id == QLatin1String("extract_all") && single.isLocalFile()) {
        Raye::Archive::Wizards::extractAll(window, single.toLocalFile());
    } else if (id == QLatin1String("extract_here") && single.isLocalFile()) {
        Raye::Archive::Wizards::extractHere(window, single.toLocalFile());
    } else if (id == QLatin1String("extract_to") && single.isLocalFile()) {
        Raye::Archive::Wizards::extractToFolder(window, single.toLocalFile());
    } else if (id == QLatin1String("set_background") && !single.isEmpty()) {
        Raye::Menu::setAsDesktopBackground(single);
    } else if (id == QLatin1String("rotate_left") || id == QLatin1String("rotate_right")) {
        Raye::Menu::rotate(single, id == QLatin1String("rotate_right"));
    } else if (id == QLatin1String("copy_path")) {
        Raye::Menu::copyAsPath(urls);
    } else if (id == QLatin1String("create_shortcut")) {
        Raye::Menu::createShortcuts(urls, window);
    } else if (id == QLatin1String("restore")) {
        KIO::RestoreJob *job = KIO::restoreFromTrash(urls);
        KJobWidgets::setWindow(job, m_mainWindow);
        job->uiDelegate()->setAutoErrorHandlingEnabled(true);
    } else if (id == QLatin1String("empty_bin")) {
        Trash::empty(m_mainWindow);
    } else if (id == QLatin1String("more_options")) {
        openFullMenu();
    }
}

void DolphinContextMenu::addCompactActions()
{
    // The selection mode bar reads this menu's actions and drops the first
    // one (Dolphin's hamburger menu): it gets a hidden stand-in.
    if (QAction *hamburger = m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::HamburgerMenu))) {
        auto *standIn = new QAction(hamburger->icon(), QString(), this);
        standIn->setVisible(false);
        addAction(standIn);
    }

    const Raye::Menu::Selection selection = rayeSelection();
    if (!selection.background) {
        m_fileItemActions->setItemListProperties(selectedItemsProperties());
    }
    if (selection.place == Raye::Menu::Place::RecycleBin && selection.background) {
        // Empty Recycle Bin follows the bin.
        connect(&Trash::instance(), &Trash::emptinessChanged, this, [this](bool isEmpty) {
            if (QAction *empty = item(QStringLiteral("empty_bin"))) {
                empty->setEnabled(!isEmpty);
            }
        });
    }

    Raye::Menu::CompactMenu::Hooks hooks;
    hooks.action = [this](const QString &id) {
        return rayeWindowAction(id);
    };
    hooks.trigger = [this](const QString &id) {
        rayeTrigger(id);
    };
    hooks.submenu = [this](const QString &id, QMenu *parent) {
        return rayeSubmenu(id, parent);
    };
    hooks.available = [this](const QString &id) {
        return rayeAvailable(id);
    };
    hooks.text = [this](const QString &id) {
        return rayeText(id);
    };
    hooks.enabled = [](const QString &id) {
        if (id == QLatin1String("empty_bin")) {
            return !Trash::isEmpty();
        }
        return true;
    };
    build(Raye::Menu::plan(selection), hooks);
}

#include "moc_dolphincontextmenu.cpp"
