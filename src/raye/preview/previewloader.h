/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QString>
#include <QStringList>

// Which preview the Preview pane shows for a file (explorer spec 3.8):
// documents in a read-only part of the app that opens them (Okular for
// PDF and e-books, Kate's editor for text and code), pictures, fonts,
// audio and video drawn by Files itself, office files as the thumbnail of
// their first page, and "No preview available." for the rest.
namespace Raye::Preview
{

enum class Kind {
    None,
    Image,
    Text, // Kate's part when installed, else Files' own plain view
    Document, // Okular's part
    Audio,
    Video,
    Font,
    Thumbnail, // the first page, from the thumbnailers
};

struct Loader {
    Kind kind = Kind::None;
    // The part to load, by plugin id, for Text and Document; empty for the
    // kinds Files draws itself.
    QString partId;
    // Parts are created once per family and reused while the pane lives.
    QString family;
};

// What to use for a MIME type. installedParts lists the part plugin ids
// present on the system ("okularpart", "katepart", ...); a kind whose
// part is missing falls back to what Files can draw, or to None.
DOLPHIN_EXPORT Loader loaderFor(const QString &mimeType, const QStringList &installedParts);

// The same with the part plugins actually installed.
DOLPHIN_EXPORT Loader loaderFor(const QString &mimeType);

// Part plugin ids found under kf6/parts.
DOLPHIN_EXPORT QStringList installedParts();

// Files bigger than this are not shown in the plain text view (the part
// reads them lazily and has no limit).
inline constexpr qint64 maxPlainTextBytes = 512 * 1024;

}
