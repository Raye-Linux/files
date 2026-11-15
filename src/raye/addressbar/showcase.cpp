/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The address row in the render harness (tests/raye/harness): sample
// locations of a made-up account, nothing read from the machine.

#include "addressbar.h"
#include "themeicon_p.h"
#include "addressbox.h"

#include "locations/locationnames.h"
#include "ui/flyout.h"
#include "ui/showcase.h"
#include "ui/tokens.h"

#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

namespace Raye::Address
{
namespace
{
using Ui::TokenSet;
using Ui::Tokens;

Locations::Context sampleContext()
{
    Locations::Context c;
    c.homePath = QStringLiteral("/home/ann");
    c.userName = QStringLiteral("Ann");
    const QList<std::pair<Locations::KnownFolder, const char *>> folders = {
        {Locations::KnownFolder::Desktop, "/home/ann/Desktop"},
        {Locations::KnownFolder::Documents, "/home/ann/Documents"},
        {Locations::KnownFolder::Downloads, "/home/ann/Downloads"},
        {Locations::KnownFolder::Music, "/home/ann/Music"},
        {Locations::KnownFolder::Pictures, "/home/ann/Pictures"},
        {Locations::KnownFolder::Videos, "/home/ann/Videos"},
    };
    for (const auto &[folder, path] : folders) {
        c.knownFolders.insert(folder, QString::fromLatin1(path));
    }
    c.drives = {{QStringLiteral("/"), Locations::LocationNames::systemDriveName(), QStringLiteral("drive-harddisk-system"), false},
                {QStringLiteral("/media/ann/STICK"), QStringLiteral("USB"), QStringLiteral("drive-removable-media-usb"), true}};
    return c;
}

// A surface painted with one token, like the window body behind the row.
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

// Stands where the search box goes, without its words.
class SearchSpot : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const TokenSet &t = Tokens::current();
        const QRectF box = QRectF(0, (height() - 32) / 2.0, width(), 32).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(QPen(t.color(TokenSet::strokeControl), 1));
        p.setBrush(t.color(TokenSet::fillControl));
        p.drawRoundedRect(box, 4, 4);
        themeIcon(QStringLiteral("edit-find")).paint(&p, QRect(int(box.right()) - 26, int(box.center().y()) - 8, 16, 16));
    }
};

AddressBar *sampleBar(QWidget *parent, const QUrl &url, bool search = true)
{
    auto *bar = new AddressBar(parent);
    bar->setLocationNames(Locations::LocationNames(sampleContext()));
    bar->setUrl(Locations::homePageUrl());
    bar->setUrl(url);
    if (search) {
        bar->setTrailingWidget(new SearchSpot);
    }
    return bar;
}

QWidget *framed(QWidget *parent, QWidget *content, int width = -1)
{
    auto *frame = new Surface(TokenSet::bgLayerSolid, parent);
    auto *l = new QVBoxLayout(frame);
    l->setContentsMargins(0, 0, 0, 0);
    content->setParent(frame);
    l->addWidget(content);
    if (width > 0) {
        frame->setFixedWidth(width);
    }
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
}

RAYE_SHOWCASE(addressRow, "AddressRow", "the address row: crumbs, edit mode, loading, a long path, the network")
{
    QWidget *base = page(parent);
    auto *column = static_cast<QVBoxLayout *>(base->layout());

    column->addWidget(framed(base, sampleBar(nullptr, QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents/Work")))));

    AddressBar *editing = sampleBar(nullptr, QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents/Work")));
    column->addWidget(framed(base, editing));
    QTimer::singleShot(0, editing, [editing]() {
        editing->editLocation();
    });

    AddressBar *loading = sampleBar(nullptr, QUrl(QStringLiteral("smb://nas/Music/2024")));
    loading->setLoadingProgress(60);
    column->addWidget(framed(base, loading));

    column->addWidget(framed(base, sampleBar(nullptr, Locations::thisPcUrl())));
    column->addWidget(framed(base, sampleBar(nullptr, QUrl(QStringLiteral("trash:/")))));

    AddressBar *folded = sampleBar(nullptr, QUrl::fromLocalFile(QStringLiteral("/home/ann/Pictures/2024/Summer in Lisbon/Day three/Old town")), false);
    column->addWidget(framed(base, folded, 560), 0, Qt::AlignLeft);
    column->addStretch(1);
    return base;
}

RAYE_SHOWCASE(addressRowFolders, "AddressRowFolders", "the address row with the folders of This PC dropped down")
{
    QWidget *base = page(parent);
    auto *column = static_cast<QVBoxLayout *>(base->layout());
    AddressBar *bar = sampleBar(nullptr, QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents/Work")));
    column->addWidget(framed(base, bar));
    column->addStretch(1);
    QTimer::singleShot(0, bar, [bar]() {
        bar->addressBox()->showSubfolders(0);
        // Shots are taken offscreen, where no compositor says it can blur:
        // draw what a Raye session shows.
        if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
            const QList<Ui::Flyout *> flyouts = bar->addressBox()->findChildren<Ui::Flyout *>();
            for (Ui::Flyout *f : flyouts) {
                f->setTranslucent(true);
            }
        }
    });
    return base;
}

}
