/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include "locations/locationnames.h"

#include <QPointer>
#include <QUrl>
#include <QWidget>

class KUrlNavigator;
class QAction;
class QDropEvent;
class QHBoxLayout;
class QMenu;
class QTimer;

namespace Raye::Ui
{
class CommandButton;
}

namespace Raye::Address
{

class AddressBox;
class Splitter;

// The address row of Files (explorer spec 3.3), 48 tall with 8 px margins:
// Back, Forward, Up and Refresh (32 x 32), the address box, then a 4 px
// splitter and whatever sits at the end of the row (the search box).
//
// History and location live in a KUrlNavigator, the one Dolphin's view
// container keeps for its history (urlNavigatorInternalWithHistory()):
// Dolphin's own Back and Forward actions and this row then never disagree.
// Without one the row keeps its own.
class DOLPHIN_EXPORT AddressBar : public QWidget
{
    Q_OBJECT
public:
    static constexpr int rowHeight = 48;
    static constexpr int historyEntries = 10;
    static constexpr int trailingDefault = 240;
    static constexpr int trailingMinimum = 160;
    static constexpr int trailingMaximum = 480;

    enum Direction {
        Back,
        Forward,
    };

    explicit AddressBar(QWidget *parent = nullptr);
    ~AddressBar() override;

    void setNavigator(KUrlNavigator *navigator);
    KUrlNavigator *navigator() const;

    void setLocationNames(const Locations::LocationNames &names);

    QUrl url() const;
    // Goes there: a new step in the history.
    void setUrl(const QUrl &url);

    // Alt+D, Ctrl+L and F4: the address box as a text field. The action is
    // added to the row's window; the main window may add it to its own
    // action collection instead.
    QAction *editLocationAction() const
    {
        return m_editAction;
    }
    void editLocation();

    void setLoading(bool loading);
    void setLoadingProgress(int percent);

    // The widget at the end of the row (the search box), and its width.
    void setTrailingWidget(QWidget *widget);
    QWidget *trailingWidget() const
    {
        return m_trailing;
    }
    int trailingWidth() const
    {
        return m_trailingWidth;
    }
    void setTrailingWidth(int width);

    // The history of one direction, nearest first, at most ten; choosing an
    // entry goes there. The caller owns the menu.
    QMenu *historyMenu(Direction direction);

    AddressBox *addressBox() const
    {
        return m_box;
    }
    Ui::CommandButton *backButton() const
    {
        return m_back;
    }
    Ui::CommandButton *forwardButton() const
    {
        return m_forward;
    }
    Ui::CommandButton *upButton() const
    {
        return m_up;
    }
    Ui::CommandButton *refreshButton() const
    {
        return m_refresh;
    }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void refreshRequested();
    // Same arguments as KUrlNavigator::urlsDropped, so the view container's
    // drop handling takes them as they are.
    void urlsDropped(const QUrl &destination, QDropEvent *event);
    void trailingWidthChanged(int width);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateFromNavigator();
    void useOwnNavigator(const QUrl &url);
    void goSteps(Direction direction, int steps);
    void showHistoryMenu(Direction direction);

    QPointer<KUrlNavigator> m_navigator;
    KUrlNavigator *m_ownNavigator = nullptr;
    Ui::CommandButton *m_back;
    Ui::CommandButton *m_forward;
    Ui::CommandButton *m_up;
    Ui::CommandButton *m_refresh;
    AddressBox *m_box;
    Splitter *m_splitter;
    QHBoxLayout *m_layout;
    QPointer<QWidget> m_trailing;
    int m_trailingWidth = trailingDefault;
    QAction *m_editAction;
    QTimer *m_holdTimer;
    Direction m_holdDirection = Back;
};

}
