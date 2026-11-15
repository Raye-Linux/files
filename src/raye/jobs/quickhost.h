/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QVariantMap>

class QQuickWidget;
class QWidget;

namespace Raye::Jobs
{

// A QQuickWidget showing a page of org.raye.files, its strings from the
// raye-files catalog, its clear colour the given token. Properties are set
// after loading; a QObject among them goes with the view. Load errors are
// printed and leave an empty view.
DOLPHIN_EXPORT QQuickWidget *quickPage(const QString &type, const QVariantMap &properties = {}, QWidget *parent = nullptr, const char *clearToken = "bgBase");

// The view follows the page: as wide as given, as tall as the page's
// implicit height, also when it changes; then the widget given (a dialog
// or a window holding the view) takes its new size.
DOLPHIN_EXPORT void fitToPage(QQuickWidget *view, int width, QWidget *resize = nullptr);

}
