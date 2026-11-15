/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Raye: the Windows window of Files (explorer spec 3.2 to 3.8, FX-9). The
// Dolphin window keeps its actions, views and panels; this puts Raye's
// title row, address row, command bar, navigation pane and status bar
// around the tabs, and hides the menu bar, toolbar and dock panels.

#include "dolphinmainwindow.h"

#include "dolphin_generalsettings.h"

#include "dolphintabpage.h"
#include "dolphintabwidget.h"
#include "dolphinviewcontainer.h"
#include "panels/information/informationpanel.h"
#include "search/dolphinquery.h"
#include "views/dolphinview.h"
#include "views/draganddrophelper.h"
#include "views/dolphinnewfilemenuobserver.h"

#include "raye/addressbar/addressbar.h"
#include "raye/commandbar/commandbar.h"
#include "raye/commandbar/commandids.h"
#include "raye/navpane/navigationpane.h"
#include "raye/options/folderoptionsdialog.h"
#include "raye/pages/host.h"
#include "raye/preview/rightpane.h"
#include "raye/search/searchbox.h"
#include "raye/share/shareflyout.h"
#include "raye/titlebar/titlebar.h"
#include "raye/ui/carddialog.h"
#include "raye/viewdefaults/viewdefaults.h"
#include "raye/window/shell.h"
#include "raye/window/shortcuts.h"
#include "raye/window/statusbar.h"

#include <KActionCollection>
#include <KIO/SimpleJob>
#include <KIO/StoredTransferJob>
#include <KJobWidgets>
#include <KNewFileMenu>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KPropertiesDialog>

#include "raye/properties/propertiesdialog.h"
#include <KSharedConfig>
#include <KToggleAction>
#include <KToolBar>
#include <Solid/Device>
#include <Solid/StorageAccess>

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QPointer>
#include <QTimer>

#include <memory>
#include <QDockWidget>
#include <QMenuBar>
#include <QMimeData>
#include <QStandardPaths>

namespace
{
KConfigGroup rayeConfig()
{
    return KSharedConfig::openConfig(QStringLiteral("dolphinrc"))->group(QStringLiteral("Raye"));
}

QString quotedPaths(const KFileItemList &items, const QUrl &folder)
{
    QStringList paths;
    for (const KFileItem &item : items) {
        paths << QLatin1Char('"') + item.url().toDisplayString(QUrl::PreferLocalFile) + QLatin1Char('"');
    }
    if (paths.isEmpty() && folder.isValid()) {
        paths << QLatin1Char('"') + folder.toDisplayString(QUrl::PreferLocalFile) + QLatin1Char('"');
    }
    return paths.join(QLatin1Char('\n'));
}
}

void DolphinMainWindow::rayeCreateItem(const QString &baseName, const QString &suffix, const QByteArray &content)
{
    if (!m_activeViewContainer || m_activeViewContainer->isShowingPage()) {
        return;
    }
    const QUrl folder = m_activeViewContainer->url();
    QUrl target = folder.adjusted(QUrl::StripTrailingSlash);
    target.setPath(target.path() + QLatin1Char('/') + baseName + suffix);
    if (folder.isLocalFile()) {
        // "New Text Document (2).txt", like Windows.
        const QString dir = folder.toLocalFile();
        QString name = baseName + suffix;
        for (int n = 2; QFileInfo::exists(dir + QLatin1Char('/') + name); ++n) {
            name = QStringLiteral("%1 (%2)%3").arg(baseName).arg(n).arg(suffix);
        }
        target = QUrl::fromLocalFile(dir + QLatin1Char('/') + name);
    }
    KIO::StoredTransferJob *job = KIO::storedPut(content, target, -1, KIO::HideProgressInfo);
    KJobWidgets::setWindow(job, this);
    QPointer<DolphinMainWindow> self(this);
    connect(job, &KJob::result, this, [self, target](KJob *job) {
        if (!self || job->error()) {
            return;
        }
        Q_EMIT DolphinNewFileMenuObserver::instance().itemCreated(target);
        // Windows puts the new name in edit mode right away, once the view
        // has the item and has selected it.
        auto tries = std::make_shared<int>(0);
        auto *timer = new QTimer(self);
        timer->setInterval(150);
        QObject::connect(timer, &QTimer::timeout, self, [self, target, tries, timer] {
            DolphinView *view = self->m_activeViewContainer ? self->m_activeViewContainer->view() : nullptr;
            const KFileItemList selected = view ? view->selectedItems() : KFileItemList();
            if (selected.count() == 1 && selected.first().url() == target) {
                self->actionCollection()->action(QStringLiteral("renamefile"))->trigger();
            } else if (++*tries < 20) {
                return;
            }
            timer->deleteLater();
        });
        timer->start();
    });
}

void DolphinMainWindow::rayeAddActions()
{
    KActionCollection *ac = actionCollection();
    using namespace Raye::Commands;

    auto *nav = ac->add<KToggleAction>(QString::fromLatin1(Ids::navigationPane));
    nav->setText(i18ndc("raye-files", "@action:inmenu", "Navigation pane"));
    nav->setChecked(rayeConfig().readEntry("ShowNavigationPane", true));
    connect(nav, &QAction::toggled, this, [this](bool on) {
        if (m_rayeShell) {
            m_rayeShell->setNavigationPaneVisible(on);
        }
        KConfigGroup g = rayeConfig();
        g.writeEntry("ShowNavigationPane", on);
    });

    auto *details = ac->add<KToggleAction>(QString::fromLatin1(Ids::detailsPane));
    details->setText(i18ndc("raye-files", "@action:inmenu", "Details pane"));
    details->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
    auto *preview = ac->add<KToggleAction>(QString::fromLatin1(Ids::previewPane));
    preview->setText(i18ndc("raye-files", "@action:inmenu", "Preview pane"));
    const QString pane = rayeConfig().readEntry("PreviewPane", QStringLiteral("none"));
    details->setChecked(pane == QLatin1String("details"));
    preview->setChecked(pane == QLatin1String("preview"));
    // One pane at a time, like Windows.
    connect(details, &QAction::toggled, this, [this, preview](bool on) {
        if (on) {
            preview->setChecked(false);
        }
        rayeShowRightPane();
    });
    connect(preview, &QAction::toggled, this, [this, details](bool on) {
        if (on) {
            details->setChecked(false);
        }
        rayeShowRightPane();
    });

    auto *selectNone = ac->addAction(QString::fromLatin1(Ids::selectNone));
    selectNone->setText(i18ndc("raye-files", "@action:inmenu", "Select none"));
    selectNone->setIcon(QIcon::fromTheme(QStringLiteral("edit-select-none")));
    connect(selectNone, &QAction::triggered, this, [this] {
        if (m_activeViewContainer) {
            m_activeViewContainer->view()->clearSelection();
        }
    });

    auto *options = ac->addAction(QString::fromLatin1(Ids::folderOptions));
    options->setText(i18ndc("raye-files", "@action:inmenu", "Options"));
    options->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    connect(options, &QAction::triggered, this, [this] {
        auto *dialog = Raye::Options::folderOptionsDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->open();
    });

    auto *settings = Raye::ViewDefaults::Settings::instance();
    auto *extensions = ac->add<KToggleAction>(QString::fromLatin1(Ids::fileExtensions));
    extensions->setText(i18ndc("raye-files", "@action:inmenu", "File name extensions"));
    extensions->setChecked(settings->showExtensions());
    connect(extensions, &QAction::toggled, settings, &Raye::ViewDefaults::Settings::setShowExtensions);
    connect(settings, &Raye::ViewDefaults::Settings::showExtensionsChanged, extensions, &QAction::setChecked);

    auto *checkBoxes = ac->add<KToggleAction>(QString::fromLatin1(Ids::itemCheckBoxes));
    checkBoxes->setText(i18ndc("raye-files", "@action:inmenu", "Item check boxes"));
    checkBoxes->setChecked(settings->itemCheckBoxes());
    connect(checkBoxes, &QAction::toggled, settings, &Raye::ViewDefaults::Settings::setItemCheckBoxes);
    connect(settings, &Raye::ViewDefaults::Settings::itemCheckBoxesChanged, checkBoxes, &QAction::setChecked);

    auto *compact = ac->add<KToggleAction>(QString::fromLatin1(Ids::compactView));
    compact->setText(i18ndc("raye-files", "@action:inmenu", "Compact view"));
    compact->setChecked(settings->compact());
    connect(compact, &QAction::toggled, settings, &Raye::ViewDefaults::Settings::setCompact);
    connect(settings, &Raye::ViewDefaults::Settings::compactChanged, compact, &QAction::setChecked);

    auto *modeGroup = new QActionGroup(this);
    const QStringList modeNames = Raye::Window::viewModeActions();
    for (int i = 0; i < modeNames.size(); ++i) {
        auto *mode = ac->add<KToggleAction>(modeNames.at(i));
        mode->setText(Raye::ViewDefaults::modeId(Raye::ViewDefaults::Mode(i)));
        mode->setActionGroup(modeGroup);
        connect(mode, &QAction::triggered, this, [this, i] {
            if (m_activeViewContainer && !m_activeViewContainer->isShowingPage()) {
                m_activeViewContainer->view()->setRayeViewMode(Raye::ViewDefaults::Mode(i));
                rayeUpdateShell();
            }
        });
    }

    auto *newText = ac->addAction(QStringLiteral("raye_new_text"));
    newText->setText(i18ndc("raye-files", "@action:inmenu", "Text document"));
    newText->setIcon(QIcon::fromTheme(QStringLiteral("text-plain")));
    connect(newText, &QAction::triggered, this, [this] {
        rayeCreateItem(i18ndc("raye-files", "@item the name of a new file, before .txt", "New Text Document"), QStringLiteral(".txt"), QByteArray());
    });

    auto *newZip = ac->addAction(QString::fromLatin1(Ids::newZip));
    newZip->setText(i18ndc("raye-files", "@action:inmenu", "Compressed (zipped) folder"));
    connect(newZip, &QAction::triggered, this, [this] {
        // An empty ZIP is its end-of-directory record alone.
        QByteArray empty("PK\x05\x06", 4);
        empty.append(18, '\0');
        rayeCreateItem(i18ndc("raye-files", "@item the name of a new archive, before .zip", "New Compressed (zipped) Folder"), QStringLiteral(".zip"), empty);
    });

    auto *newShortcut = ac->addAction(QString::fromLatin1(Ids::newShortcut));
    newShortcut->setText(i18ndc("raye-files", "@action:inmenu", "Shortcut"));
    connect(newShortcut, &QAction::triggered, this, [this] {
        if (!m_activeViewContainer || m_activeViewContainer->isShowingPage() || !m_activeViewContainer->url().isLocalFile()) {
            return;
        }
        const QUrl item = QFileDialog::getOpenFileUrl(this, i18ndc("raye-files", "@title:window", "Choose what the shortcut opens"), QUrl::fromLocalFile(QDir::homePath()));
        if (!item.isLocalFile()) {
            return;
        }
        const QString dir = m_activeViewContainer->url().toLocalFile();
        const QString base = i18ndc("raye-files", "@item %1 is the name of what a shortcut opens", "%1 - Shortcut", item.fileName());
        QString name = base;
        for (int n = 2; QFileInfo::exists(dir + QLatin1Char('/') + name); ++n) {
            name = QStringLiteral("%1 (%2)").arg(base).arg(n);
        }
        KIO::SimpleJob *job = KIO::symlink(item.toLocalFile(), QUrl::fromLocalFile(dir + QLatin1Char('/') + name), KIO::HideProgressInfo);
        KJobWidgets::setWindow(job, this);
    });

    auto *search = ac->addAction(QStringLiteral("raye_focus_search"));
    search->setText(i18ndc("raye-files", "@action", "Search"));
    connect(search, &QAction::triggered, this, [this] {
        if (m_rayeShell) {
            m_rayeShell->searchBox()->focusSearch();
        }
    });

    auto *editLocation = ac->addAction(QStringLiteral("raye_edit_location"));
    editLocation->setText(i18ndc("raye-files", "@action", "Edit the address"));
    connect(editLocation, &QAction::triggered, this, [this] {
        if (m_rayeShell) {
            m_rayeShell->addressBar()->editLocation();
        }
    });

    auto *copyPath = ac->addAction(QStringLiteral("raye_copy_path"));
    copyPath->setText(i18ndc("raye-files", "@action:inmenu", "Copy as path"));
    copyPath->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy-path")));
    connect(copyPath, &QAction::triggered, this, [this] {
        if (m_activeViewContainer && !m_activeViewContainer->isShowingPage()) {
            QApplication::clipboard()->setText(quotedPaths(m_activeViewContainer->view()->selectedItems(), m_activeViewContainer->url()));
        }
    });

    auto *fullScreen = ac->addAction(QStringLiteral("raye_full_screen"));
    fullScreen->setText(i18ndc("raye-files", "@action:inmenu", "Full screen"));
    connect(fullScreen, &QAction::triggered, this, [this] {
        setWindowState(windowState() ^ Qt::WindowFullScreen);
    });
}

void DolphinMainWindow::rayeApplyShortcuts()
{
    KActionCollection *ac = actionCollection();
    for (const QString &name : Raye::Window::clearedShortcuts()) {
        if (QAction *a = ac->action(name)) {
            ac->setDefaultShortcuts(a, {});
            a->setShortcuts(QList<QKeySequence>{});
        }
    }
    for (const Raye::Window::Shortcut &s : Raye::Window::windowShortcuts()) {
        if (QAction *a = ac->action(s.action)) {
            ac->setDefaultShortcuts(a, s.keys);
        }
    }
}

void DolphinMainWindow::rayeSetupWindow()
{
    menuBar()->hide();
    for (KToolBar *bar : toolBars()) {
        bar->hide();
    }
    // The Information dock gives its pane to the shell; the others go.
    InformationPanel *info = findChild<InformationPanel *>();
    for (QDockWidget *dock : findChildren<QDockWidget *>()) {
        if (info && dock->widget() == info) {
            dock->setWidget(nullptr);
        }
        dock->hide();
        dock->toggleViewAction()->setEnabled(false);
    }

    m_rayeShell = new Raye::Window::Shell(this);
    takeCentralWidget();
    m_rayeShell->setContent(m_tabWidget);
    if (info) {
        m_rayeShell->setRightPane(info);
    }
    setCentralWidget(m_rayeShell);

    auto *title = m_rayeShell->titleBar();
    title->setTabBar(m_tabWidget->tabBar());
    connect(title, &Raye::Title::TitleBar::newTabRequested, this, [this] {
        m_tabWidget->openNewActivatedTab(Raye::Pages::PageHost::homeUrl());
    });

    // New ⌄ keeps only office templates from KDE's list; Raye adds Text
    // document, Shortcut and the compressed folder itself.
    m_newFileMenu->setSupportedMimeTypes({
        QStringLiteral("application/vnd.oasis.opendocument.text"),
        QStringLiteral("application/vnd.oasis.opendocument.spreadsheet"),
        QStringLiteral("application/vnd.oasis.opendocument.presentation"),
        QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document"),
        QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"),
        QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.presentation"),
    });

    KActionCollection *ac = actionCollection();
    auto *bar = m_rayeShell->commandBar();
    bar->setActionLookup([ac](const QString &name) {
        return ac->action(name);
    });
    bar->setShareHandler([](QWidget *anchor, const QList<QUrl> &urls) {
        Raye::Share::ShareFlyout::share(anchor, urls);
    });

    auto *search = m_rayeShell->searchBox();
    search->setUrlBuilder([](const Raye::Search::Options &options, const QUrl &folder) {
        return Search::urlForRayeSearch(options, folder);
    });
    search->registerWith(bar);
    connect(search, &Raye::Search::SearchBox::searchRequested, this, [this](const QUrl &url) {
        m_activeViewContainer->setUrl(url);
    });
    connect(search, &Raye::Search::SearchBox::filterRequested, this, [this](const QString &name, const QStringList &mimeTypes) {
        m_activeViewContainer->view()->setNameFilter(name);
        m_activeViewContainer->view()->setMimeTypeFilters(mimeTypes);
    });
    connect(search, &Raye::Search::SearchBox::searchClosed, this, [this] {
        DolphinView *view = m_activeViewContainer->view();
        view->setNameFilter(QString());
        view->setMimeTypeFilters({});
        if (isSearchUrl(m_activeViewContainer->url())) {
            m_activeViewContainer->setUrl(m_rayeShell->searchBox()->folder());
        }
    });

    auto *address = m_rayeShell->addressBar();
    connect(address, &Raye::Address::AddressBar::refreshRequested, this, [this] {
        if (!m_activeViewContainer->isShowingPage()) {
            m_activeViewContainer->reload();
        }
    });
    connect(address, &Raye::Address::AddressBar::urlsDropped, this, [this](const QUrl &destination, QDropEvent *event) {
        DragAndDropHelper::dropUrls(destination, event, this);
    });

    auto *nav = m_rayeShell->navigationPane();
    connect(nav, &Raye::NavPane::NavigationPane::urlActivated, this, [this](const QUrl &url) {
        m_activeViewContainer->setUrl(url);
    });
    connect(nav, &Raye::NavPane::NavigationPane::openInNewTabRequested, this, &DolphinMainWindow::openNewTabAndActivate);
    connect(nav, &Raye::NavPane::NavigationPane::openInNewWindowRequested, this, &DolphinMainWindow::openNewWindow);
    connect(nav, &Raye::NavPane::NavigationPane::propertiesRequested, this, [this](const QUrl &url) {
        Raye::Properties::showProperties({url}, this);
    });
    connect(nav, &Raye::NavPane::NavigationPane::ejectRequested, this, [](const QString &driveId, const QUrl &) {
        Solid::Device device(driveId);
        if (auto *access = device.as<Solid::StorageAccess>()) {
            access->teardown();
        }
    });
    connect(nav, &Raye::NavPane::NavigationPane::mountRequested, this, [this](const QString &driveId) {
        Solid::Device device(driveId);
        auto *access = device.as<Solid::StorageAccess>();
        if (!access) {
            return;
        }
        if (access->isAccessible() && !access->filePath().isEmpty()) {
            m_activeViewContainer->setUrl(QUrl::fromLocalFile(access->filePath()));
            return;
        }
        QPointer<DolphinMainWindow> self(this);
        connect(access, &Solid::StorageAccess::setupDone, this,
                [self, driveId](Solid::ErrorType error, const QVariant &errorData, const QString &udi) {
                    if (!self || udi != driveId) {
                        return;
                    }
                    if (error != Solid::NoError) {
                        const QString detail = errorData.toString();
                        KMessageBox::error(self, detail.isEmpty() ? i18nc("@info", "This drive could not be opened.") : detail);
                        return;
                    }
                    Solid::Device mounted(udi);
                    if (auto *done = mounted.as<Solid::StorageAccess>(); done && !done->filePath().isEmpty()) {
                        self->m_activeViewContainer->setUrl(QUrl::fromLocalFile(done->filePath()));
                    }
                },
                Qt::SingleShotConnection);
        access->setup();
    });
    m_rayeShell->setNavigationPaneVisible(ac->action(QString::fromLatin1(Raye::Commands::Ids::navigationPane))->isChecked());

    auto *status = m_rayeShell->statusBar();
    connect(status, &Raye::Window::StatusBar::detailsRequested, this, [this] {
        if (!m_activeViewContainer->isShowingPage()) {
            m_activeViewContainer->view()->setRayeViewMode(Raye::ViewDefaults::Mode::Details);
            rayeUpdateShell();
        }
    });
    connect(status, &Raye::Window::StatusBar::largeIconsRequested, this, [this] {
        if (!m_activeViewContainer->isShowingPage()) {
            m_activeViewContainer->view()->setRayeViewMode(Raye::ViewDefaults::Mode::LargeIcons);
            rayeUpdateShell();
        }
    });
    m_rayeShell->setStatusBarVisible(GeneralSettings::showStatusBar() != GeneralSettings::EnumShowStatusBar::Disabled);
    connect(GeneralSettings::self(), &GeneralSettings::showStatusBarChanged, this, [this] {
        m_rayeShell->setStatusBarVisible(GeneralSettings::showStatusBar() != GeneralSettings::EnumShowStatusBar::Disabled);
    });

    connect(m_tabWidget, &DolphinTabWidget::activeViewChanged, this, &DolphinMainWindow::rayeActiveViewChanged);
    connect(m_tabWidget, &DolphinTabWidget::currentUrlChanged, this, &DolphinMainWindow::rayeUpdateShell);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &DolphinMainWindow::rayeUpdateShell);
    if (m_activeViewContainer) {
        rayeActiveViewChanged(m_activeViewContainer);
    }
    rayeShowRightPane();
}

void DolphinMainWindow::rayeActiveViewChanged(DolphinViewContainer *container)
{
    if (!m_rayeShell || !container) {
        return;
    }
    for (const QMetaObject::Connection &c : std::as_const(m_rayeViewConnections)) {
        disconnect(c);
    }
    m_rayeViewConnections.clear();
    m_rayeShell->addressBar()->setNavigator(container->urlNavigatorInternalWithHistory());
    DolphinView *view = container->view();
    const auto update = [this] {
        rayeUpdateShell();
    };
    m_rayeViewConnections << connect(view, &DolphinView::selectionChanged, this, update);
    m_rayeViewConnections << connect(view, &DolphinView::itemCountChanged, this, update);
    m_rayeViewConnections << connect(view, &DolphinView::directoryLoadingCompleted, this, update);
    m_rayeViewConnections << connect(view, &DolphinView::modeChanged, this, update);
    m_rayeViewConnections << connect(view, &DolphinView::zoomLevelChanged, this, update);
    m_rayeViewConnections << connect(view, &DolphinView::urlChanged, this, update);
    m_rayeViewConnections << connect(view, &DolphinView::directoryLoadingStarted, this, [this] {
        m_rayeShell->addressBar()->setLoading(true);
    });
    m_rayeViewConnections << connect(view, &DolphinView::directoryLoadingProgress, this, [this](int percent) {
        m_rayeShell->addressBar()->setLoadingProgress(percent);
    });
    m_rayeViewConnections << connect(view, &DolphinView::directoryLoadingCompleted, this, [this] {
        m_rayeShell->addressBar()->setLoading(false);
    });
    m_rayeViewConnections << connect(view, &DolphinView::directoryLoadingCanceled, this, [this] {
        m_rayeShell->addressBar()->setLoading(false);
    });
    m_rayeViewConnections << connect(container, &DolphinViewContainer::captionChanged, this, update);
    rayeUpdateShell();
}

void DolphinMainWindow::rayeUpdateShell()
{
    if (!m_rayeShell || !m_activeViewContainer) {
        return;
    }
    DolphinViewContainer *container = m_activeViewContainer;
    const QUrl url = container->url();
    const bool page = container->isShowingPage();
    DolphinView *view = container->view();

    m_rayeShell->navigationPane()->setCurrentUrl(url);
    if (!isSearchUrl(url)) {
        // A page searches what it shows: Home the account's folder, Gallery
        // its pictures; the box still says "Search Home".
        QUrl searchFolder = url;
        if (page) {
            const QString name = Raye::Pages::PageHost::pageName(url);
            searchFolder = QUrl::fromLocalFile(name == QLatin1String("gallery") ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                                                                                : QDir::homePath());
        }
        m_rayeShell->searchBox()->setFolder(searchFolder, container->caption());
    }

    Raye::Commands::CommandContext context;
    context.url = url;
    context.searching = isSearchUrl(url);
    context.writable = !page && view->isFolderWritable();
    Raye::Window::StatusCounts counts;
    if (!page) {
        const KFileItemList selection = view->selectedItems();
        for (const KFileItem &item : selection) {
            context.selection << item.url();
            context.selectionMimeTypes << item.mimetype();
            if (!item.isDir()) {
                ++counts.selectedFiles;
                counts.selectedBytes += item.size();
            }
        }
        counts.items = view->itemsCount();
        counts.selected = selection.count();
    }
    m_rayeShell->commandBar()->setContext(context);

    auto *status = m_rayeShell->statusBar();
    if (page) {
        status->clear();
        status->setMode(Raye::Window::StatusBar::Mode::Other);
    } else {
        status->setCounts(counts);
        const QStringList modeNames = Raye::Window::viewModeActions();
        const int current = int(view->rayeViewMode());
        if (current >= 0 && current < modeNames.size()) {
            if (QAction *a = actionCollection()->action(modeNames.at(current))) {
                a->setChecked(true);
            }
        }
        using Mode = Raye::Window::StatusBar::Mode;
        Mode mode = Mode::Other;
        if (view->rayeViewMode() == Raye::ViewDefaults::Mode::Details) {
            mode = Mode::Details;
        } else if (view->rayeViewMode() == Raye::ViewDefaults::Mode::LargeIcons) {
            mode = Mode::LargeIcons;
        }
        status->setMode(mode);
    }
}

void DolphinMainWindow::rayeShowRightPane()
{
    if (!m_rayeShell) {
        return;
    }
    KActionCollection *ac = actionCollection();
    const bool details = ac->action(QString::fromLatin1(Raye::Commands::Ids::detailsPane))->isChecked();
    const bool preview = ac->action(QString::fromLatin1(Raye::Commands::Ids::previewPane))->isChecked();
    if (auto *info = qobject_cast<InformationPanel *>(m_rayeShell->rightPane())) {
        if (Raye::Preview::RightPane *pane = info->rightPane()) {
            pane->setMode(preview ? Raye::Preview::RightPane::Preview : Raye::Preview::RightPane::Details);
        }
    }
    m_rayeShell->setRightPaneVisible(details || preview);
    KConfigGroup g = rayeConfig();
    g.writeEntry("PreviewPane", preview ? QStringLiteral("preview") : details ? QStringLiteral("details") : QStringLiteral("none"));
}
