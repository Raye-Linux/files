/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QTimer>
#include <QUrl>
#include <QWidget>

class QLabel;

namespace Raye::Ui
{
class CommandButton;
}

namespace Raye::Jobs
{

// The bar that says "Deleted report.pdf" with Undo for 5 seconds after a
// delete (explorer spec 3.11, a Raye extra; Ctrl+Z works anyway). The main
// window places it over the bottom of the view and calls showDeleted().
class DOLPHIN_EXPORT UndoBar : public QWidget
{
    Q_OBJECT
public:
    static constexpr int visibleMs = 5000;

    explicit UndoBar(QWidget *parent = nullptr);

    void showDeleted(const QList<QUrl> &urls);
    QString text() const;

Q_SIGNALS:
    // Undo was pressed: the window runs KIO's undo.
    void undoRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QLabel *m_label;
    Raye::Ui::CommandButton *m_undo;
    QTimer m_timer;
};

// "Deleted report.pdf", "Deleted 4 items"
DOLPHIN_EXPORT QString deletedText(const QList<QUrl> &urls);

}
