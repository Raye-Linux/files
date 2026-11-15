/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The share flyout in the render harness (tests/raye/harness): open under
// the Share button of a command bar, for a picture of a made-up account.
// The targets are the ones Purpose really offers on the machine.

#include "shareflyout.h"
#include "sharemodel.h"

#include "commandbar/commandbar.h"
#include "commandbar/commandids.h"
#include "ui/showcase.h"
#include "ui/tokens.h"

#include <QAbstractButton>
#include <QAction>
#include <QGuiApplication>
#include <QPainter>
#include <QQuickItem>
#include <QTimer>
#include <QVBoxLayout>

namespace Raye::Share
{
namespace
{
class Surface : public QWidget
{
public:
    Surface(Ui::TokenSet::Role role, QWidget *parent)
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
        p.setBrush(Ui::Tokens::current().color(m_role));
        p.drawRoundedRect(rect(), Ui::Metrics::radiusOverlay, Ui::Metrics::radiusOverlay);
    }

private:
    Ui::TokenSet::Role m_role;
};
}

namespace
{
QWidget *sharePage(QWidget *parent, const QString &page)
{
    // The QML page comes from the build tree when the harness runs there.
    const QString build = qEnvironmentVariable("RAYE_FILES_BINARY_DIR");
    if (!build.isEmpty()) {
        ShareFlyout::addImportPath(build + QStringLiteral("/bin"));
    }

    auto *base = new Surface(Ui::TokenSet::bgBase, parent);
    auto *column = new QVBoxLayout(base);
    column->setContentsMargins(16, 16, 16, 16);
    auto *frame = new Surface(Ui::TokenSet::bgLayerSolid, base);
    auto *inner = new QVBoxLayout(frame);
    inner->setContentsMargins(0, 0, 0, 0);

    auto *bar = new Commands::CommandBar(frame);
    QHash<QString, QAction *> actions;
    for (const char *id : {Commands::Ids::newMenu, Commands::Ids::cut, Commands::Ids::copy, Commands::Ids::paste, Commands::Ids::rename,
                           Commands::Ids::moveToTrash, Commands::Ids::sort, Commands::Ids::viewMode}) {
        auto *a = new QAction(QString::fromLatin1(id), bar);
        a->setObjectName(QString::fromLatin1(id));
        actions.insert(a->objectName(), a);
    }
    bar->setActionLookup([actions](const QString &name) {
        return actions.value(name);
    });
    Commands::CommandContext context;
    context.url = QUrl::fromLocalFile(QStringLiteral("/home/ann/Pictures"));
    context.selection = {QUrl::fromLocalFile(QStringLiteral("/home/ann/Pictures/Lisbon at night.jpg"))};
    context.selectionMimeTypes = {QStringLiteral("image/jpeg")};
    bar->setContext(context);
    bar->setShareHandler([page](QWidget *anchor, const QList<QUrl> &urls) {
        auto *flyout = new ShareFlyout(anchor);
        if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
            flyout->setTranslucent(true); // what a Raye session shows
        }
        flyout->setUrls(urls, {QStringLiteral("image/jpeg")});
        if (flyout->isReady()) {
            flyout->view()->rootObject()->setProperty("page", page);
        }
        QObject::connect(flyout, &Ui::Flyout::closed, flyout, &QObject::deleteLater);
        flyout->showAt(anchor, Qt::BottomEdge, Qt::AlignLeft);
    });
    inner->addWidget(bar);
    inner->addStretch(1);
    column->addWidget(frame);

    QTimer::singleShot(0, bar, [bar]() {
        if (auto *share = qobject_cast<QAbstractButton *>(bar->button(QStringLiteral("share")))) {
            share->click();
        }
    });
    return base;
}
}

RAYE_SHOWCASE(shareSheet, "ShareSheet", "the share flyout open under the Share button")
{
    return sharePage(parent, QStringLiteral("main"));
}

RAYE_SHOWCASE(shareSheetMore, "ShareSheetMore", "the share flyout showing every target")
{
    return sharePage(parent, QStringLiteral("more"));
}

}
