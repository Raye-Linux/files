/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"
#include "window/statustext.h"

#include <QWidget>

namespace Raye::Ui
{
class CommandButton;
}

namespace Raye::Window
{
// The status bar at the bottom of Files (explorer spec 1.6, 3.7): 24 tall,
// "12 items | 1 item selected  2.4 MB" on the left in caption text, and two
// small toggles on the right, Details and Large icons.
class DOLPHIN_EXPORT StatusBar : public QWidget
{
    Q_OBJECT

public:
    static constexpr int height = 24;

    enum class Mode {
        Other,
        Details,
        LargeIcons,
    };

    explicit StatusBar(QWidget *parent = nullptr);

    void setCounts(const StatusCounts &counts);
    // Nothing to count (Home and the other pages).
    void clear();
    // A message that replaces the counts until the next setCounts(), like
    // "Loading folder…".
    void setMessage(const QString &message);
    QString text() const;

    void setMode(Mode mode);

    QSize sizeHint() const override;

Q_SIGNALS:
    void detailsRequested();
    void largeIconsRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QStringList m_parts;
    QString m_message;
    Ui::CommandButton *m_details;
    Ui::CommandButton *m_large;
};
}
