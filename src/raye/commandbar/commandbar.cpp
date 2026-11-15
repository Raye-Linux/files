/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "commandbar.h"
#include "themeicon_p.h"
#include "commandids.h"
#include "defaultsets.h"
#include "fold.h"

#include "share/shareflyout.h"
#include "ui/commandbutton.h"
#include "ui/motion.h"
#include "ui/separator.h"
#include "ui/tokens.h"

#include <KLocalizedString>

#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace Raye::Commands
{
using Ui::CommandButton;

namespace
{
constexpr int buttonHeight = Ui::Metrics::controlHeight;
constexpr int separatorWidth = 2 * CommandBar::separatorMargin + 1;
constexpr int detailsGap = 8; // between "…" and Details when they meet

enum Group {
    NewGroup = 0,
    EditGroup = 1,
    ViewGroup = 2,
    ContextGroup = 3, // and up: one per context set
};

const QString idNew = QStringLiteral("new");
const QString idCut = QStringLiteral("cut");
const QString idCopy = QStringLiteral("copy");
const QString idPaste = QStringLiteral("paste");
const QString idRename = QStringLiteral("rename");
const QString idShare = QStringLiteral("share");
const QString idDelete = QStringLiteral("delete");
const QString idSort = QStringLiteral("sort");
const QString idView = QStringLiteral("view");
const QString idSeeMore = QStringLiteral("seemore");
const QString idDetails = QStringLiteral("details");

QString name(const char *id)
{
    return QString::fromLatin1(id);
}
}

// The once-only glow on Paste after something is copied (spec 3.4): a ring
// of accent light that comes and goes in 250 ms.
class PasteGlow : public QWidget
{
public:
    explicit PasteGlow(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        hide();
    }

    void pulse()
    {
        if (!Ui::Motion::enabled()) {
            return;
        }
        show();
        raise();
        Ui::Motion::run(
            this,
            Ui::Motion::Normal,
            [this](qreal t) {
                m_strength = t < 0.4 ? t / 0.4 : (1.0 - t) / 0.6;
                update();
            },
            [this]() {
                hide();
            });
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Ui::TokenSet &t = Ui::Tokens::current();
        QColor glow = t.color(Ui::TokenSet::accent);
        glow.setAlphaF(0.22 * m_strength);
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Ui::Metrics::radiusControl, Ui::Metrics::radiusControl);
        QColor ring = t.color(Ui::TokenSet::glow);
        ring.setAlphaF(std::min(1.0, ring.alphaF() * 2 * m_strength));
        p.setPen(QPen(ring, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), Ui::Metrics::radiusControl, Ui::Metrics::radiusControl);
    }

private:
    qreal m_strength = 0;
};

CommandBar::CommandBar(QWidget *parent)
    : QWidget(parent)
    , m_seeMore(new QMenu(this))
    , m_sort(new QMenu(this))
    , m_view(new QMenu(this))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(i18nc("@info accessible name of the command bar", "Commands"));
    connect(m_seeMore, &QMenu::aboutToShow, this, [this]() {
        fillSeeMore(m_seeMore);
    });
    connect(m_sort, &QMenu::aboutToShow, this, [this]() {
        fillSort(m_sort);
    });
    connect(m_view, &QMenu::aboutToShow, this, [this]() {
        fillView(m_view);
    });

    createButtons();
    m_pasteGlow = new PasteGlow(this);

    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &CommandBar::clipboardChanged);
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    m_state.clipboardHasFiles = mime && mime->hasUrls();

    m_share = [](QWidget *anchor, const QList<QUrl> &urls) {
        Share::ShareFlyout::share(anchor, urls);
    };

    addDefaultSets(this);
    updateStates();
}

CommandBar::~CommandBar()
{
    // The New menu is Dolphin's: give it back untouched.
    for (const QPointer<QAction> &a : std::as_const(m_newExtras)) {
        delete a.data();
    }
}

CommandButton *CommandBar::makeButton(const QString &id, const QString &text, const char *icon, bool iconOnly)
{
    auto *b = new CommandButton(themeIcon(QString::fromLatin1(icon)), text, this);
    b->setIconOnly(iconOnly);
    b->setAccessibleName(text);
    b->setObjectName(QStringLiteral("raye-command-") + id);
    return b;
}

void CommandBar::createButtons()
{
    struct Spec {
        QString id;
        QString text;
        const char *icon;
        bool iconOnly;
        Command command;
        int group;
    };
    const QList<Spec> specs = {
        {idNew, i18nc("@action:button make a new folder or file", "New"), "document-new", false, Command::New, NewGroup},
        {idCut, i18nc("@action:button", "Cut"), "edit-cut", true, Command::Cut, EditGroup},
        {idCopy, i18nc("@action:button", "Copy"), "edit-copy", true, Command::Copy, EditGroup},
        {idPaste, i18nc("@action:button", "Paste"), "edit-paste", true, Command::Paste, EditGroup},
        {idRename, i18nc("@action:button", "Rename"), "edit-rename", true, Command::Rename, EditGroup},
        {idShare, i18nc("@action:button", "Share"), "document-share", true, Command::Share, EditGroup},
        {idDelete, i18nc("@action:button send to the Recycle Bin", "Delete"), "edit-delete", true, Command::Delete, EditGroup},
        {idSort, i18nc("@action:button sort the items", "Sort"), "view-sort", false, Command::Other, ViewGroup},
        {idView, i18nc("@action:button how the items are shown", "View"), "view-list-icons", false, Command::Other, ViewGroup},
    };
    int index = 0;
    for (const Spec &s : specs) {
        Entry e;
        e.id = s.id;
        e.button = makeButton(s.id, s.text, s.icon, s.iconOnly);
        e.command = s.command;
        e.group = s.group;
        e.priority = 1000 - index++;
        m_entries.append(e);
    }

    auto *sort = qobject_cast<CommandButton *>(button(idSort));
    sort->setMenu(m_sort);
    auto *view = qobject_cast<CommandButton *>(button(idView));
    view->setMenu(m_view);

    connect(qobject_cast<CommandButton *>(button(idCut)), &QAbstractButton::clicked, this, [this]() {
        triggerSource(name(Ids::cut));
    });
    connect(qobject_cast<CommandButton *>(button(idCopy)), &QAbstractButton::clicked, this, [this]() {
        triggerSource(name(Ids::copy));
    });
    connect(qobject_cast<CommandButton *>(button(idPaste)), &QAbstractButton::clicked, this, [this]() {
        triggerSource(name(Ids::paste));
    });
    connect(qobject_cast<CommandButton *>(button(idRename)), &QAbstractButton::clicked, this, [this]() {
        triggerSource(name(Ids::rename));
    });
    connect(qobject_cast<CommandButton *>(button(idDelete)), &QAbstractButton::clicked, this, [this]() {
        if (QAction *a = deleteSource()) {
            a->trigger();
        }
    });
    connect(qobject_cast<CommandButton *>(button(idShare)), &QAbstractButton::clicked, this, [this]() {
        if (m_share && !m_context.selection.isEmpty()) {
            m_share(button(idShare), m_context.selection);
        }
    });

    Entry more;
    more.id = idSeeMore;
    more.button = makeButton(idSeeMore, i18nc("@action:button more commands", "See more"), "overflow-menu", true);
    // No chevron on "…": its menu is opened by hand.
    connect(more.button, &QAbstractButton::clicked, this, &CommandBar::showSeeMore);
    more.fixed = true;
    m_entries.append(more);

    Entry details;
    details.id = idDetails;
    details.button = makeButton(idDetails, i18nc("@action:button show or hide the details pane", "Details"), "documentinfo", false);
    details.button->setCheckable(true);
    details.fixed = true;
    connect(details.button, &QAbstractButton::toggled, this, [this](bool checked) {
        QAction *source = sourceAction(name(Ids::detailsPane));
        if (source && source->isChecked() != checked) {
            source->trigger();
        }
    });
    m_entries.append(details);
}

void CommandBar::setActionLookup(const ActionLookup &lookup)
{
    m_lookup = lookup;

    // New: Dolphin's menu, which fills itself on opening; Raye's extras
    // join it after.
    if (m_newMenu) {
        m_newMenu->disconnect(this);
    }
    QAction *newAction = sourceAction(name(Ids::newMenu));
    m_newMenu = newAction ? newAction->menu() : nullptr;
    auto *newButton = qobject_cast<CommandButton *>(button(idNew));
    newButton->setMenu(m_newMenu);
    if (m_newMenu) {
        connect(m_newMenu, &QMenu::aboutToShow, this, &CommandBar::fillNewExtras);
    }

    // Shortcut hints from the actions Files really has.
    for (const auto &[id, source] : {std::pair{idCut, Ids::cut}, std::pair{idCopy, Ids::copy}, std::pair{idPaste, Ids::paste}, std::pair{idRename, Ids::rename}}) {
        QAction *a = sourceAction(name(source));
        qobject_cast<CommandButton *>(button(id))->setShortcutHint(a ? a->shortcut() : QKeySequence());
    }
    QAction *del = deleteSource();
    qobject_cast<CommandButton *>(button(idDelete))->setShortcutHint(del ? del->shortcut() : QKeySequence());

    watchSources();
    updateStates();
}

QAction *CommandBar::sourceAction(const QString &actionName) const
{
    return m_lookup ? m_lookup(actionName) : nullptr;
}

QAction *CommandBar::deleteSource() const
{
    // Files with a Recycle Bin send there; places without one delete.
    QAction *trash = sourceAction(name(Ids::moveToTrash));
    if (trash && trash->isVisible()) {
        return trash;
    }
    QAction *del = sourceAction(name(Ids::deleteFile));
    return del ? del : trash;
}

void CommandBar::triggerSource(const QString &actionName)
{
    if (QAction *a = sourceAction(actionName)) {
        a->trigger();
    }
}

void CommandBar::watchSources()
{
    for (const QPointer<QAction> &a : std::as_const(m_watched)) {
        if (a) {
            a->disconnect(this);
        }
    }
    m_watched.clear();
    for (const char *id : {Ids::newMenu, Ids::cut, Ids::copy, Ids::paste, Ids::rename, Ids::moveToTrash, Ids::deleteFile, Ids::sort, Ids::viewMode,
                           Ids::detailsPane}) {
        if (QAction *a = sourceAction(name(id))) {
            connect(a, &QAction::changed, this, &CommandBar::scheduleUpdate);
            connect(a, &QAction::toggled, this, &CommandBar::scheduleUpdate);
            m_watched.append(a);
        }
    }
}

void CommandBar::scheduleUpdate()
{
    updateStates();
}

void CommandBar::clipboardChanged()
{
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    const bool files = mime && mime->hasUrls();
    m_state.clipboardHasFiles = files;
    updateStates();
    // Something was just copied: Paste glows once.
    if (files) {
        if (QWidget *paste = button(idPaste); paste && paste->isVisible()) {
            m_pasteGlow->setGeometry(paste->geometry());
            m_pasteGlow->pulse();
        }
    }
}

void CommandBar::setContext(const CommandContext &context)
{
    m_context = context;
    m_state.selected = int(context.selection.size());
    m_state.writable = context.writable;
    rebuildContextButtons();
    updateStates();
}

void CommandBar::registerSet(const QString &id, const QList<QAction *> &actions, const Predicate &predicate, Placement placement)
{
    unregisterSet(id);
    m_sets.append({id, actions, predicate, placement});
    rebuildContextButtons();
    updateStates();
}

bool CommandBar::unregisterSet(const QString &id)
{
    for (int i = 0; i < m_sets.size(); ++i) {
        if (m_sets.at(i).id == id) {
            m_sets.removeAt(i);
            rebuildContextButtons();
            updateStates();
            return true;
        }
    }
    return false;
}

QStringList CommandBar::setIds() const
{
    QStringList out;
    for (const ContextSet &s : m_sets) {
        out << s.id;
    }
    return out;
}

void CommandBar::setShareHandler(const ShareHandler &handler)
{
    m_share = handler;
}

void CommandBar::rebuildContextButtons()
{
    QList<Entry> old = m_contextEntries;
    m_contextEntries.clear();
    m_replaceEditing = false;

    int group = ContextGroup;
    int index = 0;
    for (const ContextSet &set : std::as_const(m_sets)) {
        if (!set.predicate || !set.predicate(m_context)) {
            continue;
        }
        m_replaceEditing = m_replaceEditing || set.placement == Placement::ReplaceEditing;
        for (int i = 0; i < set.actions.size(); ++i) {
            QAction *action = set.actions.at(i);
            if (!action) {
                continue;
            }
            Entry e;
            e.id = action->objectName().isEmpty() ? set.id + QLatin1Char('-') + QString::number(i) : action->objectName();
            e.contextAction = action;
            e.group = group;
            e.priority = 900 - index++; // right of View: folds before it
            // Keep the button of an action already shown.
            for (int k = 0; k < old.size(); ++k) {
                if (old.at(k).contextAction == action) {
                    e.button = old.takeAt(k).button;
                    break;
                }
            }
            if (!e.button) {
                // A context action shown or hidden by its owner moves the row.
                connect(action, &QAction::changed, this, &CommandBar::scheduleUpdate, Qt::UniqueConnection);
                e.button = new CommandButton(this);
                e.button->setAction(action);
                e.button->setIconOnly(false);
                e.button->setObjectName(QStringLiteral("raye-command-") + e.id);
                if (QMenu *menu = action->menu()) {
                    e.button->setMenu(menu);
                }
            }
            m_contextEntries.append(e);
        }
        ++group;
    }
    for (const Entry &gone : std::as_const(old)) {
        delete gone.button;
    }
}

void CommandBar::updateStates()
{
    auto setButton = [this](const QString &id, QAction *source, Command command) {
        QWidget *b = button(id);
        b->setEnabled(source && source->isEnabled() && allows(command, m_state));
    };
    setButton(idNew, sourceAction(name(Ids::newMenu)), Command::New);
    setButton(idCut, sourceAction(name(Ids::cut)), Command::Cut);
    setButton(idCopy, sourceAction(name(Ids::copy)), Command::Copy);
    setButton(idPaste, sourceAction(name(Ids::paste)), Command::Paste);
    setButton(idRename, sourceAction(name(Ids::rename)), Command::Rename);
    setButton(idDelete, deleteSource(), Command::Delete);
    button(idShare)->setEnabled(allows(Command::Share, m_state));

    QAction *sort = sourceAction(name(Ids::sort));
    button(idSort)->setEnabled(sort && sort->isEnabled());
    QAction *view = sourceAction(name(Ids::viewMode));
    button(idView)->setEnabled(view && view->isEnabled());

    if (QAction *details = sourceAction(name(Ids::detailsPane))) {
        auto *b = qobject_cast<QAbstractButton *>(button(idDetails));
        const QSignalBlocker blocker(b);
        b->setChecked(details->isChecked());
        b->setEnabled(details->isEnabled());
    }
    relayout();
}

QWidget *CommandBar::button(const QString &id) const
{
    for (const Entry &e : m_entries) {
        if (e.id == id) {
            return e.button;
        }
    }
    for (const Entry &e : m_contextEntries) {
        if (e.id == id) {
            return e.button;
        }
    }
    return nullptr;
}

QStringList CommandBar::visibleIds() const
{
    QStringList out;
    auto add = [&out, this](const Entry &e) {
        if (m_visible.value(e.id)) {
            out << e.id;
        }
    };
    for (int i = 0; i < m_entries.size() && !m_entries.at(i).fixed; ++i) {
        add(m_entries.at(i));
    }
    for (const Entry &e : m_contextEntries) {
        add(e);
    }
    for (const Entry &e : m_entries) {
        if (e.fixed) {
            add(e);
        }
    }
    return out;
}

QStringList CommandBar::foldedIds() const
{
    QStringList out;
    for (const Entry &e : m_entries) {
        if (!e.fixed && m_visible.contains(e.id) && !m_visible.value(e.id)) {
            out << e.id;
        }
    }
    for (const Entry &e : m_contextEntries) {
        if (m_visible.contains(e.id) && !m_visible.value(e.id)) {
            out << e.id;
        }
    }
    return out;
}

void CommandBar::relayout()
{
    // What the row may show at all: buttons whose action exists.
    auto available = [this](const Entry &e) {
        if (e.contextAction) {
            return e.contextAction->isVisible();
        }
        if (m_replaceEditing && (e.group == NewGroup || e.group == EditGroup)) {
            return false;
        }
        if (e.id == idNew) {
            return sourceAction(name(Ids::newMenu)) != nullptr;
        }
        if (e.id == idCut) {
            return sourceAction(name(Ids::cut)) != nullptr;
        }
        if (e.id == idCopy) {
            return sourceAction(name(Ids::copy)) != nullptr;
        }
        if (e.id == idPaste) {
            return sourceAction(name(Ids::paste)) != nullptr;
        }
        if (e.id == idRename) {
            return sourceAction(name(Ids::rename)) != nullptr;
        }
        if (e.id == idDelete) {
            return deleteSource() != nullptr;
        }
        if (e.id == idSort) {
            return sourceAction(name(Ids::sort)) != nullptr;
        }
        if (e.id == idView) {
            return sourceAction(name(Ids::viewMode)) != nullptr;
        }
        if (e.id == idDetails) {
            return sourceAction(name(Ids::detailsPane)) != nullptr;
        }
        return true; // share, see more
    };

    QList<Entry *> row;
    for (Entry &e : m_entries) {
        if (!e.fixed) {
            row.append(&e);
        }
    }
    for (Entry &e : m_contextEntries) {
        row.append(&e);
    }
    Entry *more = nullptr;
    Entry *details = nullptr;
    for (Entry &e : m_entries) {
        if (e.id == idSeeMore) {
            more = &e;
        } else if (e.id == idDetails) {
            details = &e;
        }
    }

    m_visible.clear();
    QList<Entry *> shown;
    QList<Slot> slotList;
    for (Entry *e : std::as_const(row)) {
        if (!available(*e)) {
            e->button->hide();
            continue;
        }
        shown.append(e);
        slotList.append({e->button->sizeHint().width() + buttonGap, e->priority, e->group});
    }

    const bool hasDetails = details && available(*details);
    const int moreWidth = more->button->sizeHint().width();
    const int detailsWidth = hasDetails ? details->button->sizeHint().width() : 0;
    const int room = width() - 2 * margin - moreWidth - (hasDetails ? detailsWidth + detailsGap : 0);
    const QList<bool> visible = fold(slotList, room, separatorWidth);

    const int y = (rowHeight - buttonHeight) / 2;
    int x = margin;
    int separators = 0;
    bool first = true;
    int lastGroup = -1;
    auto separator = [this, &separators](int at) {
        if (separators >= m_separators.size()) {
            m_separators.append(new Ui::Separator(Qt::Vertical, this));
        }
        Ui::Separator *s = m_separators.at(separators++);
        const int h = Ui::Metrics::separatorLength;
        s->setGeometry(at, (rowHeight - h) / 2, 1, h);
        s->show();
    };
    for (int i = 0; i < shown.size(); ++i) {
        Entry *e = shown.at(i);
        m_visible.insert(e->id, visible.at(i));
        if (!visible.at(i)) {
            e->button->hide();
            continue;
        }
        if (!first && e->group != lastGroup) {
            separator(x + separatorMargin - buttonGap);
            x += separatorWidth;
        }
        const int w = e->button->sizeHint().width();
        e->button->setGeometry(x, y, w, buttonHeight);
        e->button->show();
        x += w + buttonGap;
        first = false;
        lastGroup = e->group;
    }
    while (separators < m_separators.size()) {
        m_separators.at(separators++)->hide();
    }

    more->button->setGeometry(x, y, moreWidth, buttonHeight);
    more->button->show();
    m_visible.insert(idSeeMore, true);

    if (hasDetails) {
        details->button->setGeometry(width() - margin - detailsWidth, y, detailsWidth, buttonHeight);
        details->button->show();
        m_visible.insert(idDetails, true);
    } else if (details) {
        details->button->hide();
    }

    if (QWidget *paste = button(idPaste)) {
        m_pasteGlow->setGeometry(paste->geometry());
    }
}

QAction *CommandBar::proxy(QMenu *menu, const char *actionName, const QString &text, const char *icon)
{
    QAction *source = sourceAction(name(actionName));
    if (!source) {
        return nullptr;
    }
    auto *p = new QAction(icon ? themeIcon(QString::fromLatin1(icon)) : source->icon(), text, menu);
    p->setCheckable(source->isCheckable());
    p->setChecked(source->isChecked());
    p->setEnabled(source->isEnabled());
    p->setShortcuts(source->shortcuts());
    // Only shown in the menu: the window's own action keeps the key.
    p->setShortcutContext(Qt::WidgetShortcut);
    QPointer<QAction> guard(source);
    connect(p, &QAction::triggered, this, [guard]() {
        if (guard) {
            guard->trigger();
        }
    });
    menu->addAction(p);
    return p;
}

void CommandBar::addFoldedEntry(QMenu *menu, const Entry &entry)
{
    auto *b = entry.button;
    auto *a = new QAction(b->icon(), KLocalizedString::removeAcceleratorMarker(b->text()), menu);
    a->setEnabled(b->isEnabled());
    a->setCheckable(b->isCheckable());
    a->setChecked(b->isChecked());
    if (QMenu *sub = b->menu()) {
        a->setMenu(sub);
    } else {
        QPointer<CommandButton> guard(b);
        connect(a, &QAction::triggered, this, [guard]() {
            if (guard) {
                guard->click();
            }
        });
    }
    menu->addAction(a);
}

void CommandBar::fillSeeMore(QMenu *menu)
{
    menu->clear();
    const QStringList folded = foldedIds();
    for (const QString &id : folded) {
        for (const QList<Entry> *list : {&m_entries, &m_contextEntries}) {
            for (const Entry &e : *list) {
                if (e.id == id) {
                    addFoldedEntry(menu, e);
                }
            }
        }
    }
    if (!folded.isEmpty()) {
        menu->addSeparator();
    }
    proxy(menu, Ids::undo, i18nc("@action:inmenu", "Undo"), "edit-undo");
    proxy(menu, Ids::redo, i18nc("@action:inmenu", "Redo"), "edit-redo");
    menu->addSeparator();
    proxy(menu, Ids::addToPlaces, i18nc("@action:inmenu add the folder to the navigation pane", "Pin to Quick access"), "window-pin");
    proxy(menu, Ids::selectAll, i18nc("@action:inmenu", "Select all"), "edit-select-all");
    proxy(menu, Ids::selectNone, i18nc("@action:inmenu", "Select none"), "edit-select-none");
    proxy(menu, Ids::invertSelection, i18nc("@action:inmenu", "Invert selection"), "edit-select-invert");
    menu->addSeparator();
    proxy(menu, Ids::properties, i18nc("@action:inmenu", "Properties"), "document-properties");
    if (!proxy(menu, Ids::folderOptions, i18nc("@action:inmenu folder options", "Options"), "configure")) {
        proxy(menu, Ids::preferences, i18nc("@action:inmenu folder options", "Options"), "configure");
    }
    proxy(menu, Ids::openTerminal, i18nc("@action:inmenu", "Open in Terminal"), "utilities-terminal");
}

void CommandBar::showSeeMore()
{
    QWidget *b = button(idSeeMore);
    fillSeeMore(m_seeMore);
    const QSize size = m_seeMore->sizeHint();
    const int x = isRightToLeft() ? b->width() - size.width() : 0;
    m_seeMore->popup(b->mapToGlobal(QPoint(x, b->height() + Ui::Metrics::flyoutGap)));
}

QMenu *CommandBar::seeMoreMenu()
{
    fillSeeMore(m_seeMore);
    return m_seeMore;
}

void CommandBar::fillSort(QMenu *menu)
{
    menu->clear();
    auto *roles = new QActionGroup(menu);
    const QList<std::pair<const char *, QString>> main = {
        {Ids::sortByName, i18nc("@action:inmenu sort by", "Name")},
        {Ids::sortByDate, i18nc("@action:inmenu sort by", "Date modified")},
        {Ids::sortByType, i18nc("@action:inmenu sort by", "Type")},
        {Ids::sortBySize, i18nc("@action:inmenu sort by", "Size")},
    };
    for (const auto &[id, text] : main) {
        if (QAction *p = proxy(menu, id, text)) {
            p->setIcon(QIcon());
            p->setActionGroup(roles);
        }
    }

    // The view's other columns, with their own names.
    QAction *sortSource = sourceAction(name(Ids::sort));
    if (sortSource && sortSource->menu()) {
        QList<QAction *> others;
        const QList<QAction *> all = sortSource->menu()->actions();
        for (QAction *a : all) {
            const QString n = a->objectName();
            if (n.startsWith(QLatin1String(Ids::sortByPrefix))
                && std::none_of(main.cbegin(), main.cend(), [&n](const auto &m) {
                       return n == QLatin1String(m.first);
                   })) {
                others << a;
            }
        }
        if (!others.isEmpty()) {
            auto *moreMenu = new QMenu(i18nc("@action:inmenu other ways to sort", "More"), menu);
            for (QAction *a : std::as_const(others)) {
                if (QAction *p = proxy(moreMenu, qPrintable(a->objectName()), KLocalizedString::removeAcceleratorMarker(a->text()))) {
                    p->setIcon(QIcon());
                    p->setActionGroup(roles);
                }
            }
            menu->addMenu(moreMenu);
        }
    }

    menu->addSeparator();
    auto *order = new QActionGroup(menu);
    // Radio marks, no icons: the whole menu reads as one set of choices.
    if (QAction *p = proxy(menu, Ids::ascending, i18nc("@action:inmenu sort order", "Ascending"))) {
        p->setIcon(QIcon());
        p->setActionGroup(order);
    }
    if (QAction *p = proxy(menu, Ids::descending, i18nc("@action:inmenu sort order", "Descending"))) {
        p->setIcon(QIcon());
        p->setActionGroup(order);
    }

    // Group by: Files groups by the column it sorts on.
    QAction *groups = sourceAction(name(Ids::showInGroups));
    if (groups) {
        menu->addSeparator();
        auto *groupMenu = new QMenu(i18nc("@action:inmenu", "Group by"), menu);
        auto *choice = new QActionGroup(groupMenu);
        QPointer<QAction> groupGuard(groups);
        QAction *none = groupMenu->addAction(i18nc("@action:inmenu no groups", "(None)"));
        none->setCheckable(true);
        none->setChecked(!groups->isChecked());
        none->setActionGroup(choice);
        connect(none, &QAction::triggered, this, [groupGuard]() {
            if (groupGuard && groupGuard->isChecked()) {
                groupGuard->trigger();
            }
        });
        for (const auto &[id, text] : main) {
            QAction *role = sourceAction(name(id));
            if (!role) {
                continue;
            }
            QAction *item = groupMenu->addAction(text);
            item->setCheckable(true);
            item->setChecked(groups->isChecked() && role->isChecked());
            item->setActionGroup(choice);
            QPointer<QAction> roleGuard(role);
            connect(item, &QAction::triggered, this, [groupGuard, roleGuard]() {
                if (roleGuard && !roleGuard->isChecked()) {
                    roleGuard->trigger();
                }
                if (groupGuard && !groupGuard->isChecked()) {
                    groupGuard->trigger();
                }
            });
        }
        menu->addMenu(groupMenu);
    }
}

QMenu *CommandBar::sortMenu()
{
    fillSort(m_sort);
    return m_sort;
}

void CommandBar::fillView(QMenu *menu)
{
    menu->clear();
    // Windows' eight views when the window has them, else Dolphin's three.
    const QList<std::pair<const char *, QString>> rayeModes = {
        {Ids::modeExtraLarge, i18nc("@action:inmenu view mode", "Extra large icons")},
        {Ids::modeLarge, i18nc("@action:inmenu view mode", "Large icons")},
        {Ids::modeMedium, i18nc("@action:inmenu view mode", "Medium icons")},
        {Ids::modeSmall, i18nc("@action:inmenu view mode", "Small icons")},
        {Ids::modeList, i18nc("@action:inmenu view mode", "List")},
        {Ids::modeDetails, i18nc("@action:inmenu view mode", "Details")},
        {Ids::modeTiles, i18nc("@action:inmenu view mode", "Tiles")},
        {Ids::modeContent, i18nc("@action:inmenu view mode", "Content")},
    };
    QAction *modes = sourceAction(name(Ids::viewMode));
    if (sourceAction(name(Ids::modeDetails))) {
        auto *group = new QActionGroup(menu);
        for (const auto &[id, text] : rayeModes) {
            if (QAction *p = proxy(menu, id, text)) {
                p->setActionGroup(group);
            }
        }
    } else if (modes && modes->menu()) {
        auto *group = new QActionGroup(menu);
        const QHash<QString, QString> words = {
            {name(Ids::iconsMode), i18nc("@action:inmenu view mode", "Icons")},
            {name(Ids::compactMode), i18nc("@action:inmenu view mode", "List")},
            {name(Ids::detailsMode), i18nc("@action:inmenu view mode", "Details")},
        };
        const QList<QAction *> all = modes->menu()->actions();
        for (QAction *a : all) {
            if (a->isSeparator() || a->objectName().isEmpty()) {
                continue;
            }
            const QString text = words.value(a->objectName(), KLocalizedString::removeAcceleratorMarker(a->text()));
            if (QAction *p = proxy(menu, qPrintable(a->objectName()), text)) {
                p->setActionGroup(group);
            }
        }
    }
    menu->addSeparator();
    proxy(menu, Ids::compactView, i18nc("@action:inmenu less space between items", "Compact view"));

    auto *show = new QMenu(i18nc("@action:inmenu show or hide parts of the window", "Show"), menu);
    proxy(show, Ids::navigationPane, i18nc("@action:inmenu", "Navigation pane"));
    proxy(show, Ids::detailsPane, i18nc("@action:inmenu", "Details pane"));
    proxy(show, Ids::previewPane, i18nc("@action:inmenu", "Preview pane"));
    proxy(show, Ids::itemCheckBoxes, i18nc("@action:inmenu", "Item check boxes"));
    proxy(show, Ids::fileExtensions, i18nc("@action:inmenu", "File name extensions"));
    proxy(show, Ids::showHidden, i18nc("@action:inmenu", "Hidden items"));
    if (show->actions().isEmpty()) {
        delete show;
    } else {
        menu->addMenu(show);
    }
}

QMenu *CommandBar::viewMenu()
{
    fillView(m_view);
    return m_view;
}

void CommandBar::fillNewExtras()
{
    if (!m_newMenu) {
        return;
    }
    for (const QPointer<QAction> &a : std::as_const(m_newExtras)) {
        delete a.data();
    }
    m_newExtras.clear();
    // Windows' order: Folder, Shortcut, the documents, the compressed folder.
    QAction *shortcut = sourceAction(name(Ids::newShortcut));
    if (shortcut) {
        auto *p = new QAction(themeIcon(QStringLiteral("insert-link")), i18nc("@action:inmenu a new shortcut to a file", "Shortcut"), m_newMenu);
        connect(p, &QAction::triggered, shortcut, &QAction::trigger);
        const QList<QAction *> actions = m_newMenu->actions();
        m_newMenu->insertAction(actions.size() > 1 ? actions.at(1) : nullptr, p);
        m_newExtras << p;
    }
    // The folder item comes from KDE's menu; say it the Windows way.
    const QList<QAction *> current = m_newMenu->actions();
    QAction *createDir = sourceAction(QStringLiteral("create_dir"));
    if (!current.isEmpty() && createDir && !createDir->shortcut().isEmpty() && current.first()->shortcut() == createDir->shortcut()) {
        current.first()->setText(i18nc("@action:inmenu a new folder", "Folder"));
    }
    QAction *text = sourceAction(name(Ids::newText));
    if (text) {
        auto *p = new QAction(themeIcon(QStringLiteral("text-plain")), i18nc("@action:inmenu a new empty text file", "Text document"), m_newMenu);
        connect(p, &QAction::triggered, text, &QAction::trigger);
        m_newMenu->addAction(p);
        m_newExtras << p;
    }
    QAction *zip = sourceAction(name(Ids::newZip));
    if (zip) {
        auto *p = new QAction(themeIcon(QStringLiteral("application-zip")), i18nc("@action:inmenu", "Compressed (zipped) folder"), m_newMenu);
        connect(p, &QAction::triggered, zip, &QAction::trigger);
        m_newMenu->addAction(p);
        m_newExtras << p;
    }
}

QSize CommandBar::sizeHint() const
{
    return QSize(1000, rowHeight);
}

QSize CommandBar::minimumSizeHint() const
{
    return QSize(2 * margin + 2 * Ui::Metrics::iconButtonWidth, rowHeight);
}

void CommandBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayout();
}

void CommandBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::LanguageChange) {
        relayout();
    }
}

}
