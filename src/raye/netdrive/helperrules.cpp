/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "helperrules.h"

#include <algorithm>

namespace Raye::NetDrives::Helper
{
namespace
{
bool isControl(char c)
{
    return static_cast<unsigned char>(c) < 0x20 || c == 0x7f;
}

bool isAlnum(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool startsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}
}

bool validServer(std::string_view server)
{
    if (server.empty() || server.size() > 253) {
        return false;
    }
    // IPv6, written without brackets ("fe80::1").
    if (server.find(':') != std::string_view::npos) {
        return std::all_of(server.begin(), server.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == ':' || c == '.';
        });
    }
    if (!isAlnum(server.front()) || !isAlnum(server.back())) {
        return false;
    }
    return std::all_of(server.begin(), server.end(), [](char c) {
        return isAlnum(c) || c == '.' || c == '-' || c == '_';
    });
}

bool validShare(std::string_view share)
{
    if (share.empty() || share.size() > 80 || share == "." || share == "..") {
        return false;
    }
    return std::none_of(share.begin(), share.end(), [](char c) {
        return isControl(c) || c == '/' || c == '\\' || c == ',';
    });
}

bool validCredential(std::string_view text)
{
    if (text.size() > 256) {
        return false;
    }
    return std::none_of(text.begin(), text.end(), [](char c) {
        return isControl(c) || c == ',' || c == '=';
    });
}

std::string cifsOptions(uid_t uid, gid_t gid, std::string_view user, std::string_view domain)
{
    std::string o = "uid=" + std::to_string(uid) + ",gid=" + std::to_string(gid)
        + ",forceuid,forcegid,file_mode=0700,dir_mode=0700,nosuid,nodev,iocharset=utf8,cache=strict";
    if (user.empty()) {
        o += ",guest";
    } else {
        o += ",username=";
        o += user;
        if (!domain.empty()) {
            o += ",domain=";
            o += domain;
        }
    }
    return o;
}

bool allowedMountPoint(std::string_view path, std::string_view home, std::string_view login)
{
    if (path.empty() || path.front() != '/' || path.find("/..") != std::string_view::npos || path.find("//") != std::string_view::npos
        || path.back() == '/') {
        return false;
    }
    const auto inside = [path](std::string_view base) {
        if (base.empty() || base == "/") {
            return false;
        }
        std::string b(base);
        while (b.size() > 1 && b.back() == '/') {
            b.pop_back();
        }
        return startsWith(path, b + "/") && path.size() > b.size() + 1;
    };
    if (inside(home)) {
        return true;
    }
    return !login.empty() && login.find('/') == std::string_view::npos && login != "." && login != ".."
        && inside(std::string("/media/") + std::string(login));
}

std::string source(std::string_view server, std::string_view share)
{
    return "//" + std::string(server) + "/" + std::string(share);
}

}
