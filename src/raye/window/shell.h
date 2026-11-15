/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QPointer>
#include <QWidget>

class QSplitter;
class QVBoxLayout;

namespace Raye::Title
{
class TitleBar;
}
namespace Raye::Address
{
class AddressBar;
}
namespace Raye::Search
{
class SearchBox;
}
namespace Raye::Commands
{
class CommandBar;
}
namespace Raye::NavPane
{
class NavigationPane;
}

namespace Raye::Window
{
class StatusBar;

// The window of Files put together (explorer spec 3.2 to 3.8): the title
// row with the tabs, the address row with the search box, the command bar,
// then the navigation pane, the folder views and the right pane side by
// side, and the status bar. The main window gives it the views (Dolphin's
// tab widget) and the right pane, and wires the pieces to its actions.
class DOLPHIN_EXPORT Shell : public QWidget
{
    Q_OBJECT

public:
    static constexpr int navMinimum = 160;
    static constexpr int navMaximum = 480;
    static constexpr int rightMinimum = 240;
    static constexpr int rightMaximum = 480;
    static constexpr int rightDefault = 320;

    explicit Shell(QWidget *parent = nullptr);
    ~Shell() override;

    Title::TitleBar *titleBar() const
    {
        return m_title;
    }
    Address::AddressBar *addressBar() const
    {
        return m_address;
    }
    Search::SearchBox *searchBox() const
    {
        return m_search;
    }
    Commands::CommandBar *commandBar() const
    {
        return m_commands;
    }
    NavPane::NavigationPane *navigationPane() const
    {
        return m_nav;
    }
    StatusBar *statusBar() const
    {
        return m_status;
    }

    // The folder views, between the panes.
    void setContent(QWidget *content);
    QWidget *content() const
    {
        return m_content;
    }

    // The Details or Preview pane, on the right.
    void setRightPane(QWidget *pane);
    QWidget *rightPane() const
    {
        return m_right;
    }

    void setNavigationPaneVisible(bool visible);
    bool isNavigationPaneVisible() const;
    void setRightPaneVisible(bool visible);
    bool isRightPaneVisible() const;
    void setStatusBarVisible(bool visible);

    QSplitter *splitter() const
    {
        return m_splitter;
    }

    // dolphinrc [Raye] NavPaneWidth and the right pane's width.
    void readSizes();
    void saveSizes() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applySizes();

    Title::TitleBar *m_title;
    QWidget *m_chrome;
    Address::AddressBar *m_address;
    Search::SearchBox *m_search;
    Commands::CommandBar *m_commands;
    QSplitter *m_splitter;
    NavPane::NavigationPane *m_nav;
    QPointer<QWidget> m_content;
    QPointer<QWidget> m_right;
    StatusBar *m_status;
    int m_navWidth = 240;
    int m_rightWidth = rightDefault;
};
}
