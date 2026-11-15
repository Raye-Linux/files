/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "menuprefix.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>

namespace Raye::OpenWith
{

QByteArray missingMenuPrefix(const QByteArray &current, const QStringList &configDirs)
{
    if (!current.isEmpty()) {
        return {};
    }
    auto found = [&configDirs](const QString &name) {
        for (const QString &dir : configDirs) {
            if (QFileInfo::exists(dir + QLatin1String("/menus/") + name)) {
                return true;
            }
        }
        return false;
    };
    if (found(QStringLiteral("applications.menu"))) {
        return {};
    }
    for (const char *prefix : {"raye-", "plasma-"}) {
        if (found(QLatin1String(prefix) + QLatin1String("applications.menu"))) {
            return prefix;
        }
    }
    return {};
}

}

// Files started from a remote shell or a service has no menu prefix: set it
// before anything reads the app database, or "Open with" lists no app.
static void rayeMenuPrefix()
{
    const QByteArray prefix = Raye::OpenWith::missingMenuPrefix(qgetenv("XDG_MENU_PREFIX"),
                                                                QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation));
    if (!prefix.isEmpty()) {
        qputenv("XDG_MENU_PREFIX", prefix);
    }
}
Q_COREAPP_STARTUP_FUNCTION(rayeMenuPrefix)
