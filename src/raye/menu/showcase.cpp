/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The right-click menu in the render harness (tests/raye/harness): a file,
// a folder with files on the clipboard, a picture, and the background of a
// folder. Stand-in hooks: nothing runs.

#include "compactmenu.h"
#include "menuplan.h"

#include "ui/showcase.h"
#include "ui/tokens.h"

#include <QPainter>
#include <QTimer>

namespace Raye::Menu
{
namespace
{
using Ui::TokenSet;
using Ui::Tokens;

class Surface : public QWidget
{
public:
    explicit Surface(QWidget *parent)
        : QWidget(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), Tokens::current().color(TokenSet::bgBase));
    }
};

CompactMenu::Hooks standIns()
{
    CompactMenu::Hooks h;
    h.trigger = [](const QString &) { };
    h.submenu = [](const QString &id, QMenu *parent) -> QMenu * {
        if (id == QLatin1String("compress")) {
            return nullptr;
        }
        auto *m = new QMenu(parent);
        m->addAction(id);
        return m;
    };
    return h;
}

QWidget *withMenu(QWidget *parent, const Selection &selection, QPoint at)
{
    auto *base = new Surface(parent);
    QTimer::singleShot(150, base, [base, selection, at] {
        auto *menu = new CompactMenu(plan(selection), standIns(), base);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(base->mapToGlobal(at));
    });
    return base;
}
}

RAYE_SHOWCASE(contextMenuFile, "ContextMenuFile", "the right-click menu of a file")
{
    Selection s;
    s.items = {ItemKind::File};
    return withMenu(parent, s, QPoint(80, 40));
}

RAYE_SHOWCASE(contextMenuFolder, "ContextMenuFolder", "the right-click menu of a folder, files on the clipboard")
{
    Selection s;
    s.items = {ItemKind::Folder};
    s.clipboardHasFiles = true;
    return withMenu(parent, s, QPoint(80, 40));
}

RAYE_SHOWCASE(contextMenuPicture, "ContextMenuPicture", "the right-click menu of a picture")
{
    Selection s;
    s.items = {ItemKind::Picture};
    return withMenu(parent, s, QPoint(80, 40));
}

RAYE_SHOWCASE(contextMenuArchive, "ContextMenuArchive", "the right-click menu of a ZIP file")
{
    Selection s;
    s.items = {ItemKind::Archive};
    s.local = true;
    s.writable = true;
    return withMenu(parent, s, QPoint(80, 40));
}

RAYE_SHOWCASE(contextMenuBackground, "ContextMenuBackground", "the right-click menu of a folder's empty space, files on the clipboard")
{
    Selection s;
    s.background = true;
    s.clipboardHasFiles = true;
    return withMenu(parent, s, QPoint(80, 40));
}

}
