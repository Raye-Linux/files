/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

// The helper is built on its own; Files and its tests get the same code
// from the private library.
#ifdef RAYE_NETDRIVE_HELPER
#define RAYE_HELPER_EXPORT
#else
#include "dolphin_export.h"
#define RAYE_HELPER_EXPORT DOLPHIN_EXPORT
#endif

#include <string>
#include <string_view>
#include <sys/types.h>

// What raye-files-netdrive-helper accepts. It runs as root for whoever is
// signed in, so everything it is told is checked here first. Plain C++: the
// helper links nothing else.
namespace Raye::NetDrives::Helper
{

// A host name, an IPv4 address or a bare IPv6 address.
RAYE_HELPER_EXPORT bool validServer(std::string_view server);
// A share name: no slash, backslash, comma or control character.
RAYE_HELPER_EXPORT bool validShare(std::string_view share);
// A user name or domain for mount options: no comma, no '=', no control
// character (they would be read as more options). May be empty.
RAYE_HELPER_EXPORT bool validCredential(std::string_view text);

// The -o value given to mount.cifs. An empty user mounts as a guest. Files
// belong to the caller and nobody else can read them; no device or setuid
// file works from the share.
RAYE_HELPER_EXPORT std::string cifsOptions(uid_t uid, gid_t gid, std::string_view user, std::string_view domain);

// The folders a drive may be mounted on: inside the caller's home or in
// /media/<login>. path must already be made canonical (realpath) and
// absolute; the folder itself, never home or /media/<login>.
RAYE_HELPER_EXPORT bool allowedMountPoint(std::string_view path, std::string_view home, std::string_view login);

// "//server/share"
RAYE_HELPER_EXPORT std::string source(std::string_view server, std::string_view share);

}
