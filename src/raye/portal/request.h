/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QString>
#include <QStringList>
#include <QList>
#include <QUrl>

class QByteArray;

namespace Raye::Portal
{

// What the portal asks for, read from its D-Bus options. No widget here, so
// all of it can be tested without a window.

// One line of the type list: "Pictures (*.png, *.jpg)".
struct DOLPHIN_EXPORT FileType {
    QString label;
    QStringList globs;
    QStringList mimeTypes;

    bool operator==(const FileType &other) const = default;
    // Every file is listed: no glob or type at all, or a plain "*".
    bool showsEverything() const;
    QString displayName() const;
};

struct ChoiceOption {
    QString id;
    QString label;
};

// An extra control an app adds: a list when it has options, else a check box
// whose value is "true" or "false".
struct Choice {
    QString id;
    QString label;
    QList<ChoiceOption> options;
    QString initial;
};

enum class Mode {
    OpenFile,
    OpenFolder,
    SaveFile,
    SaveFiles,
};

struct FileRequest {
    Mode mode = Mode::OpenFile;
    QString title;
    QString acceptLabel;
    bool multiple = false;
    QUrl folder;
    QString name; // SaveFile: what the name field starts with
    QStringList names; // SaveFiles: the files that will be written
    QList<FileType> types;
    int currentType = 0;
    QList<Choice> choices;
};

// Paths come as bytes and may end in a NUL.
DOLPHIN_EXPORT QString decodePath(const QByteArray &bytes);
// "_Open" to "Open": toolkits send their mnemonic with the label.
DOLPHIN_EXPORT QString cleanLabel(const QString &label);
// *.[pP][nN][gG] to *.png.
DOLPHIN_EXPORT QString simplifyGlob(const QString &glob);

// What the name field shows for these names: one name as it is, several in
// quotes, with \" and \\ for a quote or a backslash inside one. A single name
// that would read back as something else is quoted too. namesFromField() reads
// it back and takes what a person types.
DOLPHIN_EXPORT QString fieldFromNames(const QStringList &names);
DOLPHIN_EXPORT QStringList namesFromField(const QString &text);

// What a name typed in the field points to: a full path, ~/ or relative to the
// folder on screen.
DOLPHIN_EXPORT QUrl resolveName(const QString &name, const QUrl &folder);

// The name to save under: with no extension and a type that has exactly one
// literal one, the extension is added, as Windows does.
DOLPHIN_EXPORT QString withTypeExtension(const QString &name, const FileType &type);

}
