/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "workertext.h"

#include <KLocalizedString>

namespace Raye::Archive::WorkerText
{

QString passwordCaption()
{
    return i18nc("@title:window", "Protected archive");
}

QString passwordCommentLabel()
{
    return i18nc("@label the archive a password is asked for", "Archive:");
}

QString cannotChangeProtected()
{
    return i18nc("@info", "Files can't change a protected archive.");
}

}
