/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"
#include "searchoptions.h"
#include "ui/flyout.h"

#include <KSharedConfig>

#include <QPointer>
#include <QUrl>
#include <QWidget>

#include <functional>

class QAction;
class QActionGroup;
class QLineEdit;
class QMenu;
class QTimer;

namespace Raye::Commands
{
class CommandBar;
}

namespace Raye::Search
{

// The search box at the end of the address row (explorer spec 1.2, 3.3):
// always there, "Search Documents" in it, a magnifier on the right. Typing
// searches the folder and its sub-folders 300 ms after the last key (Enter
// at once); Esc clears it and goes back to the folder. Its "Search options"
// (Kind, Date modified, Size, Search file contents, Current folder / All
// subfolders, Close search) join the command bar while a search shows.
// The last ten searches drop down under the empty box.
//
// The box does not open anything itself: the window opens what it asks
// for (searchRequested, filterRequested, searchClosed).
class DOLPHIN_EXPORT SearchBox : public QWidget
{
    Q_OBJECT
public:
    static constexpr int boxHeight = 32;
    static constexpr int glowMargin = 3;
    static constexpr int innerPadding = 10;
    static constexpr int focusStroke = 2;

    // Builds the URL that searches folder with these options. The window
    // sets Dolphin's (Search::urlForRayeSearch: Baloo where the folder is
    // indexed, filenamesearch elsewhere); fileNameSearchUrl() otherwise.
    using UrlBuilder = std::function<QUrl(const Options &options, const QUrl &folder)>;

    explicit SearchBox(QWidget *parent = nullptr);
    ~SearchBox() override;

    void setUrlBuilder(const UrlBuilder &builder);

    // The folder the box searches, and its name for the placeholder. A
    // search under way is closed when the folder changes from outside.
    void setFolder(const QUrl &folder, const QString &displayName);
    QUrl folder() const
    {
        return m_folder;
    }

    Options options() const
    {
        return m_options;
    }
    void setOptions(const Options &options);

    QLineEdit *lineEdit() const
    {
        return m_edit;
    }
    QString placeholder() const;

    // A search (or a Current folder filter) is showing.
    bool isSearching() const
    {
        return m_searching;
    }

    // dolphinrc [Raye] RecentSearches. Another file can stand in for
    // dolphinrc (the tests and the render harness).
    void setConfig(const KSharedConfig::Ptr &config);
    QStringList recentSearches() const;
    void clearRecentSearches();

    // The "Search options" content, kept up to date with the options.
    QMenu *optionsMenu() const
    {
        return m_optionsMenu;
    }
    // "Search options ⌄" and "Close search", for the command bar.
    QAction *optionsAction() const
    {
        return m_optionsAction;
    }
    QAction *closeAction() const
    {
        return m_closeAction;
    }
    // Adds them as the bar's "search" context set, shown while searching.
    void registerWith(Commands::CommandBar *bar);

    // The filter the view applies to results (kind, date, size), for the
    // search tools that do not.
    bool acceptsResult(const QMimeType &mime, bool isDir, const QDateTime &modified, qint64 size) const;

    // The recent searches drop-down, while it is open.
    Ui::Flyout *recentFlyout() const
    {
        return m_recentFlyout;
    }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public Q_SLOTS:
    // Ctrl+E, Ctrl+F, F3: the words are selected, recent searches show.
    void focusSearch();
    // Clears the box and goes back to the folder.
    void closeSearch();
    // Searches now, without waiting.
    void searchNow();

Q_SIGNALS:
    // Open this URL: the results of the search (All subfolders).
    void searchRequested(const QUrl &url);
    // Show the folder itself (leaving search results if any) and filter its
    // items (Current folder): names that contain the words, of these MIME
    // types (all when empty). Both empty: no filter.
    void filterRequested(const QString &nameFilter, const QStringList &mimeTypes);
    // The search is over: show the folder again, without a filter.
    void searchClosed();
    void optionsChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QRect boxRect() const;
    void textEdited();
    void run(bool remember);
    void buildOptionsMenu();
    void syncOptionsMenu();
    void changeOptions(const std::function<void(Options &)> &change);
    void showRecent();
    void pickRecent(const QString &term);
    void remember(const QString &term);
    KConfigGroup rayeGroup() const;

    QLineEdit *m_edit;
    QTimer *m_debounce;
    UrlBuilder m_builder;
    QUrl m_folder;
    QString m_folderName;
    Options m_options;
    bool m_searching = false;
    bool m_hovered = false;
    QMenu *m_optionsMenu;
    QAction *m_optionsAction;
    QAction *m_closeAction;
    QAction *m_contentsAction = nullptr;
    QActionGroup *m_scopeGroup = nullptr;
    QActionGroup *m_kindGroup = nullptr;
    QActionGroup *m_dateGroup = nullptr;
    QActionGroup *m_sizeGroup = nullptr;
    QPointer<Ui::Flyout> m_recentFlyout;
    KSharedConfig::Ptr m_config;
};

}
