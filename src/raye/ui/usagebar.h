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

// How full a drive is (explorer spec 3.10): 4 px tall, radius 2, an
// accent-to-glow fill on a faint track; over 90 % used it turns to the
// caution colour. It grows from empty the first time it shows (400 ms).
class RAYE_UI_EXPORT UsageBar : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal value READ value WRITE setValue)

public:
    explicit UsageBar(QWidget *parent = nullptr);

    // Used part, 0..1.
    qreal value() const
    {
        return m_value;
    }
    void setValue(qreal value);
    // From bytes: used = total - free. Nothing to show for an empty total.
    void setUsage(qint64 usedBytes, qint64 totalBytes);

    bool caution() const;
    // The value drawn right now (it moves while the bar grows).
    qreal shownValue() const
    {
        return m_shown;
    }
    void setAnimateOnShow(bool animate)
    {
        m_animate = animate;
    }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void grow();

    qreal m_value = 0;
    qreal m_shown = 0;
    bool m_animate = true;
    bool m_grown = false;
};

}
