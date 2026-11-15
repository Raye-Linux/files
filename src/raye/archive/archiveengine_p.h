/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archiveentry.h"

#include <raye/archive/archive.h>

#include <QFile>
#include <QString>

namespace Raye::Archive
{

inline std::filesystem::path toPath(const QString &path)
{
    return std::filesystem::path(QFile::encodeName(path).toStdString());
}

inline QString fromStd(const std::string &s)
{
    return QString::fromUtf8(s.data(), qsizetype(s.size()));
}

inline Error mapError(raye::archive::ErrorCode code)
{
    using C = raye::archive::ErrorCode;
    switch (code) {
    case C::Ok:
        return Error::None;
    case C::NotFound:
    case C::Io:
        return Error::CantOpen;
    case C::UnknownFormat:
        return Error::NotAnArchive;
    case C::Corrupt:
    case C::Truncated:
    case C::Checksum:
        return Error::Corrupt;
    case C::PasswordRequired:
        return Error::PasswordNeeded;
    case C::WrongPassword:
        return Error::WrongPassword;
    case C::UnsafePath:
    case C::LinkOutside:
        return Error::Unsafe;
    case C::Bomb:
        return Error::TooLarge;
    case C::Cancelled:
        return Error::Cancelled;
    case C::Exists:
    case C::NoSpace:
        return Error::WriteFailed;
    case C::MissingVolume:
        return Error::MissingPart;
    case C::NotImplemented:
    case C::UnsupportedMethod:
    case C::ReadOnlyFormat:
    case C::InvalidOption:
    case C::Internal:
        return Error::Unsupported;
    }
    return Error::Unsupported;
}

}
