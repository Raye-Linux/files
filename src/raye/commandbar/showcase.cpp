/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The command bar in the render harness (tests/raye/harness), built from
// stand-in actions: a folder with nothing selected, a picture selected, a
// USB drive selected in This PC, and the Sort, View and See more menus.

#include "commandbar.h"
#include "themeicon_p.h"
#include "commandids.h"

#include "locations/locationnames.h"
#include "ui/commandbutton.h"
#include "ui/showcase.h"
#include "ui/tokens.h"

#include <KLocalizedString>

#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

namespace Raye::Commands
{
namespace
{
using Ui::TokenSet;
using Ui::Tokens;

// Sample words of the stand-ins (Dolphin's own in real use), not Files'.
QString demoText(const char *english, const char *french)
{
    const QStringList languages = KLocalizedString::languages();
    return QString::fromUtf8(!languages.isEmpty() && languages.first().startsWith(QLatin1String("fr")) ? french : english);
}

class Surface : public QWidget
{
public:
    Surface(TokenSet::Role role, QWidget *parent)
        : QWidget(parent)
        , m_role(role)
    {
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Tokens::current().color(m_role));
        p.drawRoundedRect(rect(), Ui::Metrics::radiusOverlay, Ui::Metrics::radiusOverlay);
    }

private:
    TokenSet::Role m_role;
};

// What a window's action collection holds, in short.
QHash<QString, QAction *> standIns(QObject *owner)
{
    QHash<QString, QAction *> actions;
    auto add = [&](const char *id, const char *icon = nullptr, bool checkable = false) {
        auto *a = new QAction(icon ? themeIcon(QString::fromLatin1(icon)) : QIcon(), QString::fromLatin1(id), owner);
        a->setObjectName(QString::fromLatin1(id));
        a->setCheckable(checkable);
        actions.insert(a->objectName(), a);
        return a;
    };
    for (const char *id : {Ids::newMenu, Ids::paste, Ids::moveToTrash, Ids::deleteFile, Ids::sort, Ids::viewMode, Ids::undo,
                           Ids::redo, Ids::selectAll, Ids::invertSelection, Ids::properties, Ids::addToPlaces, Ids::preferences, Ids::openTerminal}) {
        add(id);
    }
    add(Ids::cut)->setShortcut(QKeySequence::Cut);
    add(Ids::copy)->setShortcut(QKeySequence::Copy);
    add(Ids::rename)->setShortcut(Qt::Key_F2);
    actions.value(QString::fromLatin1(Ids::moveToTrash))->setShortcut(QKeySequence::Delete);
    actions.value(QString::fromLatin1(Ids::undo))->setShortcut(QKeySequence::Undo);
    actions.value(QString::fromLatin1(Ids::selectAll))->setShortcut(QKeySequence::SelectAll);
    add(Ids::detailsPane, nullptr, true);
    add(Ids::previewPane, nullptr, true);
    add(Ids::navigationPane, nullptr, true)->setChecked(true);
    add(Ids::fileExtensions, nullptr, true);
    add(Ids::itemCheckBoxes, nullptr, true);
    add(Ids::showHidden, nullptr, true);
    add(Ids::compactView, nullptr, true);
    add(Ids::showInGroups, nullptr, true);

    auto *sortGroup = new QActionGroup(owner);
    auto *sortMenu = new QMenu;
    QObject::connect(owner, &QObject::destroyed, sortMenu, &QObject::deleteLater);
    for (const char *id : {Ids::sortByName, Ids::sortByDate, Ids::sortByType, Ids::sortBySize}) {
        QAction *a = add(id, nullptr, true);
        a->setActionGroup(sortGroup);
        sortMenu->addAction(a);
    }
    actions.value(QString::fromLatin1(Ids::sortByName))->setChecked(true);
    actions.value(QString::fromLatin1(Ids::sort))->setMenu(sortMenu);
    auto *orderGroup = new QActionGroup(owner);
    for (const char *id : {Ids::ascending, Ids::descending}) {
        add(id, nullptr, true)->setActionGroup(orderGroup);
    }
    actions.value(QString::fromLatin1(Ids::ascending))->setChecked(true);

    auto *modeGroup = new QActionGroup(owner);
    auto *modeMenu = new QMenu;
    QObject::connect(owner, &QObject::destroyed, modeMenu, &QObject::deleteLater);
    for (const auto &[id, icon] : {std::pair{Ids::iconsMode, "view-list-icons"}, std::pair{Ids::compactMode, "view-list-text"}, std::pair{Ids::detailsMode, "view-list-details"}}) {
        QAction *a = add(id, icon, true);
        a->setActionGroup(modeGroup);
        modeMenu->addAction(a);
    }
    actions.value(QString::fromLatin1(Ids::detailsMode))->setChecked(true);
    actions.value(QString::fromLatin1(Ids::viewMode))->setMenu(modeMenu);

    auto *newMenu = new QMenu;
    QObject::connect(owner, &QObject::destroyed, newMenu, &QObject::deleteLater);
    newMenu->addAction(themeIcon(QStringLiteral("folder-new")), demoText("Folder", "Dossier"));
    newMenu->addAction(themeIcon(QStringLiteral("text-plain")), demoText("Text document", "Document texte"));
    actions.value(QString::fromLatin1(Ids::newMenu))->setMenu(newMenu);
    return actions;
}

CommandBar *sampleBar(QWidget *parent, const CommandContext &context)
{
    auto *bar = new CommandBar(parent);
    const QHash<QString, QAction *> actions = standIns(bar);
    bar->setActionLookup([actions](const QString &name) {
        return actions.value(name);
    });
    bar->setShareHandler([](QWidget *, const QList<QUrl> &) { });
    bar->setContext(context);
    return bar;
}

CommandContext pictureSelected()
{
    CommandContext c;
    c.url = QUrl::fromLocalFile(QStringLiteral("/home/ann/Pictures"));
    c.selection = {QUrl::fromLocalFile(QStringLiteral("/home/ann/Pictures/Lisbon.jpg"))};
    c.selectionMimeTypes = {QStringLiteral("image/jpeg")};
    return c;
}

QWidget *framed(QWidget *parent, QWidget *content)
{
    auto *frame = new Surface(TokenSet::bgLayerSolid, parent);
    auto *l = new QVBoxLayout(frame);
    l->setContentsMargins(0, 0, 0, 0);
    content->setParent(frame);
    l->addWidget(content);
    return frame;
}

QWidget *page(QWidget *parent)
{
    auto *base = new Surface(TokenSet::bgBase, parent);
    auto *column = new QVBoxLayout(base);
    column->setContentsMargins(16, 16, 16, 16);
    column->setSpacing(16);
    return base;
}

QWidget *withMenu(QWidget *parent, const QString &button)
{
    QWidget *base = page(parent);
    auto *column = static_cast<QVBoxLayout *>(base->layout());
    CommandBar *bar = sampleBar(nullptr, pictureSelected());
    column->addWidget(framed(base, bar));
    column->addStretch(1);
    QTimer::singleShot(0, bar, [bar, button]() {
        if (button == QLatin1String("seemore")) {
            bar->showSeeMore();
        } else if (auto *b = qobject_cast<Ui::CommandButton *>(bar->button(button))) {
            b->showMenu();
        }
    });
    return base;
}
}

RAYE_SHOWCASE(commands, "Commands", "the command bar: nothing selected, a picture selected, a USB drive selected")
{
    QWidget *base = page(parent);
    auto *column = static_cast<QVBoxLayout *>(base->layout());

    CommandContext empty;
    empty.url = QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents"));
    column->addWidget(framed(base, sampleBar(nullptr, empty)));

    CommandBar *picture = sampleBar(nullptr, pictureSelected());
    if (QAction *details = picture->sourceAction(QString::fromLatin1(Ids::detailsPane))) {
        details->setChecked(true);
    }
    column->addWidget(framed(base, picture));

    CommandContext drive;
    drive.url = Locations::thisPcUrl();
    drive.driveUdi = QStringLiteral("/org/freedesktop/UDisks2/block_devices/sdb1");
    drive.driveRemovable = true;
    drive.writable = false;
    column->addWidget(framed(base, sampleBar(nullptr, drive)));
    column->addStretch(1);
    return base;
}

RAYE_SHOWCASE(commandsSort, "CommandsSort", "the command bar with its Sort menu open")
{
    return withMenu(parent, QStringLiteral("sort"));
}

RAYE_SHOWCASE(commandsView, "CommandsView", "the command bar with its View menu open")
{
    return withMenu(parent, QStringLiteral("view"));
}

RAYE_SHOWCASE(commandsMore, "CommandsMore", "the command bar with See more open")
{
    return withMenu(parent, QStringLiteral("seemore"));
}

}
