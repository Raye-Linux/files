/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "uiexport.h"

#include <QDialog>
#include <QPointer>

#include <array>

class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class KWindowShadow;

namespace Raye::Ui
{

class CommandButton;

// The Raye dialog (WinUI ContentDialog, theming spec 1.9): a frameless card,
// radius 8, 24 px padding, the title in Subtitle, the content, then an 80 px
// button row on the base colour with up to three buttons of equal width,
// 8 px apart: Primary (accent), Secondary, Close. At most 548 wide. The
// card drags the window from its title; Esc closes it, Enter presses the
// default button. Files' delete, conflict, extract and network dialogs are
// built on it.
class RAYE_UI_EXPORT CardDialog : public QDialog
{
    Q_OBJECT
public:
    enum ButtonRole {
        Primary,
        Secondary,
        Close,
    };
    Q_ENUM(ButtonRole)

    // What exec() returns: Close and Esc give Rejected (0).
    enum Outcome {
        PrimaryChosen = QDialog::Accepted,
        SecondaryChosen = 2,
    };

    explicit CardDialog(QWidget *parent = nullptr);
    ~CardDialog() override;

    QString title() const;
    void setTitle(const QString &title);

    QWidget *contentWidget() const
    {
        return m_content;
    }
    // The dialog takes ownership; the old content is deleted.
    void setContentWidget(QWidget *content);
    // A plain message under the title (Body, wraps): a QLabel as content.
    void setMessage(const QString &message);

    // Shows the button with this label; an empty label removes it.
    CommandButton *setButtonText(ButtonRole role, const QString &text);
    CommandButton *button(ButtonRole role) const;

    // The button Enter presses and that is drawn in the accent. Primary
    // by default; Close makes no button accent (a question with no safe
    // main action).
    ButtonRole defaultButton() const
    {
        return m_default;
    }
    void setDefaultButton(ButtonRole role);

    // False: a button only emits buttonClicked() and the dialog stays
    // open (a "Try again" that runs something first).
    bool closesOnButton() const
    {
        return m_autoClose;
    }
    void setClosesOnButton(bool closes)
    {
        m_autoClose = closes;
    }

    // Translucent corners need a compositor; without one the card is
    // square. The render harness forces it on.
    bool translucent() const
    {
        return m_translucent;
    }
    void setTranslucent(bool translucent);

    // The width the card asks for: its content plus padding, 320 to 548.
    QSize sizeHint() const override;
    void setVisible(bool visible) override;

    static constexpr int maximumWidth = 548;
    static constexpr int minimumWidth = 320;

    // Where the button row starts (the rest is the card's body).
    int buttonRowTop() const;

Q_SIGNALS:
    void buttonClicked(Raye::Ui::CardDialog::ButtonRole role);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void relayoutButtons();
    void applyWindowEffects();
    void press(ButtonRole role);

    QVBoxLayout *m_bodyLayout;
    QHBoxLayout *m_buttonLayout;
    QWidget *m_buttonRow;
    QLabel *m_title;
    QPointer<QWidget> m_content;
    std::array<QPointer<CommandButton>, 3> m_buttons;
    ButtonRole m_default = Primary;
    bool m_autoClose = true;
    bool m_translucent;
    KWindowShadow *m_shadow = nullptr;
};

}
