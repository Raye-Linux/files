/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "uiexport.h"

#include "commandbutton.h"

#include <QWidget>

class QMenu;

namespace Raye::Ui
{

// A button with a second, narrow part that opens a menu (WinUI SplitButton):
// the main part acts at once, the chevron part (28 wide) offers the other
// choices. One Tab stop; Alt+Down or F4 on it opens the menu.
class RAYE_UI_EXPORT SplitButton : public QWidget
{
    Q_OBJECT
public:
    explicit SplitButton(QWidget *parent = nullptr);
    SplitButton(const QIcon &icon, const QString &text, QWidget *parent = nullptr);

    CommandButton *mainButton() const
    {
        return m_main;
    }
    CommandButton *menuButton() const
    {
        return m_more;
    }

    void setKind(CommandButton::Kind kind);
    CommandButton::Kind kind() const
    {
        return m_main->kind();
    }
    void setText(const QString &text);
    QString text() const
    {
        return m_main->text();
    }
    void setIcon(const QIcon &icon);

    void setMenu(QMenu *menu);
    QMenu *menu() const;
    void showMenu();

Q_SIGNALS:
    void clicked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void init();

    CommandButton *m_main;
    CommandButton *m_more;
};

}
