/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QString>

// The share targets in Raye's words (explorer spec 3.11). Purpose finds what
// can take the files; this decides what each one is called, which ones sit
// in the "Share with" row and which only under "More options".
namespace Raye::Share
{

// Files' own targets, next to Purpose's.
inline constexpr char copyTarget[] = "raye-copy";
inline constexpr char copyLinkTarget[] = "raye-copy-link";

enum class Place {
    Row, // the "Share with" row
    More, // only under "More options"
    Hidden, // never shown (Files does it better itself)
};

struct Target {
    QString name;
    QString iconName;
    Place place = Place::More;
    int order = 100; // in the row, smaller first
    // A one-colour glyph, drawn in the text colour (Copy, Email...).
    bool glyph = false;
};

// For a Purpose plugin id ("emailplugin", "kdeconnectplugin"...). An unknown
// plugin keeps the name and icon Purpose gives it, under "More options".
DOLPHIN_EXPORT Target describe(const QString &pluginId, const QString &purposeName = QString(), const QString &purposeIcon = QString());

}
