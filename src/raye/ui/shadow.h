/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "uiexport.h"

#include <QImage>
#include <QSize>

class QWidget;
class KWindowShadow;

namespace Raye::Ui
{

// The Raye shadows of floating surfaces (theming spec 3.3, elevation):
// blur radius, y offset and opacity of the shadow colour, Night and Day.
enum class Elevation {
    Tooltip, // 12/4/30 %, 10/3/12 %
    Flyout, // 28/10/40 %, 20/8/16 %: menus, flyouts, popups
    Dialog, // 56/18/50 %, 40/16/22 %: same as an active window
};

// True when a compositor can draw translucent windows, blur and shadows.
// Without one (a bare X server) Raye surfaces are solid with square corners.
RAYE_UI_EXPORT bool compositorActive();

namespace Shadow
{
struct Params {
    int blur;
    int yOffset;
    qreal opacity;
};
RAYE_UI_EXPORT Params params(Elevation elevation, bool night);

// How far the shadow reaches outside the surface, on every side.
RAYE_UI_EXPORT int margin(Elevation elevation, bool night);

// The shadow of a rounded surface of this size, with the surface itself
// cut out (an acrylic surface must not show its own shadow through it).
// The image is the surface plus margin() on each side; the surface sits
// at (margin, margin).
RAYE_UI_EXPORT QImage image(const QSize &surface, qreal radius, Elevation elevation, bool night);

// Asks the compositor to draw that shadow around a frameless top-level
// widget (its window handle must exist). The returned object belongs to
// widget; pass it back to update it after a resize or a mode change.
RAYE_UI_EXPORT KWindowShadow *apply(QWidget *widget, qreal radius, Elevation elevation, KWindowShadow *existing = nullptr);
}

}
