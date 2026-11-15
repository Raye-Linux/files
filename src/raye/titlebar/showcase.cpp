/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The title row in the render harness (tests/raye/harness): a window top
// with four tabs (a copy running into one, a drop over another, the pointer
// on a third), a crowded strip that scrolls, the row under a normal title
// bar (OwnTitleBar=false), and the tab menu.

#include "tabbar.h"
#include "tabwidth.h"
#include "titlebar.h"

#include "ui/showcase.h"
#include "ui/tokens.h"

#include <KIconEngine>
#include <KIconLoader>
#include <KLocalizedString>

#include <QAbstractButton>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

namespace Raye::Title
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

QIcon icon(const char *name)
{
    return QIcon(new KIconEngine(QString::fromLatin1(name), KIconLoader::global()));
}

class Surface : public QWidget
{
public:
    Surface(TokenSet::Role role, QWidget *parent, int radius = 0)
        : QWidget(parent)
        , m_role(role)
        , m_radius(radius)
    {
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Tokens::current().color(m_role));
        p.drawRoundedRect(rect(), m_radius, m_radius);
    }

private:
    TokenSet::Role m_role;
    int m_radius;
};

// A window top: the title row, then a strip the colour of the address row
// so the selected tab is seen flowing into it.
struct Top {
    QWidget *frame;
    TitleBar *row;
    TabBar *tabs;
};

Top windowTop(QWidget *parent, const QList<std::pair<const char *, QString>> &tabs, int current)
{
    auto *frame = new Surface(TokenSet::bgBase, parent, Ui::Metrics::radiusOverlay);
    auto *column = new QVBoxLayout(frame);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    auto *row = new TitleBar(frame);
    auto *bar = new TabBar;
    for (const auto &[iconName, text] : tabs) {
        bar->addTab(icon(iconName), text);
    }
    bar->setCurrentIndex(current);
    row->setTabBar(bar);
    column->addWidget(row);
    auto *address = new Surface(TokenSet::bgLayerSolid, frame);
    address->setFixedHeight(56);
    column->addWidget(address);
    return {frame, row, bar};
}

QWidget *page(QWidget *parent)
{
    auto *base = new Surface(TokenSet::bgBase, parent);
    auto *column = new QVBoxLayout(base);
    column->setContentsMargins(24, 24, 24, 24);
    column->setSpacing(24);
    return base;
}

QList<std::pair<const char *, QString>> sampleTabs()
{
    return {
        {"user-home", demoText("Home", "Accueil")},
        {"folder-documents", demoText("Documents", "Documents")},
        {"folder-download", demoText("Downloads", "Téléchargements")},
        {"folder-pictures", demoText("Vacation photos 2025 Lisbon", "Photos de vacances 2025 Lisbonne")},
    };
}

void hoverTab(TabBar *bar, int index)
{
    QTimer::singleShot(0, bar, [bar, index] {
        const QPointF pos = bar->tabRect(index).center();
        QMouseEvent move(QEvent::MouseMove, pos, bar->mapToGlobal(pos), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(bar, &move);
    });
}
}

RAYE_SHOWCASE(titleBar, "TitleBar", "the title row: tabs, a copy ring, a drop target, the pointer on a tab, the close button hovered")
{
    QWidget *base = page(parent);
    auto *column = static_cast<QVBoxLayout *>(base->layout());

    Top top = windowTop(base, sampleTabs(), 1);
    top.tabs->setTabProgress(2, 0.62);
    top.tabs->setDropTargetIndex(3);
    column->addWidget(top.frame);
    hoverTab(top.tabs, 0);

    Top second = windowTop(base, sampleTabs(), 0);
    column->addWidget(second.frame);
    if (QAbstractButton *close = second.row->button(QStringLiteral("close"))) {
        close->setAttribute(Qt::WA_UnderMouse, true);
    }
    if (QAbstractButton *add = second.row->button(QStringLiteral("newtab"))) {
        add->setAttribute(Qt::WA_UnderMouse, true);
    }

    QList<std::pair<const char *, QString>> many;
    for (int i = 1; i <= 14; ++i) {
        many.append({"folder", demoText("Project ", "Projet ") + QString::number(i)});
    }
    Top crowded = windowTop(base, many, 5);
    column->addWidget(crowded.frame);

    Top fallback = windowTop(base, sampleTabs(), 2);
    fallback.row->setOwnTitleBar(false);
    column->addWidget(fallback.frame);

    column->addStretch(1);
    return base;
}

RAYE_SHOWCASE(titleBarTabMenu, "TitleBarTabMenu", "the tab menu: Close tab, Close other tabs, Close tabs to the right, Duplicate tab, Move tab to new window")
{
    QWidget *base = page(parent);
    auto *column = static_cast<QVBoxLayout *>(base->layout());
    Top top = windowTop(base, sampleTabs(), 1);
    column->addWidget(top.frame);
    column->addStretch(1);
    TabBar *bar = top.tabs;
    QTimer::singleShot(0, bar, [bar] {
        QMenu *menu = bar->createTabMenu(1, bar);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(bar->mapToGlobal(bar->tabRect(1).bottomLeft() + QPoint(24, 0)));
    });
    return base;
}

}
