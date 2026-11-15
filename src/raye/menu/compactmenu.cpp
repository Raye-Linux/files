/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "compactmenu.h"

#include "ui/motion.h"
#include "ui/tokens.h"

#include <KIconEngine>
#include <KIconLoader>
#include <KLocalizedString>

#include <QAbstractButton>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QWidgetAction>

namespace Raye::Menu
{
using Ui::TokenSet;
using Ui::Tokens;

namespace
{
QIcon themeIcon(const QString &name)
{
    return name.isEmpty() ? QIcon() : QIcon(new KIconEngine(name, KIconLoader::global()));
}

// "Ctrl+X" for a tooltip, in the user's words for the keys.
QString keysText(const QKeySequence &keys)
{
    return keys.toString(QKeySequence::NativeText);
}
}

// The row of icon buttons at the top of the menu: 48 x 48 each, the glyph
// over its name in Caption, the hover fill with radius 4 (labels like
// Windows 11 24H2). Left and Right move between them, Enter acts.
class IconRow : public QWidget
{
public:
    static constexpr int buttonSize = 48;
    static constexpr int maxButtonWidth = 80;
    static constexpr int gap = 2;

    explicit IconRow(QWidget *parent)
        : QWidget(parent)
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 2, 4, 2);
        layout->setSpacing(gap);
        layout->addStretch(1);
        setFocusPolicy(Qt::StrongFocus);
    }

    void addButton(QAction *action, const std::function<void()> &done)
    {
        auto *button = new RowButton(action, this);
        connect(button, &QAbstractButton::clicked, this, [action, done] {
            if (action->isEnabled()) {
                done();
                action->trigger();
            }
        });
        auto *layout = static_cast<QHBoxLayout *>(this->layout());
        layout->insertWidget(layout->count() - 1, button);
        m_buttons.append(button);

        // One width for the row: 48, or more for a longer name ("Supprimer"),
        // so names are not cut short.
        const QFontMetrics fm(Ui::textFont(Ui::TextStyle::Caption));
        int width = buttonSize;
        for (RowButton *b : m_buttons) {
            width = qMax(width, fm.horizontalAdvance(b->action()->text()) + 8);
        }
        width = qMin(width, maxButtonWidth);
        for (RowButton *b : m_buttons) {
            b->setFixedSize(width, buttonSize);
        }
    }

    QList<QAction *> actions() const
    {
        QList<QAction *> out;
        for (RowButton *b : m_buttons) {
            out.append(b->action());
        }
        return out;
    }

    QSize sizeHint() const override
    {
        int width = 8;
        for (RowButton *b : m_buttons) {
            width += b->width() + gap;
        }
        return QSize(width - (m_buttons.isEmpty() ? 0 : gap), buttonSize + 4);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (m_buttons.isEmpty()) {
            QWidget::keyPressEvent(event);
            return;
        }
        switch (event->key()) {
        case Qt::Key_Left:
            moveFocus(-1);
            return;
        case Qt::Key_Right:
            moveFocus(1);
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (m_focus >= 0 && m_focus < m_buttons.size()) {
                m_buttons.at(m_focus)->click();
            }
            return;
        default:
            // Up and Down belong to the menu.
            event->ignore();
        }
    }

    void focusInEvent(QFocusEvent *event) override
    {
        if (m_focus < 0) {
            moveFocus(1);
        }
        QWidget::focusInEvent(event);
    }

    void focusOutEvent(QFocusEvent *event) override
    {
        setFocusIndex(-1);
        QWidget::focusOutEvent(event);
    }

private:
    class RowButton : public QAbstractButton
    {
    public:
        RowButton(QAction *action, IconRow *row)
            : QAbstractButton(row)
            , m_action(action)
            , m_row(row)
        {
            setFixedSize(buttonSize, buttonSize);
            setFocusPolicy(Qt::NoFocus);
            setAttribute(Qt::WA_Hover);
            setObjectName(action->objectName());
            setAccessibleName(action->text());
            const QString hint = action->shortcut().isEmpty() ? QString() : QStringLiteral(" (%1)").arg(keysText(action->shortcut()));
            setToolTip(action->text() + hint);
            setEnabled(action->isEnabled());
            connect(action, &QAction::changed, this, [this] {
                setEnabled(m_action->isEnabled());
                update();
            });
        }

        QAction *action() const
        {
            return m_action;
        }

    protected:
        bool event(QEvent *event) override
        {
            if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave) {
                update();
            }
            return QAbstractButton::event(event);
        }

        void paintEvent(QPaintEvent *) override
        {
            const TokenSet &t = Tokens::current();
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            const bool hot = isEnabled() && (underMouse() || m_row->focusedButton() == this);
            if (hot || isDown()) {
                p.setPen(Qt::NoPen);
                p.setBrush(t.color(isDown() ? TokenSet::fillSubtlePressed : TokenSet::fillSubtleHover));
                p.drawRoundedRect(QRectF(rect()), Ui::Metrics::radiusControl, Ui::Metrics::radiusControl);
            }
            if (m_row->focusedButton() == this) {
                p.setPen(QPen(t.color(TokenSet::focusOuter), 1.5));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), Ui::Metrics::radiusControl, Ui::Metrics::radiusControl);
            }
            const QRect icon(width() / 2 - 8, 8, 16, 16);
            m_action->icon().paint(&p, icon, Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled);
            p.setFont(Ui::textFont(Ui::TextStyle::Caption));
            p.setPen(t.color(isEnabled() ? TokenSet::textPrimary : TokenSet::textDisabled));
            const QFontMetrics fm(p.font());
            const QRect label(1, 27, width() - 2, 16);
            p.drawText(label, Qt::AlignHCenter | Qt::AlignVCenter, fm.elidedText(m_action->text(), Qt::ElideRight, label.width()));
        }

    private:
        QAction *m_action;
        IconRow *m_row;
    };

    void moveFocus(int step)
    {
        const int n = int(m_buttons.size());
        for (int k = 1; k <= n; ++k) {
            const int i = ((m_focus < 0 ? (step > 0 ? -1 : 0) : m_focus) + step * k + n * 2) % n;
            if (m_buttons.at(i)->isEnabled()) {
                setFocusIndex(i);
                return;
            }
        }
    }

    void setFocusIndex(int index)
    {
        m_focus = index;
        for (RowButton *b : m_buttons) {
            b->update();
        }
    }

public:
    QAbstractButton *focusedButton() const
    {
        return hasFocus() && m_focus >= 0 && m_focus < m_buttons.size() ? m_buttons.at(m_focus) : nullptr;
    }

private:
    QList<RowButton *> m_buttons;
    int m_focus = -1;
};

QString CompactMenu::defaultText(const QString &id)
{
    static const QHash<QString, QString> texts = {
        {QStringLiteral("cut"), i18nc("@action:inmenu", "Cut")},
        {QStringLiteral("copy"), i18nc("@action:inmenu", "Copy")},
        {QStringLiteral("paste"), i18nc("@action:inmenu", "Paste")},
        {QStringLiteral("rename"), i18nc("@action:inmenu", "Rename")},
        {QStringLiteral("share"), i18nc("@action:inmenu", "Share")},
        {QStringLiteral("delete"), i18nc("@action:inmenu", "Delete")},
        {QStringLiteral("open"), i18nc("@action:inmenu", "Open")},
        {QStringLiteral("open_with"), i18nc("@action:inmenu", "Open with")},
        {QStringLiteral("open_new_tab"), i18nc("@action:inmenu", "Open in new tab")},
        {QStringLiteral("open_new_window"), i18nc("@action:inmenu", "Open in new window")},
        {QStringLiteral("open_file_location"), i18nc("@action:inmenu", "Open file location")},
        {QStringLiteral("pin_quick_access"), i18nc("@action:inmenu", "Pin to Quick access")},
        {QStringLiteral("map_drive"), i18nc("@action:inmenu", "Map network drive…")},
        {QStringLiteral("disconnect_drive"), i18nc("@action:inmenu remove a network drive", "Disconnect")},
        {QStringLiteral("favorite"), i18nc("@action:inmenu", "Add to Favorites")},
        {QStringLiteral("compress"), i18nc("@action:inmenu", "Compress to")},
        {QStringLiteral("compress_zip"), i18nc("@action:inmenu compress to", "ZIP file")},
        {QStringLiteral("compress_7z"), i18nc("@action:inmenu compress to", "7z file")},
        {QStringLiteral("compress_tar"), i18nc("@action:inmenu compress to", "tar.zst file")},
        {QStringLiteral("compress_options"), i18nc("@action:inmenu compress to", "Additional options…")},
        {QStringLiteral("extract_all"), i18nc("@action:inmenu", "Extract all…")},
        {QStringLiteral("extract_here"), i18nc("@action:inmenu", "Extract here")},
        {QStringLiteral("extract_to"), i18nc("@action:inmenu", "Extract to a new folder")},
        {QStringLiteral("set_background"), i18nc("@action:inmenu", "Set as desktop background")},
        {QStringLiteral("rotate_right"), i18nc("@action:inmenu", "Rotate right")},
        {QStringLiteral("rotate_left"), i18nc("@action:inmenu", "Rotate left")},
        {QStringLiteral("copy_path"), i18nc("@action:inmenu", "Copy as path")},
        {QStringLiteral("create_shortcut"), i18nc("@action:inmenu", "Create shortcut")},
        {QStringLiteral("open_terminal"), i18nc("@action:inmenu", "Open in Terminal")},
        {QStringLiteral("eject"), i18nc("@action:inmenu eject a drive or disc", "Eject")},
        {QStringLiteral("format"), i18nc("@action:inmenu erase a drive and prepare it", "Format…")},
        {QStringLiteral("restore"), i18nc("@action:inmenu put back from the Recycle Bin", "Restore")},
        {QStringLiteral("empty_bin"), i18nc("@action:inmenu", "Empty Recycle Bin")},
        {QStringLiteral("view"), i18nc("@action:inmenu", "View")},
        {QStringLiteral("sort"), i18nc("@action:inmenu", "Sort by")},
        {QStringLiteral("group"), i18nc("@action:inmenu", "Group by")},
        {QStringLiteral("refresh"), i18nc("@action:inmenu", "Refresh")},
        {QStringLiteral("undo"), i18nc("@action:inmenu", "Undo")},
        {QStringLiteral("new"), i18nc("@action:inmenu create a new item", "New")},
        {QStringLiteral("properties"), i18nc("@action:inmenu", "Properties")},
        {QStringLiteral("more_options"), i18nc("@action:inmenu", "Show more options")},
        {QStringLiteral("open_with_choose"), i18nc("@action:inmenu open with", "Choose another app")},
        {QStringLiteral("open_admin"), i18nc("@action:inmenu", "Open as administrator")},
        // the words of an item that is already done
        {QStringLiteral("unpin_quick_access"), i18nc("@action:inmenu", "Unpin from Quick access")},
        {QStringLiteral("unfavorite"), i18nc("@action:inmenu", "Remove from Favorites")},
    };
    return texts.value(id);
}

QString CompactMenu::iconName(const QString &id)
{
    static const QHash<QString, QString> icons = {
        {QStringLiteral("cut"), QStringLiteral("edit-cut")},
        {QStringLiteral("copy"), QStringLiteral("edit-copy")},
        {QStringLiteral("paste"), QStringLiteral("edit-paste")},
        {QStringLiteral("rename"), QStringLiteral("edit-rename")},
        {QStringLiteral("share"), QStringLiteral("document-share")},
        {QStringLiteral("delete"), QStringLiteral("edit-delete")},
        {QStringLiteral("open"), QStringLiteral("document-open")},
        {QStringLiteral("open_with"), QStringLiteral("system-run")},
        {QStringLiteral("open_new_tab"), QStringLiteral("tab-new")},
        {QStringLiteral("open_new_window"), QStringLiteral("window-new")},
        {QStringLiteral("open_file_location"), QStringLiteral("document-open-folder")},
        {QStringLiteral("pin_quick_access"), QStringLiteral("window-pin")},
        {QStringLiteral("map_drive"), QStringLiteral("folder-network")},
        {QStringLiteral("disconnect_drive"), QStringLiteral("network-disconnect")},
        {QStringLiteral("favorite"), QStringLiteral("emblem-favorite")},
        {QStringLiteral("compress"), QStringLiteral("archive-insert")},
        {QStringLiteral("extract_all"), QStringLiteral("archive-extract")},
        {QStringLiteral("extract_here"), QStringLiteral("archive-extract")},
        {QStringLiteral("extract_to"), QStringLiteral("folder-new")},
        {QStringLiteral("set_background"), QStringLiteral("preferences-desktop-wallpaper")},
        {QStringLiteral("rotate_right"), QStringLiteral("object-rotate-right")},
        {QStringLiteral("rotate_left"), QStringLiteral("object-rotate-left")},
        {QStringLiteral("copy_path"), QStringLiteral("edit-copy-path")},
        {QStringLiteral("create_shortcut"), QStringLiteral("insert-link")},
        {QStringLiteral("open_terminal"), QStringLiteral("utilities-terminal")},
        {QStringLiteral("eject"), QStringLiteral("media-eject")},
        {QStringLiteral("format"), QStringLiteral("drive-harddisk")},
        {QStringLiteral("restore"), QStringLiteral("edit-reset")},
        {QStringLiteral("empty_bin"), QStringLiteral("trash-empty")},
        {QStringLiteral("view"), QStringLiteral("view-list-icons")},
        {QStringLiteral("sort"), QStringLiteral("view-sort")},
        {QStringLiteral("group"), QStringLiteral("view-group")},
        {QStringLiteral("refresh"), QStringLiteral("view-refresh")},
        {QStringLiteral("undo"), QStringLiteral("edit-undo")},
        {QStringLiteral("new"), QStringLiteral("list-add")},
        {QStringLiteral("properties"), QStringLiteral("document-properties")},
        {QStringLiteral("more_options"), QStringLiteral("overflow-menu")},
        {QStringLiteral("open_with_choose"), QStringLiteral("application-x-executable")},
        {QStringLiteral("open_admin"), QStringLiteral("system-lock-screen")},
    };
    return icons.value(id);
}

QKeySequence CompactMenu::shortcutHint(const QString &id)
{
    if (id == QLatin1String("cut")) {
        return QKeySequence(Qt::CTRL | Qt::Key_X);
    }
    if (id == QLatin1String("copy")) {
        return QKeySequence(Qt::CTRL | Qt::Key_C);
    }
    if (id == QLatin1String("paste")) {
        return QKeySequence(Qt::CTRL | Qt::Key_V);
    }
    if (id == QLatin1String("rename")) {
        return QKeySequence(Qt::Key_F2);
    }
    if (id == QLatin1String("delete")) {
        return QKeySequence(Qt::Key_Delete);
    }
    if (id == QLatin1String("copy_path")) {
        return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C);
    }
    if (id == QLatin1String("refresh")) {
        return QKeySequence(Qt::Key_F5);
    }
    if (id == QLatin1String("undo")) {
        return QKeySequence(Qt::CTRL | Qt::Key_Z);
    }
    if (id == QLatin1String("properties")) {
        return QKeySequence(Qt::ALT | Qt::Key_Return);
    }
    if (id == QLatin1String("more_options")) {
        return QKeySequence(Qt::SHIFT | Qt::Key_F10);
    }
    return QKeySequence();
}

CompactMenu::CompactMenu(const Plan &plan, const Hooks &hooks, QWidget *parent)
    : CompactMenu(parent)
{
    build(plan, hooks);
}

CompactMenu::CompactMenu(QWidget *parent)
    : QMenu(parent)
{
    setObjectName(QStringLiteral("raye_context_menu"));
    setSeparatorsCollapsible(true);
}

void CompactMenu::build(const Plan &plan, const Hooks &hooks)
{
    if (!m_actions.isEmpty() || m_row) {
        return;
    }
    m_hooks = hooks;

    if (!plan.row.isEmpty()) {
        m_row = new IconRow(this);
        for (const QString &id : plan.row) {
            QAction *a = makeAction(id, this);
            m_row->addButton(a, [this] {
                // The whole menu closes, then the action runs.
                close();
            });
            m_rowIds.append(id);
        }
        m_rowAction = new QWidgetAction(this);
        m_rowAction->setObjectName(QStringLiteral("icon_row"));
        m_rowAction->setDefaultWidget(m_row);
        addAction(m_rowAction);
        if (!plan.items.isEmpty()) {
            addSeparator();
        }
    }

    for (const QString &id : plan.items) {
        addItem(id);
    }
    // No separator at the end, nor two in a row, when an item was left out.
    while (!m_itemIds.isEmpty() && m_itemIds.last() == QLatin1String("-")) {
        m_itemIds.removeLast();
    }
}

CompactMenu::~CompactMenu() = default;

QAction *CompactMenu::makeAction(const QString &id, QObject *owner)
{
    QString text = m_hooks.text ? m_hooks.text(id) : QString();
    if (text.isEmpty()) {
        text = defaultText(id);
    }
    auto *a = new QAction(themeIcon(iconName(id)), text, owner);
    a->setObjectName(id);
    const QKeySequence keys = shortcutHint(id);
    if (!keys.isEmpty()) {
        // A hint: the window's own action has the shortcut.
        a->setShortcut(keys);
        a->setShortcutContext(Qt::WidgetShortcut);
    }

    QAction *source = m_hooks.action ? m_hooks.action(id) : nullptr;
    if (source) {
        a->setEnabled(source->isEnabled());
        connect(source, &QAction::changed, a, [a, source] {
            a->setEnabled(source->isEnabled());
        });
        connect(a, &QAction::triggered, source, [source] {
            source->trigger();
        });
    } else {
        a->setEnabled(m_hooks.enabled ? m_hooks.enabled(id) : true);
        connect(a, &QAction::triggered, this, [this, id] {
            if (m_hooks.trigger) {
                m_hooks.trigger(id);
            }
        });
    }
    m_actions.insert(id, a);
    return a;
}

QMenu *CompactMenu::compressMenu()
{
    auto *menu = new QMenu(this);
    for (const char *id : {"compress_zip", "compress_7z", "compress_tar"}) {
        QAction *a = makeAction(QString::fromLatin1(id), menu);
        a->setIcon(QIcon());
        menu->addAction(a);
    }
    menu->addSeparator();
    QAction *more = makeAction(QStringLiteral("compress_options"), menu);
    more->setIcon(QIcon());
    menu->addAction(more);
    return menu;
}

void CompactMenu::addItem(const QString &id)
{
    if (id == QLatin1String("-")) {
        if (!m_itemIds.isEmpty() && m_itemIds.last() != QLatin1String("-")) {
            addSeparator();
            m_itemIds.append(id);
        }
        return;
    }

    // A hook may leave an item out (no rotate service, no terminal...).
    if (m_hooks.available && !m_hooks.available(id)) {
        return;
    }

    static const QStringList submenus = {QStringLiteral("open_with"), QStringLiteral("compress"), QStringLiteral("view"), QStringLiteral("sort"),
                                         QStringLiteral("group"), QStringLiteral("new")};
    if (submenus.contains(id)) {
        QMenu *content = m_hooks.submenu ? m_hooks.submenu(id, this) : nullptr;
        if (!content && id == QLatin1String("compress")) {
            content = compressMenu();
        }
        if (!content) {
            return;
        }
        QString text = m_hooks.text ? m_hooks.text(id) : QString();
        content->setTitle(text.isEmpty() ? defaultText(id) : text);
        content->setIcon(themeIcon(iconName(id)));
        QAction *a = addMenu(content);
        a->setObjectName(id);
        a->setEnabled(!content->isEmpty() && content->isEnabled());
        m_actions.insert(id, a);
        m_itemIds.append(id);
        return;
    }

    QAction *a = makeAction(id, this);
    if (id == QLatin1String("open")) {
        QFont bold = a->font();
        bold.setBold(true);
        a->setFont(bold);
    }
    addAction(a);
    m_itemIds.append(id);
}

QStringList CompactMenu::rowIds() const
{
    return m_rowIds;
}

QStringList CompactMenu::itemIds() const
{
    return m_itemIds;
}

QAction *CompactMenu::item(const QString &id) const
{
    return m_actions.value(id);
}

QWidget *CompactMenu::iconRow() const
{
    return m_row;
}

void CompactMenu::showEvent(QShowEvent *event)
{
    QMenu::showEvent(event);
    m_shownAt = pos();
    // The Raye popup motion: a short fade in. On Wayland the compositor
    // animates popups itself, and a window there has no opacity to set.
    if (QGuiApplication::platformName() == QLatin1String("xcb") && Ui::Motion::enabled()) {
        setWindowOpacity(0.0);
        Ui::Motion::run(this, Ui::Motion::Fast, [this](qreal t) {
            setWindowOpacity(t);
        });
    }
}

}
