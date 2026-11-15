/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KIconEngine>
#include <KIconLoader>

#include <QIcon>

namespace Raye::Address
{

// Through KIconEngine, as Plasma's platform theme gives them: symbolic icons
// take the text colour of the scheme, in Night and Day alike. A name the
// theme does not have falls back to the second one.
inline QIcon themeIcon(const QString &name, const QString &fallback = QString())
{
    if (name.isEmpty() || (!fallback.isEmpty() && !QIcon::hasThemeIcon(name))) {
        return fallback.isEmpty() ? QIcon() : QIcon(new KIconEngine(fallback, KIconLoader::global()));
    }
    return QIcon(new KIconEngine(name, KIconLoader::global()));
}

}
