/*
 * SPDX-FileCopyrightText: 2014 Emmanuel Pescosta <emmanuelpescosta099@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DOLPHIN_TAB_BAR_H
#define DOLPHIN_TAB_BAR_H

// Raye: the tabs are drawn, sized and menued by the Raye title row.
#include "titlebar/tabbar.h"

class DolphinTabBar : public Raye::Title::TabBar
{
    Q_OBJECT

public:
    explicit DolphinTabBar(QWidget *parent);

Q_SIGNALS:
    void openNewActivatedTab(int index);
    void tabDragMoveEvent(int index, QDragMoveEvent *event);
    void tabDropEvent(int index, QDropEvent *event);
    void tabDetachRequested(int index);
    // Raye: kept for the tab widget; renaming tabs is not offered.
    void tabRenamed(int index, const QString &label);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    // Raye: middle click, double click and the tab menu are Raye::Title::TabBar's.

private Q_SLOTS:
    void slotAutoActivationTimeout();

private:
    /**
     * If \a index is a valid index (>= 0), store the index and start the timer
     * (if the interval >= 0 ms). If the index is not valid (< 0), stop the timer.
     */
    void updateAutoActivationTimer(const int index);

private:
    QTimer *m_autoActivationTimer;
    int m_autoActivationIndex;
};

#endif // DOLPHIN_TAB_BAR_H
