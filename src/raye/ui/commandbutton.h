/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "uiexport.h"

#include "tokens.h"

#include <QAbstractButton>
#include <QKeySequence>
#include <QPointer>

class QAction;
#include <QMenu>

namespace Raye::Ui
{

class ColorFade;

// The Raye button (explorer spec 3.4, theming 3.12). 32 tall. With only an
// icon it is 40 wide; with a label: 12 px, icon 16, 8 px, label, 12 px, and
// a 12 px chevron after the label when a menu opens from it. Tooltips say
// the shortcut: "Copy (Ctrl+C)". Disabled, not hidden, when it cannot act.
class RAYE_UI_EXPORT CommandButton : public QAbstractButton
{
    Q_OBJECT
public:
    enum Kind {
        Subtle, // command bars: transparent at rest, subtle fill on hover
        Standard, // dialogs: control fill and a 1 px border
        Accent, // the one main action of a dialog
    };
    Q_ENUM(Kind)

    // Which side is cut square, for two buttons drawn as one (SplitButton).
    enum Join {
        NoJoin,
        JoinRight, // the right edge meets another button
        JoinLeft,
    };

    explicit CommandButton(QWidget *parent = nullptr);
    CommandButton(const QIcon &icon, const QString &text, QWidget *parent = nullptr);
    ~CommandButton() override;

    Kind kind() const
    {
        return m_kind;
    }
    void setKind(Kind kind);

    // The label stays the button's name for tooltips and screen readers.
    bool iconOnly() const
    {
        return m_iconOnly;
    }
    void setIconOnly(bool iconOnly);

    // 24 tall instead of 32 (compact view).
    bool compact() const
    {
        return m_compact;
    }
    void setCompact(bool compact);

    bool showsChevron() const;
    void setShowChevron(bool show);

    // A menu that opens under the button when it is clicked (New, Sort,
    // View...). The button shows the chevron and stays pressed meanwhile.
    QMenu *menu() const
    {
        return m_menu;
    }
    void setMenu(QMenu *menu);
    void showMenu();

    QKeySequence shortcutHint() const
    {
        return m_shortcutHint;
    }
    void setShortcutHint(const QKeySequence &shortcut);

    // Takes icon, label, tooltip, shortcut, enabled and checked state from
    // the action and follows it; clicking triggers it. The command bar is
    // built from Files' existing actions this way.
    QAction *action() const
    {
        return m_action;
    }
    void setAction(QAction *action);

    // Where icon and label sit when the button is wider than they need:
    // centred (buttons), or Qt::AlignLeft for rows of a list or a flyout.
    Qt::Alignment contentAlignment() const
    {
        return m_alignment;
    }
    void setContentAlignment(Qt::Alignment alignment);

    void setJoin(Join join);
    Join join() const
    {
        return m_join;
    }

    // "Copy (Ctrl+C)", in the user's language.
    static QString toolTipFor(const QString &name, const QKeySequence &shortcut);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // The colours of each state, for tests and for SplitButton.
    struct Look {
        QColor fill;
        QColor ink;
        QColor border; // invalid: none
    };
    static Look look(Kind kind, bool enabled, bool hovered, bool pressed, bool checked, const TokenSet &tokens);

Q_SIGNALS:
    void menuAboutToShow();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void checkStateSet() override;
    void nextCheckState() override;
    bool event(QEvent *event) override;

private:
    void init();
    void syncFromAction();
    QString plainText() const;

    Kind m_kind = Subtle;
    Join m_join = NoJoin;
    Qt::Alignment m_alignment = Qt::AlignHCenter;
    bool m_iconOnly = false;
    bool m_compact = false;
    bool m_chevron = false;
    bool m_hovered = false;
    bool m_menuOpen = false;
    QKeySequence m_shortcutHint;
    QPointer<QMenu> m_menu;
    QPointer<QAction> m_action;
    ColorFade *m_fill = nullptr;
};

// Draws a 12 px chevron pointing down, centred in box.
RAYE_UI_EXPORT void paintChevron(QPainter *painter, const QRectF &box, const QColor &color);

}
