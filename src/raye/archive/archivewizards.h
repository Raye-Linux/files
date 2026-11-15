/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archiveexport.h"
#include "archiveformat.h"

#include <QString>
#include <QStringList>

#include <functional>

class QWidget;

// The archive actions as the menus and the command bar call them (explorer
// spec 3.12): each one shows its Raye dialog, runs its job (which the Raye
// progress window and the taskbar pick up through KIO's job tracker) and
// answers the questions that come up: password, an archive that expands
// too much, an error. Nothing here blocks the window but the password
// prompt.
namespace Raye::Archive::Wizards
{

// "Extract all…": the destination dialog, then the extraction; opens the
// folder after when "Show extracted files when complete" stays ticked.
RAYE_ARCHIVE_EXPORT void extractAll(QWidget *parent, const QString &archivePath);

// "Extract here": into the archive's own folder. A name already there is
// asked about in the Raye "Replace or skip files" window.
RAYE_ARCHIVE_EXPORT void extractHere(QWidget *parent, const QString &archivePath);

// "Extract to "Holiday\"": into a folder named after the archive, next
// to it. An archive that holds only that folder is not nested twice.
RAYE_ARCHIVE_EXPORT void extractToFolder(QWidget *parent, const QString &archivePath);

// Compress to › Additional options…
RAYE_ARCHIVE_EXPORT void compressWithOptions(QWidget *parent, const QStringList &sources);

// Compress to › ZIP / 7z / TAR: "<name>.zip" next to the items at once, no
// dialog. created gets the new archive's path once it is complete: the
// view starts renaming it there, like Windows does.
RAYE_ARCHIVE_EXPORT void compressNow(QWidget *parent, const QStringList &sources, Format format, const std::function<void(const QString &)> &created = {});

// The password prompt. Empty when cancelled.
RAYE_ARCHIVE_EXPORT QString askPassword(QWidget *parent, const QString &archiveName, bool wrongPassword);

// "This archive expands to much more than its size…": true to go on.
RAYE_ARCHIVE_EXPORT bool confirmExpand(QWidget *parent, const QString &archiveName);

}
