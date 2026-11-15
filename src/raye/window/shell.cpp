/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "shell.h"

#include "addressbar/addressbar.h"
#include "commandbar/commandbar.h"
#include "navpane/navigationpane.h"
#include "search/searchbox.h"
#include "titlebar/titlebar.h"
#include "ui/tokens.h"
#include "window/statusbar.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QSplitter>
#include <QVBoxLayout>

using Raye::Ui::TokenSet;
using Raye::Ui::Tokens;

namespace Raye::Window
{
namespace
{
// The address row and the command bar sit on the layer colour, like the
// selected tab they flow out of.
class Chrome : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        const auto &t = Tokens::current();
        QPainter p(this);
        p.fillRect(event->rect(), t.color(TokenSet::bgLayerSolid));
        p.setPen(t.color(TokenSet::strokeDividerSolid));
        p.drawLine(0, height() - 1, width(), height() - 1);
    }
};

KConfigGroup rayeGroup()
{
    return KSharedConfig::openConfig(QStringLiteral("dolphinrc"))->group(QStringLiteral("Raye"));
}
}

Shell::Shell(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("RayeShell"));

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    m_title = new Title::TitleBar(this);
    column->addWidget(m_title);

    m_chrome = new Chrome(this);
    auto *chromeColumn = new QVBoxLayout(m_chrome);
    chromeColumn->setContentsMargins(0, 0, 0, 1);
    chromeColumn->setSpacing(0);
    m_address = new Address::AddressBar(m_chrome);
    m_search = new Search::SearchBox(m_address);
    m_address->setTrailingWidget(m_search);
    chromeColumn->addWidget(m_address);
    m_commands = new Commands::CommandBar(m_chrome);
    chromeColumn->addWidget(m_commands);
    column->addWidget(m_chrome);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);
    m_nav = new NavPane::NavigationPane(m_splitter);
    m_nav->setMinimumWidth(navMinimum);
    m_nav->setMaximumWidth(navMaximum);
    m_splitter->addWidget(m_nav);
    m_splitter->setStretchFactor(0, 0);
    column->addWidget(m_splitter, 1);

    m_status = new StatusBar(this);
    column->addWidget(m_status);

    connect(m_splitter, &QSplitter::splitterMoved, this, [this] {
        const QList<int> sizes = m_splitter->sizes();
        if (m_nav->isVisible() && !sizes.isEmpty()) {
            m_navWidth = sizes.first();
        }
        if (m_right && m_right->isVisible()) {
            m_rightWidth = sizes.last();
        }
        saveSizes();
    });

    readSizes();
}

Shell::~Shell() = default;

void Shell::setContent(QWidget *content)
{
    if (m_content == content) {
        return;
    }
    m_content = content;
    m_splitter->insertWidget(1, content);
    m_splitter->setStretchFactor(1, 1);
    applySizes();
}

void Shell::setRightPane(QWidget *pane)
{
    if (m_right == pane) {
        return;
    }
    m_right = pane;
    if (pane) {
        pane->setMinimumWidth(rightMinimum);
        pane->setMaximumWidth(rightMaximum);
        m_splitter->addWidget(pane);
        m_splitter->setStretchFactor(m_splitter->indexOf(pane), 0);
    }
    applySizes();
}

void Shell::setNavigationPaneVisible(bool visible)
{
    m_nav->setVisible(visible);
    applySizes();
}

bool Shell::isNavigationPaneVisible() const
{
    return !m_nav->isHidden();
}

void Shell::setRightPaneVisible(bool visible)
{
    if (m_right) {
        m_right->setVisible(visible);
        applySizes();
    }
}

bool Shell::isRightPaneVisible() const
{
    return m_right && !m_right->isHidden();
}

void Shell::setStatusBarVisible(bool visible)
{
    m_status->setVisible(visible);
}

void Shell::readSizes()
{
    const KConfigGroup g = rayeGroup();
    m_navWidth = qBound(navMinimum, g.readEntry("NavPaneWidth", 240), navMaximum);
    m_rightWidth = qBound(rightMinimum, g.readEntry("RightPaneWidth", int(rightDefault)), rightMaximum);
    applySizes();
}

void Shell::saveSizes() const
{
    KConfigGroup g = rayeGroup();
    g.writeEntry("NavPaneWidth", m_navWidth);
    g.writeEntry("RightPaneWidth", m_rightWidth);
}

void Shell::applySizes()
{
    if (!m_content) {
        return;
    }
    QList<int> sizes;
    const int total = qMax(width(), 800);
    int rest = total;
    for (int i = 0; i < m_splitter->count(); ++i) {
        QWidget *w = m_splitter->widget(i);
        if (w == m_nav) {
            sizes << (w->isHidden() ? 0 : m_navWidth);
        } else if (w == m_right) {
            sizes << (w->isHidden() ? 0 : m_rightWidth);
        } else {
            sizes << 0;
        }
        rest -= sizes.last();
    }
    const int contentIndex = m_splitter->indexOf(m_content);
    if (contentIndex >= 0) {
        sizes[contentIndex] = qMax(200, rest);
    }
    m_splitter->setSizes(sizes);
}

void Shell::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.fillRect(event->rect(), Tokens::current().color(TokenSet::bgBase));
}

void Shell::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        update();
    }
    QWidget::changeEvent(event);
}
}
