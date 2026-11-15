/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <KFileItem>

#include <QList>
#include <QString>
#include <QUrl>

// What the Details pane lists (explorer spec 3.8): for one item its type,
// size, location and dates, plus what the type has (a picture's
// dimensions); for several the count, total size and the range of dates;
// with nothing selected, the folder itself.
namespace Raye::Preview
{

struct DetailRow {
    QString label;
    QString value;
    QUrl link; // the Location row opens its folder
    bool operator==(const DetailRow &other) const = default;
};

struct Details {
    QString title; // the name, or "3 items selected"
    QList<DetailRow> rows;
    bool multiple = false;
};

// home is passed in for the tests; empty means the account's home.
DOLPHIN_EXPORT Details detailsFor(const KFileItemList &selection, const KFileItem &folder, const QString &home = QString());

// The folder of url written the Raye way: "Documents › Work" for a folder
// in the home, "Raye › usr › share" elsewhere on the system drive, the
// address of anything else. Never "/home/<user>".
DOLPHIN_EXPORT QString locationText(const QUrl &folder, const QString &home = QString());

// "12 May 2026 – 3 Jun 2026", or one date when both are the same day.
DOLPHIN_EXPORT QString dateRange(const QDateTime &first, const QDateTime &last);

}
