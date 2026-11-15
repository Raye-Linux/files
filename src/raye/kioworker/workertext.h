/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archive/archiveexport.h"

#include <QString>

// The words of the rayearchive worker. In a .cpp of their own so the
// catalog script, which reads src/raye/**/*.cpp, finds them (the worker's
// entry file is .cc).
namespace Raye::Archive::WorkerText
{
RAYE_ARCHIVE_EXPORT QString passwordCaption();
RAYE_ARCHIVE_EXPORT QString passwordCommentLabel();
RAYE_ARCHIVE_EXPORT QString cannotChangeProtected();
}
