/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QList>
#include <QPointer>
#include <QTabBar>

#include <functional>

class QMenu;
class QVariantAnimation;

namespace Raye::Title
{

class TabStyle;

// The tabs of the title row (explorer spec 1.1, 3.2): 32 tall on the bottom
// of the 40 px row, 112 to 240 wide, icon, folder name and a close button
// that shows on the selected tab and under the pointer. The selected tab
// takes the colour of the address row below and flows into it. A new tab
// grows in, a closed one shrinks out, a dragged one slides; a drop over a
// tab lights a line under it, and a copy into its folder draws a small ring
// around its icon.
//
// Dolphin's DolphinTabBar derives from it and keeps its drag and drop code.
class DOLPHIN_EXPORT TabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit TabBar(QWidget *parent = nullptr);
    ~TabBar() override;

    // The room the title row leaves for the tabs; their widths follow it.
    void setAvailableWidth(int width);
    int availableWidth() const
    {
        return m_available;
    }

    // Copy progress for the folder of a tab, 0 to 1; a negative value takes
    // the ring away.
    void setTabProgress(int index, qreal progress);
    qreal tabProgress(int index) const;

    // The tab a drag is over, or -1.
    void setDropTargetIndex(int index);
    int dropTargetIndex() const
    {
        return m_dropIndex;
    }

    // The tab under the pointer, or -1.
    int hoveredIndex() const
    {
        return m_hoverIndex;
    }

    // Closes a tab the way the close button does: it shrinks out, then
    // tabCloseRequested() is emitted.
    void requestClose(int index);

    // The tab menu (Close tab, Close other tabs, Close tabs to the right,
    // Duplicate tab, Move tab to new window), for the tab at index. The
    // caller owns it.
    QMenu *createTabMenu(int index, QWidget *parent);

    // Whether a close button is shown on the tab (the selected one and the
    // one under the pointer).
    bool isCloseButtonShown(int index) const;

    QSize tabSizeHint(int index) const override;
    QSize minimumTabSizeHint(int index) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // For the style and the tests: the label width the tab gets, and the
    // tab's width scale while it grows in or shrinks out (1 at rest).
    int labelWidth(int index) const;
    qreal tabScale(int index) const;

Q_SIGNALS:
    void duplicateTabRequested(int index);
    void moveTabToNewWindowRequested(int index);
    // A double click on a tab: the title row maximises or restores the window.
    void titleAreaDoubleClicked();

protected:
    void tabInserted(int index) override;
    void tabRemoved(int index) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct TabState {
        quint64 id = 0;
        qreal scale = 1.0;
        qreal progress = -1.0;
        bool closing = false;
    };

    int indexOfId(quint64 id) const;
    void animateScale(quint64 id, qreal from, qreal to, const std::function<void()> &done = {});
    void relayout();
    void setHoverIndex(int index);
    void updateCloseButtons();
    void closeButtonClicked();
    void tabsMoved(int from, int to);

    TabStyle *m_style;
    QList<TabState> m_states;
    quint64 m_nextId = 1;
    int m_available = -1;
    int m_hoverIndex = -1;
    int m_dropIndex = -1;
    int m_dragIndex = -1;
    bool m_relayoutQueued = false;
};

}
