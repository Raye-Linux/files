/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The widget kit in the harness: every control in every state, a command
// bar, an open flyout, an open dialog and the QML versions. Built into
// tests/raye/test_ui_harness.cpp; no Q_OBJECT here.

#include "harness.h"

#include "ui/carddialog.h"
#include "ui/commandbutton.h"
#include "ui/flyout.h"
#include "ui/focusring.h"
#include "ui/pivotbar.h"
#include "ui/separator.h"
#include "ui/showcase.h"
#include "ui/splitbutton.h"
#include "ui/tokens.h"
#include "ui/usagebar.h"

#include <KIconEngine>
#include <KIconLoader>

#include <QApplication>
#include <QEnterEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>
#include <tuple>

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
#include <KLocalizedContext>
QT_WARNING_POP

namespace RayeHarness
{

namespace
{
using namespace Raye::Ui;

enum class State {
    Normal,
    Hover,
    Pressed,
    Disabled,
    Focus,
    Checked,
};

const State s_states[] = {State::Normal, State::Hover, State::Pressed, State::Disabled, State::Focus, State::Checked};

QString stateName(State state)
{
    switch (state) {
    case State::Normal:
        return text("Normal", "Normal");
    case State::Hover:
        return text("Hover", "Survol");
    case State::Pressed:
        return text("Pressed", "Appuyé");
    case State::Disabled:
        return text("Disabled", "Désactivé");
    case State::Focus:
        return text("Keyboard focus", "Focus clavier");
    case State::Checked:
        return text("Checked", "Coché");
    }
    return {};
}

QLabel *caption(const QString &words, QWidget *parent)
{
    auto *label = new QLabel(words, parent);
    label->setFont(textFont(TextStyle::Caption));
    QPalette p = label->palette();
    p.setColor(QPalette::WindowText, Tokens::current().color(TokenSet::textSecondary));
    label->setPalette(p);
    return label;
}

QLabel *heading(const QString &words, QWidget *parent)
{
    auto *label = new QLabel(words, parent);
    label->setFont(textFont(TextStyle::BodyStrong));
    return label;
}

// Puts a button in a state the way the user would: the pointer over it,
// held down, disabled, reached with Tab.
void putInState(QAbstractButton *button, State state)
{
    switch (state) {
    case State::Normal:
        break;
    case State::Hover: {
        QEnterEvent enter(QPointF(4, 4), QPointF(4, 4), QPointF(4, 4));
        QApplication::sendEvent(button, &enter);
        break;
    }
    case State::Pressed:
        button->setDown(true);
        break;
    case State::Disabled:
        button->setEnabled(false);
        break;
    case State::Focus:
        if (FocusRing *ring = FocusRing::of(button)) {
            ring->showRing();
        }
        break;
    case State::Checked:
        button->setCheckable(true);
        button->setChecked(true);
        break;
    }
}

// Through KIconEngine, as in a Plasma session: symbolic icons take the
// text colour of the palette instead of the colour baked into the file.
QIcon themeIcon(const QString &name)
{
    return name.isEmpty() ? QIcon() : QIcon(new KIconEngine(name, KIconLoader::global()));
}

CommandButton *button(const QString &icon, const QString &label, CommandButton::Kind kind, QWidget *parent)
{
    auto *b = new CommandButton(themeIcon(icon), label, parent);
    b->setKind(kind);
    return b;
}

// A painted surface: the window's layer (content area, selected tab) or a card.
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
        p.setPen(QPen(Tokens::current().color(TokenSet::strokeCardSolid), 1));
        p.setBrush(Tokens::current().color(m_role));
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Metrics::radiusOverlay, Metrics::radiusOverlay);
    }

private:
    TokenSet::Role m_role;
};

QMenu *sampleMenu(QWidget *parent, const QList<std::pair<const char *, const char *>> &items)
{
    auto *menu = new QMenu(parent);
    for (const auto &[en, fr] : items) {
        menu->addAction(text(en, fr));
    }
    return menu;
}

// The command bar of a normal folder (explorer spec 1.3, 3.4).
QWidget *commandBar(QWidget *parent, CommandButton **shareOut = nullptr)
{
    auto *bar = new QWidget(parent);
    bar->setFixedHeight(48);
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(8, 8, 8, 8);
    row->setSpacing(4);

    auto *newButton = button(QStringLiteral("list-add"), text("New", "Nouveau"), CommandButton::Subtle, bar);
    newButton->setMenu(sampleMenu(bar, {{"Folder", "Dossier"}, {"Shortcut", "Raccourci"}, {"Text document", "Document texte"}}));
    row->addWidget(newButton);
    row->addWidget(new Separator(Qt::Vertical, bar));

    struct Command {
        const char *icon;
        const char *en;
        const char *fr;
        const char *keys;
        bool enabled;
    };
    const Command commands[] = {
        {"edit-cut", "Cut", "Couper", "Ctrl+X", true},
        {"edit-copy", "Copy", "Copier", "Ctrl+C", true},
        {"edit-paste", "Paste", "Coller", "Ctrl+V", false},
        {"edit-rename", "Rename", "Renommer", "F2", true},
        {"emblem-shared-symbolic", "Share", "Partager", "", true},
        {"edit-delete", "Delete", "Supprimer", "Del", true},
    };
    for (const Command &c : commands) {
        auto *b = button(QString::fromLatin1(c.icon), text(c.en, c.fr), CommandButton::Subtle, bar);
        b->setIconOnly(true);
        b->setShortcutHint(QKeySequence(QString::fromLatin1(c.keys)));
        b->setEnabled(c.enabled);
        row->addWidget(b);
        if (shareOut && QByteArray(c.en) == "Share") {
            *shareOut = b;
        }
    }
    row->addWidget(new Separator(Qt::Vertical, bar));
    auto *sort = button(QStringLiteral("view-sort"), text("Sort", "Trier"), CommandButton::Subtle, bar);
    sort->setMenu(sampleMenu(bar, {{"Name", "Nom"}, {"Date modified", "Date de modification"}, {"Type", "Type"}, {"Size", "Taille"}}));
    auto *view = button(QStringLiteral("view-list-details"), text("View", "Affichage"), CommandButton::Subtle, bar);
    view->setMenu(sampleMenu(bar, {{"Large icons", "Grandes icônes"}, {"List", "Liste"}, {"Details", "Détails"}}));
    row->addWidget(sort);
    row->addWidget(view);
    row->addWidget(new Separator(Qt::Vertical, bar));
    auto *more = button(QStringLiteral("overflow-menu"), text("See more", "Voir plus"), CommandButton::Subtle, bar);
    more->setIconOnly(true);
    row->addWidget(more);
    row->addStretch();
    auto *details = button(QStringLiteral("view-split-left-right"), text("Details", "Détails"), CommandButton::Subtle, bar);
    details->setCheckable(true);
    row->addWidget(details);
    return bar;
}

QWidget *kit(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *column = new QVBoxLayout(page);
    column->setContentsMargins(32, 24, 32, 24);
    column->setSpacing(12);

    auto *title = new QLabel(text("Raye widget kit", "Kit de contrôles Raye"), page);
    title->setFont(textFont(TextStyle::Subtitle));
    column->addWidget(title);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(14);
    for (int c = 0; c < int(std::size(s_states)); ++c) {
        grid->addWidget(caption(stateName(s_states[c]), page), 0, c + 1);
    }

    struct Row {
        const char *en;
        const char *fr;
        std::function<QWidget *(QWidget *, State)> make;
    };
    const Row rows[] = {
        {"Icon button", "Bouton icône", [](QWidget *p, State s) {
             auto *b = button(QStringLiteral("edit-copy"), text("Copy", "Copier"), CommandButton::Subtle, p);
             b->setIconOnly(true);
             putInState(b, s);
             return b;
         }},
        {"Command button", "Bouton de commande", [](QWidget *p, State s) {
             auto *b = button(QStringLiteral("view-sort"), text("Sort", "Trier"), CommandButton::Subtle, p);
             b->setShowChevron(true);
             putInState(b, s);
             return b;
         }},
        {"Standard button", "Bouton standard", [](QWidget *p, State s) {
             auto *b = button(QString(), text("Cancel", "Annuler"), CommandButton::Standard, p);
             b->setMinimumWidth(96);
             putInState(b, s);
             return b;
         }},
        {"Accent button", "Bouton principal", [](QWidget *p, State s) {
             auto *b = button(QString(), text("Save", "Enregistrer"), CommandButton::Accent, p);
             b->setMinimumWidth(96);
             putInState(b, s);
             return b;
         }},
        {"Split button", "Bouton partagé", [](QWidget *p, State s) {
             auto *b = new SplitButton(themeIcon(QStringLiteral("archive-extract")), text("Extract all", "Tout extraire"), p);
             if (s == State::Disabled) {
                 b->setEnabled(false);
             } else if (s == State::Focus) {
                 FocusRing::of(b)->showRing();
             } else if (s == State::Checked) {
                 b->setKind(CommandButton::Standard);
             } else {
                 putInState(b->mainButton(), s);
             }
             return static_cast<QWidget *>(b);
         }},
    };
    int r = 1;
    for (const Row &row : rows) {
        grid->addWidget(caption(text(row.en, row.fr), page), r, 0);
        for (int c = 0; c < int(std::size(s_states)); ++c) {
            grid->addWidget(row.make(page, s_states[c]), r, c + 1, Qt::AlignLeft | Qt::AlignVCenter);
        }
        ++r;
    }
    grid->setColumnStretch(int(std::size(s_states)) + 1, 1);
    column->addLayout(grid);

    column->addSpacing(8);
    column->addWidget(heading(text("Command bar", "Barre de commandes"), page));
    auto *layer = new Surface(TokenSet::bgLayerSolid, page);
    auto *layerRow = new QVBoxLayout(layer);
    layerRow->setContentsMargins(0, 0, 0, 0);
    layerRow->addWidget(commandBar(layer));
    column->addWidget(layer);

    auto *lower = new QGridLayout;
    lower->setHorizontalSpacing(48);
    lower->setVerticalSpacing(8);
    lower->addWidget(heading(text("Pivot", "Onglets"), page), 0, 0, Qt::AlignLeft | Qt::AlignTop);
    auto *pivot = new PivotBar(page);
    pivot->addItem(text("Recent", "Récents"));
    pivot->addItem(text("Favorites", "Favoris"));
    pivot->addItem(text("Shared", "Partagés"));
    pivot->setCurrentIndex(1);
    lower->addWidget(pivot, 1, 0, Qt::AlignLeft | Qt::AlignTop);

    lower->addWidget(heading(text("Drive usage", "Espace utilisé"), page), 0, 1, Qt::AlignLeft | Qt::AlignTop);
    auto *usage = new QVBoxLayout;
    usage->setSpacing(6);
    for (const auto &[used, en, fr] : {std::tuple{0.45, "212 GB free of 476 GB", "212 Go libres sur 476 Go"}, std::tuple{0.95, "11.8 GB free of 238 GB", "11,8 Go libres sur 238 Go"}}) {
        auto *bar = new UsageBar(page);
        bar->setAnimateOnShow(false);
        bar->setValue(used);
        bar->setFixedWidth(240);
        usage->addWidget(bar);
        usage->addWidget(caption(text(en, fr), page));
        usage->addSpacing(6);
    }
    lower->addLayout(usage, 1, 1, Qt::AlignTop);
    lower->setColumnStretch(2, 1);
    column->addLayout(lower);
    column->addStretch();
    return page;
}

// A window with the command bar on its layer, as Files' content area.
QWidget *folderWindow(QWidget *parent, CommandButton **share)
{
    auto *page = new QWidget(parent);
    auto *column = new QVBoxLayout(page);
    column->setContentsMargins(16, 16, 16, 16);
    auto *layer = new Surface(TokenSet::bgLayerSolid, page);
    auto *inner = new QVBoxLayout(layer);
    inner->setContentsMargins(0, 0, 0, 0);
    inner->addWidget(commandBar(layer, share));
    auto *empty = new QLabel(text("This folder is empty.", "Ce dossier est vide."), layer);
    empty->setAlignment(Qt::AlignCenter);
    QPalette p = empty->palette();
    p.setColor(QPalette::WindowText, Tokens::current().color(TokenSet::textSecondary));
    empty->setPalette(p);
    inner->addWidget(empty, 1);
    column->addWidget(layer);
    return page;
}

QWidget *flyoutDemo(QWidget *parent)
{
    CommandButton *share = nullptr;
    QWidget *page = folderWindow(parent, &share);
    auto *flyout = new Flyout(page);
    flyout->setTranslucent(compositing());
    auto *content = new QWidget;
    auto *list = new QVBoxLayout(content);
    list->setContentsMargins(8, 8, 8, 8);
    list->setSpacing(2);
    QLabel *title = heading(text("Share", "Partager"), content);
    title->setContentsMargins(12, 0, 0, 4); // in line with the rows' icons
    list->addWidget(title);
    for (const auto &[icon, en, fr] : {std::tuple{"edit-copy", "Copy link", "Copier le lien"},
                                        std::tuple{"mail-message-new", "Send by email", "Envoyer par e-mail"},
                                        std::tuple{"preferences-system-bluetooth", "Bluetooth", "Bluetooth"},
                                        std::tuple{"application-x-executable", "More apps", "Plus d'applications"}}) {
        auto *b = button(QString::fromLatin1(icon), text(en, fr), CommandButton::Subtle, content);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setContentAlignment(Qt::AlignLeft);
        b->setMinimumWidth(220);
        list->addWidget(b);
    }
    flyout->setContentWidget(content);
    QPointer<CommandButton> anchor(share);
    QTimer::singleShot(0, flyout, [flyout, anchor]() {
        if (anchor) {
            flyout->showAt(anchor);
        }
    });
    return page;
}

QWidget *dialogDemo(QWidget *parent)
{
    QWidget *page = folderWindow(parent, nullptr);
    auto *dialog = new CardDialog(page);
    dialog->setTranslucent(compositing());
    dialog->setTitle(text("Delete file", "Supprimer le fichier"));
    dialog->setMessage(text("Are you sure you want to permanently delete this file?\n\nreport-2026.pdf", "Voulez-vous vraiment supprimer définitivement ce fichier ?\n\nrapport-2026.pdf"));
    dialog->setButtonText(CardDialog::Primary, text("Yes", "Oui"));
    dialog->setButtonText(CardDialog::Close, text("No", "Non"));
    dialog->setFixedWidth(480);
    QTimer::singleShot(0, dialog, [dialog, page]() {
        dialog->adjustSize();
        const QPoint centre = page->window()->mapToGlobal(page->window()->rect().center());
        dialog->move(centre - QPoint(dialog->width() / 2, dialog->height() / 2));
        dialog->show();
    });
    return page;
}

QWidget *qmlKit(QWidget *parent, bool dialog)
{
    auto *view = new QQuickWidget(parent);
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->engine()->addImportPath(QStringLiteral(RAYE_FILES_BINARY_DIR "/bin"));
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    auto *context = new KLocalizedContext(view->engine());
    QT_WARNING_POP
    context->setTranslationDomain(QStringLiteral("raye-files"));
    view->engine()->rootContext()->setContextObject(context);
    view->setClearColor(Tokens::current().color(TokenSet::bgBase));
    view->setInitialProperties({{QStringLiteral("french"), language().startsWith(QLatin1String("fr"))}, {QStringLiteral("showDialog"), dialog}});
    view->setSource(QUrl::fromLocalFile(QStringLiteral(RAYE_FILES_SOURCE_DIR "/tests/raye/harness/KitGallery.qml")));
    return view;
}
}

void registerKitShowcases()
{
    using Raye::Ui::Showcase::add;
    add(QStringLiteral("Kit"), QStringLiteral("every control of the widget kit in every state"), kit);
    add(QStringLiteral("CommandBar"), QStringLiteral("the command bar of a folder"), [](QWidget *parent) {
        return folderWindow(parent, nullptr);
    });
    add(QStringLiteral("Flyout"), QStringLiteral("a flyout open under the Share button"), flyoutDemo);
    add(QStringLiteral("CardDialog"), QStringLiteral("a dialog open over a folder"), dialogDemo);
    add(QStringLiteral("KitQml"), QStringLiteral("the QML controls (org.raye.files) in their states"), [](QWidget *parent) {
        return qmlKit(parent, false);
    });
    add(QStringLiteral("KitQmlDialog"), QStringLiteral("the QML card dialog open over a page"), [](QWidget *parent) {
        return qmlKit(parent, true);
    });
}

}
