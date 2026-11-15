/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "uiexport.h"

#include <QWidget>

namespace Raye::Ui
{

// A 1 px stroke.divider line: 1 x 24 between groups of a command bar, or a
// horizontal line across a pane or dialog.
class RAYE_UI_EXPORT Separator : public QWidget
{
    Q_OBJECT
public:
    explicit Separator(Qt::Orientation orientation = Qt::Vertical, QWidget *parent = nullptr);

    Qt::Orientation orientation() const
    {
        return m_orientation;
    }
    void setOrientation(Qt::Orientation orientation);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Qt::Orientation m_orientation;
};

}
