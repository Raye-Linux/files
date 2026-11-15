/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "shadow.h"
#include "tokens.h"

#include <KWindowShadow>
#include <KWindowSystem>
#include <KX11Extras>

#include <QPainter>
#include <QWidget>
#include <QWindow>

#include <cmath>
#include <vector>

namespace Raye::Ui
{

namespace
{
// One pass of a box blur over the alpha of a premultiplied image, along
// rows (horizontal) or columns.
void boxBlur(QImage &image, int radius, bool horizontal)
{
    if (radius <= 0) {
        return;
    }
    const int lines = horizontal ? image.height() : image.width();
    const int length = horizontal ? image.width() : image.height();
    std::vector<int> line(length);
    const int span = 2 * radius + 1;
    for (int l = 0; l < lines; ++l) {
        auto pixel = [&](int i) -> QRgb & {
            const int x = horizontal ? i : l;
            const int y = horizontal ? l : i;
            return reinterpret_cast<QRgb *>(image.scanLine(y))[x];
        };
        for (int i = 0; i < length; ++i) {
            line[i] = qAlpha(pixel(i));
        }
        int sum = 0;
        for (int i = -radius; i <= radius; ++i) {
            sum += line[qBound(0, i, length - 1)];
        }
        for (int i = 0; i < length; ++i) {
            pixel(i) = qRgba(0, 0, 0, sum / span);
            sum += line[qMin(length - 1, i + radius + 1)] - line[qMax(0, i - radius)];
        }
    }
}
}

bool compositorActive()
{
    if (KWindowSystem::isPlatformWayland()) {
        return true;
    }
    if (KWindowSystem::isPlatformX11()) {
        return KX11Extras::compositingActive();
    }
    return false;
}

namespace Shadow
{

Params params(Elevation elevation, bool night)
{
    switch (elevation) {
    case Elevation::Tooltip:
        return night ? Params{12, 4, 0.30} : Params{10, 3, 0.12};
    case Elevation::Flyout:
        return night ? Params{28, 10, 0.40} : Params{20, 8, 0.16};
    case Elevation::Dialog:
        return night ? Params{56, 18, 0.50} : Params{40, 16, 0.22};
    }
    return {0, 0, 0};
}

int margin(Elevation elevation, bool night)
{
    const Params p = params(elevation, night);
    return p.blur + p.yOffset;
}

QImage image(const QSize &surface, qreal radius, Elevation elevation, bool night)
{
    const Params p = params(elevation, night);
    const int m = p.blur + p.yOffset;
    const QColor colour = Tokens::current().color(TokenSet::shadow);

    QImage mask(surface + QSize(2 * m, 2 * m), QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    {
        QPainter painter(&mask);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        painter.drawRoundedRect(QRectF(m, m + p.yOffset, surface.width(), surface.height()), radius, radius);
    }
    // Three box passes come close to a gaussian of the CSS blur radius.
    const int box = qMax(1, qRound(p.blur / 2.0 / std::sqrt(3.0)));
    for (int pass = 0; pass < 3; ++pass) {
        boxBlur(mask, box, true);
        boxBlur(mask, box, false);
    }

    QImage out(mask.size(), QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter painter(&out);
    painter.fillRect(out.rect(), withAlpha(colour, p.opacity));
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.drawImage(0, 0, mask);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    painter.drawRoundedRect(QRectF(m, m, surface.width(), surface.height()), radius, radius);
    return out;
}

KWindowShadow *apply(QWidget *widget, qreal radius, Elevation elevation, KWindowShadow *existing)
{
    QWindow *window = widget ? widget->windowHandle() : nullptr;
    if (!window) {
        return existing;
    }
    const bool night = Tokens::current().night();
    const int m = margin(elevation, night);
    // A small surface is enough: corners are radius + margin, edges 1 px.
    const int side = qCeil(4 * radius) + 1;
    const QImage source = image(QSize(side, side), radius, elevation, night);
    KWindowShadow *shadow = existing ? existing : new KWindowShadow(widget);
    if (shadow->isCreated()) {
        shadow->destroy();
    }
    const int corner = m + qCeil(2 * radius);
    const int w = source.width();
    const int h = source.height();
    auto tile = [&source](int x, int y, int tw, int th) {
        auto t = KWindowShadowTile::Ptr::create();
        t->setImage(source.copy(x, y, tw, th));
        return t;
    };
    shadow->setTopLeftTile(tile(0, 0, corner, corner));
    shadow->setTopTile(tile(corner, 0, 1, corner));
    shadow->setTopRightTile(tile(w - corner, 0, corner, corner));
    shadow->setRightTile(tile(w - corner, corner, corner, 1));
    shadow->setBottomRightTile(tile(w - corner, h - corner, corner, corner));
    shadow->setBottomTile(tile(corner, h - corner, 1, corner));
    shadow->setBottomLeftTile(tile(0, h - corner, corner, corner));
    shadow->setLeftTile(tile(0, corner, corner, 1));
    shadow->setPadding(QMargins(m, m, m, m));
    shadow->setWindow(window);
    shadow->create();
    return shadow;
}

}

}
