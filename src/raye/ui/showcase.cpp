/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "showcase.h"

#include <QMap>

namespace Raye::Ui::Showcase
{

namespace
{
// Built on first use: builders register from static initialisers, in
// whatever order the libraries load.
QMap<QString, Entry> &registry()
{
    static QMap<QString, Entry> map;
    return map;
}
}

bool add(const QString &name, const QString &description, const Builder &build)
{
    if (name.isEmpty() || !build) {
        return false;
    }
    registry().insert(name.toLower(), Entry{name, description, build});
    return true;
}

QList<Entry> entries()
{
    return registry().values();
}

const Entry *find(const QString &name)
{
    const auto &map = registry();
    const auto it = map.constFind(name.toLower());
    return it == map.constEnd() ? nullptr : &it.value();
}

}
