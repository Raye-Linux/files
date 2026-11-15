/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archiveentry.h"

#include <KLocalizedString>

namespace Raye::Archive
{

QString errorMessage(Error error)
{
    switch (error) {
    case Error::None:
        return QString();
    case Error::CantOpen:
        return i18nc("@info", "Files can't open this archive. It may have been moved, or you don't have permission to open it.");
    case Error::NotAnArchive:
        return i18nc("@info", "Files can't open this file. It isn't a compressed folder, or it uses a format Files can't read.");
    case Error::Corrupt:
        return i18nc("@info", "This compressed folder is damaged or incomplete.");
    case Error::PasswordNeeded:
        return i18nc("@info", "This archive is protected. Enter the password.");
    case Error::WrongPassword:
        return i18nc("@info", "The password is incorrect. Try again.");
    case Error::Unsafe:
        return i18nc("@info", "This archive can't be extracted safely. It tries to put files outside the destination folder.");
    case Error::TooLarge:
        return i18nc("@info", "This archive expands to much more than its size. Extracting it could fill your drive.");
    case Error::Cancelled:
        return i18nc("@info", "The operation was canceled.");
    case Error::WriteFailed:
        return i18nc("@info", "Files can't write here. The drive may be full, or you don't have permission.");
    case Error::NotFound:
        return i18nc("@info", "This item isn't in the archive any more.");
    case Error::Unsupported:
        return i18nc("@info", "Files can't do this with this kind of archive.");
    case Error::MissingPart:
        return i18nc("@info", "A part of this archive is missing. Put all its parts in the same folder and try again.");
    }
    return QString();
}

}
