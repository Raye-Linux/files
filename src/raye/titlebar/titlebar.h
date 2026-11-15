/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QPointer>
#include <QWidget>

#include <functional>

class QAbstractButton;
class QTabBar;
class QTimer;

namespace Raye::Title
{

class TabBar;

// The title row of Files (explorer spec 1.1, 3.2): one 40 px row with the
// tabs, the "+" new tab button, empty space that moves the window, and the
// caption buttons Files draws itself, like the Raye decoration does
// (46 x 40, red close under the pointer, Snap layouts after 400 ms on
// Maximise).
//
// The decoration hides its own title bar for Files (rayedecorationrc
// exception), and keeps the border, shadow, resize edges and corners. When
// dolphinrc [Raye] OwnTitleBar=false the row stays as a tab row under the
// normal title bar and draws no caption buttons.
class DOLPHIN_EXPORT TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);
    ~TitleBar() override;

    // The tab strip; the row takes it over (reparents it). A Raye TabBar
    // also gets the room it may use and the double click.
    void setTabBar(QTabBar *tabBar);
    QTabBar *tabBar() const
    {
        return m_tabs;
    }

    void setOwnTitleBar(bool own);
    bool ownTitleBar() const
    {
        return m_own;
    }
    // dolphinrc [Raye] OwnTitleBar: true, false, or not set to follow the
    // decoration (own row when it hides its title bar for Files).
    static bool readOwnTitleBar();
    static bool ownTitleBarFor(const QString &setting, bool decorationHides);
    // KWin's Wayland window class for Files is "dolphin org.kde.dolphin".
    static bool decorationHidesTitleBar(const QString &windowClass = QStringLiteral("dolphin org.kde.dolphin"));

    // The buttons by name: "newtab", "minimize", "maximize", "close".
    QAbstractButton *button(const QString &name) const;

    // What resting on Maximise does. By default it asks KWin for Snap
    // layouts (kglobalaccel "Raye Snap Layouts", without waiting for an
    // answer: kglobalaccel lives in KWin, a blocking call could hang both).
    using SnapHandler = std::function<void()>;
    static void setSnapLayoutsHandler(const SnapHandler &handler);
    static void requestSnapLayouts();

    // Maximises or restores the window, as a double click on the row does.
    void toggleMaximized();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void newTabRequested();

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void relayout();
    void watchWindow();
    void updateCaptionStates();

    QPointer<QTabBar> m_tabs;
    QAbstractButton *m_newTab;
    QAbstractButton *m_minimize;
    QAbstractButton *m_maximize;
    QAbstractButton *m_close;
    QTimer *m_snapTimer;
    QPointer<QWidget> m_window;
    QPoint m_pressPos;
    bool m_pressed = false;
    bool m_own = true;
};

}
