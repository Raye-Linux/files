/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "locations/locationnames.h"
#include "ui/flyout.h"

#include <QAbstractButton>
#include <QPointer>

class QDropEvent;

namespace Raye::Address
{

class Subfolders;

// One crumb: its name, a subtle fill on hover; a drop target when it is a
// folder. Also the "«" of folded crumbs.
class CrumbButton : public QAbstractButton
{
    Q_OBJECT
public:
    explicit CrumbButton(QWidget *parent = nullptr);

    void setCrumb(const Locations::Crumb &crumb);
    const Locations::Crumb &crumb() const
    {
        return m_crumb;
    }
    // Room for the text; shorter than it needs elides it.
    int naturalWidth() const;

    QSize sizeHint() const override;

Q_SIGNALS:
    void dropped(const QUrl &destination, QDropEvent *event);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    Locations::Crumb m_crumb;
    bool m_hovered = false;
    bool m_dropTarget = false;
};

// The chevron after a crumb: "›", turned down while its list is open.
class ChevronButton : public QAbstractButton
{
    Q_OBJECT
public:
    static constexpr int width = 16;

    explicit ChevronButton(QWidget *parent = nullptr);
    void setOpen(bool open);
    bool isOpen() const
    {
        return m_open;
    }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    bool m_open = false;
    bool m_hovered = false;
};

// The drop-down of a chevron: the folders in a crumb, 24 rows then a
// scroll bar, in a Raye flyout.
class SubfolderFlyout : public Ui::Flyout
{
    Q_OBJECT
public:
    static constexpr int visibleRows = 24;

    SubfolderFlyout(const Locations::LocationNames &names, QWidget *parent = nullptr);
    void list(const QUrl &url);

Q_SIGNALS:
    void folderChosen(const QUrl &url);
    void ready();

private:
    void fill();

    Subfolders *m_lister;
};

}
