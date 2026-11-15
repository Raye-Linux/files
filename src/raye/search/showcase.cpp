/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The search box in the render harness (tests/raye/harness): resting with
// its placeholder, typed in and focused, the recent searches drop-down, and
// the Search options menu. Recent searches come from an in-memory file, so
// the harness never writes the user's dolphinrc.

#include "searchbox.h"

#include "ui/showcase.h"
#include "ui/tokens.h"

#include <KConfigGroup>
#include <KLocalizedString>

#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

namespace Raye::Search
{
namespace
{
using Ui::TokenSet;
using Ui::Tokens;

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
        p.fillRect(rect(), Tokens::current().color(m_role));
    }

private:
    TokenSet::Role m_role;
};

KSharedConfig::Ptr sampleConfig()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup(config, QStringLiteral("Raye"))
        .writeEntry("RecentSearches",
                    QStringList{demoText("invoice 2025", "facture 2025"), demoText("holiday photos", "photos vacances"), QStringLiteral("report.pdf"),
                                demoText("budget", "budget")});
    return config;
}

SearchBox *sampleBox(QWidget *parent)
{
    auto *box = new SearchBox(parent);
    box->setConfig(sampleConfig());
    box->setFolder(QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents")), demoText("Documents", "Documents"));
    box->setFixedWidth(240);
    return box;
}

// A strip the colour of the address row, the box at its right end.
QWidget *row(QWidget *parent, SearchBox *box)
{
    auto *strip = new Surface(TokenSet::bgLayerSolid, parent);
    strip->setFixedHeight(48);
    auto *l = new QHBoxLayout(strip);
    l->setContentsMargins(8, 0, 8, 0);
    l->addStretch(1);
    box->setParent(strip);
    l->addWidget(box);
    return strip;
}

QWidget *page(QWidget *parent)
{
    auto *base = new Surface(TokenSet::bgBase, parent);
    auto *column = new QVBoxLayout(base);
    column->setContentsMargins(24, 24, 24, 24);
    column->setSpacing(24);
    return base;
}
}

RAYE_SHOWCASE(searchBox, "SearchBox", "the search box: resting, typed in and focused, searching with a filter")
{
    QWidget *base = page(parent);
    auto *column = static_cast<QVBoxLayout *>(base->layout());
    column->addWidget(row(base, sampleBox(nullptr)));

    SearchBox *typed = sampleBox(nullptr);
    typed->lineEdit()->setText(demoText("invoice", "facture"));
    column->addWidget(row(base, typed));
    QTimer::singleShot(0, typed, [typed] {
        typed->lineEdit()->setFocus();
    });

    SearchBox *filtered = sampleBox(nullptr);
    Options o;
    o.term = QStringLiteral("report");
    o.kind = Kind::Document;
    filtered->setOptions(o);
    filtered->searchNow();
    column->addWidget(row(base, filtered));

    column->addStretch(1);
    return base;
}

RAYE_SHOWCASE(searchRecent, "SearchRecent", "the search box: recent searches under the empty box")
{
    QWidget *base = page(parent);
    auto *column = static_cast<QVBoxLayout *>(base->layout());
    SearchBox *box = sampleBox(nullptr);
    column->addWidget(row(base, box));
    column->addStretch(1);
    QTimer::singleShot(200, box, [box] {
        box->focusSearch();
    });
    return base;
}

RAYE_SHOWCASE(searchOptions, "SearchOptions", "the Search options menu, Kind open")
{
    QWidget *base = page(parent);
    auto *column = static_cast<QVBoxLayout *>(base->layout());
    SearchBox *box = sampleBox(nullptr);
    Options o;
    o.term = QStringLiteral("report");
    o.kind = Kind::Document;
    o.date = DateRange::ThisMonth;
    box->setOptions(o);
    column->addWidget(row(base, box));
    column->addStretch(1);
    QTimer::singleShot(200, box, [box] {
        QMenu *menu = box->optionsMenu();
        menu->popup(box->mapToGlobal(QPoint(-260, box->height() + 4)));
        QTimer::singleShot(150, menu, [menu] {
            for (QAction *a : menu->actions()) {
                if (a->objectName() == QLatin1String("raye_search_kind")) {
                    menu->setActiveAction(a);
                    if (a->menu()) {
                        a->menu()->popup(menu->mapToGlobal(menu->actionGeometry(a).topRight()));
                    }
                }
            }
        });
    });
    return base;
}

}
