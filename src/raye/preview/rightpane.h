/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <KFileItem>

#include <QPointer>
#include <QWidget>

class QPropertyAnimation;
class QStackedWidget;

namespace Raye::Preview
{

class DetailsPane;
class PreviewPane;

// The pane on the right of Files (explorer spec 3.8): the Details pane or
// the Preview pane, never both. 320 wide by default (240 to 480); it
// slides in and out in 250 ms. The window toggles it with Alt+Shift+P and
// Alt+P; the last choice is kept in [Raye] PreviewPane.
class DOLPHIN_EXPORT RightPane : public QWidget
{
    Q_OBJECT

public:
    enum Mode {
        Details,
        Preview,
    };
    Q_ENUM(Mode)

    explicit RightPane(QWidget *parent = nullptr);
    ~RightPane() override;

    void setMode(Mode mode);
    Mode mode() const;

    // What the panes describe: the selection, and the folder shown for
    // when nothing is selected.
    void showItems(const KFileItemList &selection, const KFileItem &folder);

    DetailsPane *detailsPane() const;
    PreviewPane *previewPane() const;

    // Shows or hides the pane with the slide of the spec; without motion
    // (reduced motion, tests) it just shows or hides.
    void slideIn();
    void slideOut();

    QSize sizeHint() const override;

    // The value [Raye] PreviewPane takes for a mode, and back.
    static QString modeKey(Mode mode);
    static Mode modeFromKey(const QString &key, Mode fallback = Details);

Q_SIGNALS:
    void modeChanged(Mode mode);
    void urlActivated(const QUrl &url);
    void shareRequested(const KFileItemList &items);
    void openRequested(const KFileItemList &items);

private:
    void refresh();

    QStackedWidget *m_stack = nullptr;
    DetailsPane *m_details = nullptr;
    PreviewPane *m_preview = nullptr;
    QPointer<QPropertyAnimation> m_slide;
    Mode m_mode = Details;
    KFileItemList m_selection;
    KFileItem m_folder;
};

}
