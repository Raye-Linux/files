/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// raye-files-netdrive-helper: mounts and unmounts network drives for the
// person signed in. Started by pkexec (org.raye.files.network-drive):
//
//   mount <server> <share> <folder> <user> <domain>   password on stdin
//   unmount <folder>
//
// Exit codes: 0 done, 2 bad request, 3 folder refused, 4 mount.cifs failed
// (its message is on stderr), 5 not a network drive.

#include "helperrules.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace Raye::NetDrives::Helper;

namespace
{
// f_type of a mounted SMB share (fs/smb/client/cifsglob.h)
constexpr unsigned long cifsMagic = 0xFF534D42;
constexpr unsigned long smb2Magic = 0xFE534D42;

int fail(int code, const char *what)
{
    std::cerr << "raye-files-netdrive-helper: " << what << '\n';
    return code;
}

std::string canonical(const std::string &path)
{
    char *real = ::realpath(path.c_str(), nullptr);
    if (!real) {
        return {};
    }
    std::string out(real);
    ::free(real);
    return out;
}

std::string pathOfFd(int fd)
{
    char buf[4096];
    const std::string link = "/proc/self/fd/" + std::to_string(fd);
    const ssize_t n = ::readlink(link.c_str(), buf, sizeof(buf) - 1);
    return n > 0 ? std::string(buf, size_t(n)) : std::string();
}

std::string unescape(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 3 < s.size()) {
            const std::string oct = s.substr(i + 1, 3);
            if (oct.size() == 3 && oct.find_first_not_of("01234567") == std::string::npos) {
                out += char(std::stoi(oct, nullptr, 8));
                i += 3;
                continue;
            }
        }
        out += s[i];
    }
    return out;
}

// The file system type mounted exactly at path, or "".
std::string typeAt(const std::string &path)
{
    std::ifstream in("/proc/self/mountinfo");
    std::string line;
    std::string found;
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        std::vector<std::string> f;
        std::string w;
        while (fields >> w) {
            f.push_back(w);
        }
        size_t dash = 0;
        while (dash < f.size() && f[dash] != "-") {
            ++dash;
        }
        if (f.size() < 5 || dash + 1 >= f.size()) {
            continue;
        }
        if (unescape(f[4]) == path) {
            found = f[dash + 1]; // the last one mounted there wins
        }
    }
    return found;
}

struct Caller {
    uid_t uid = 0;
    gid_t gid = 0;
    std::string login;
    std::string home;
};

bool callerOf(Caller *c)
{
    const char *env = ::getenv("PKEXEC_UID");
    if (!env || !*env) {
        return false;
    }
    char *end = nullptr;
    const unsigned long uid = std::strtoul(env, &end, 10);
    if (*end || uid == 0) {
        return false;
    }
    const passwd *pw = ::getpwuid(uid_t(uid));
    if (!pw) {
        return false;
    }
    c->uid = pw->pw_uid;
    c->gid = pw->pw_gid;
    c->login = pw->pw_name;
    c->home = canonical(pw->pw_dir);
    return !c->home.empty();
}

// Opens the folder without following a link, and checks it is the caller's
// own, empty, not a mount yet, and in a place drives may go. The open
// descriptor is what gets mounted on, so the path cannot be swapped after.
int openFolder(const std::string &given, const Caller &c, bool forMount, std::string *path)
{
    const int fd = ::open(given.c_str(), O_PATH | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    *path = pathOfFd(fd);
    struct stat st = {};
    if (path->empty() || *path != canonical(given) || !allowedMountPoint(*path, c.home, c.login) || ::fstat(fd, &st) != 0 || !S_ISDIR(st.st_mode)
        || st.st_uid != c.uid) {
        ::close(fd);
        return -1;
    }
    if (forMount) {
        struct stat parent = {};
        const std::string up = path->substr(0, path->rfind('/'));
        if (::stat(up.empty() ? "/" : up.c_str(), &parent) != 0 || parent.st_dev != st.st_dev || !typeAt(*path).empty()) {
            ::close(fd);
            return -1;
        }
        const int dfd = ::openat(fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        DIR *dir = dfd >= 0 ? ::fdopendir(dfd) : nullptr;
        bool empty = dir != nullptr;
        while (dir && empty) {
            const dirent *e = ::readdir(dir);
            if (!e) {
                break;
            }
            empty = std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0;
        }
        if (dir) {
            ::closedir(dir);
        }
        if (!empty) {
            ::close(fd);
            return -1;
        }
    }
    return fd;
}

int doMount(const std::vector<std::string> &args, const Caller &c)
{
    if (args.size() != 5) {
        return fail(2, "mount needs server, share, folder, user and domain");
    }
    const std::string &server = args[0];
    const std::string &share = args[1];
    const std::string &user = args[3];
    const std::string &domain = args[4];
    if (!validServer(server) || !validShare(share) || !validCredential(user) || !validCredential(domain)) {
        return fail(2, "bad server, share or user name");
    }
    std::string password;
    if (!user.empty()) {
        char buf[514];
        size_t n = 0;
        ssize_t got = 0;
        while (n < sizeof(buf) && (got = ::read(STDIN_FILENO, buf + n, sizeof(buf) - n)) > 0) {
            n += size_t(got);
        }
        if (n > 0 && buf[n - 1] == '\n') {
            --n;
        }
        if (n > 512) {
            ::explicit_bzero(buf, sizeof(buf));
            return fail(2, "password too long");
        }
        password.assign(buf, n);
        ::explicit_bzero(buf, sizeof(buf));
    }
    std::string path;
    const int fd = openFolder(args[2], c, true, &path);
    if (fd < 0) {
        return fail(3, "this folder cannot take a network drive");
    }
    int pipeFd[2] = {-1, -1};
    if (!user.empty() && ::pipe(pipeFd) != 0) {
        return fail(4, "no pipe");
    }
    if (pipeFd[1] >= 0) {
        // Small enough for the pipe buffer: no reader is needed yet.
        if (::write(pipeFd[1], password.data(), password.size()) != ssize_t(password.size())) {
            return fail(4, "could not pass the password");
        }
        ::close(pipeFd[1]);
    }
    ::explicit_bzero(password.data(), password.size());

    const std::string options = cifsOptions(c.uid, c.gid, user, domain);
    const std::string src = source(server, share);
    // The folder may have been moved since it was checked: ask again.
    if (pathOfFd(fd) != path) {
        ::close(fd);
        return fail(3, "this folder cannot take a network drive");
    }
    const pid_t pid = ::fork();
    if (pid < 0) {
        return fail(4, "fork failed");
    }
    if (pid == 0) {
        if (::fchdir(fd) != 0) {
            ::_exit(4);
        }
        ::clearenv();
        ::setenv("PATH", "/usr/sbin:/usr/bin:/sbin:/bin", 1);
        if (pipeFd[0] >= 0) {
            ::setenv("PASSWD_FD", std::to_string(pipeFd[0]).c_str(), 1);
        }
        // "." is the folder opened above, whatever its path says now.
        const char *argv[] = {"mount.cifs", src.c_str(), ".", "-o", options.c_str(), nullptr};
        ::execv("/usr/sbin/mount.cifs", const_cast<char **>(argv));
        ::execv("/sbin/mount.cifs", const_cast<char **>(argv));
        std::cerr << "mount.cifs is not installed\n";
        ::_exit(4);
    }
    if (pipeFd[0] >= 0) {
        ::close(pipeFd[0]);
    }
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    ::close(fd);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 4;
}

int doUnmount(const std::vector<std::string> &args, const Caller &c)
{
    if (args.size() != 1) {
        return fail(2, "unmount needs a folder");
    }
    std::string path;
    const int fd = openFolder(args[0], c, false, &path);
    if (fd < 0) {
        return fail(3, "this folder is not one of your network drives");
    }
    // Everything below works on the folder opened above, never on the path
    // again: a parent swapped for a link cannot send umount elsewhere.
    struct statfs fs = {};
    struct stat self = {};
    struct stat parent = {};
    const int up = ::openat(fd, "..", O_PATH | O_DIRECTORY | O_CLOEXEC);
    const bool smb = ::fstatfs(fd, &fs) == 0 && (static_cast<unsigned long>(fs.f_type) == cifsMagic || static_cast<unsigned long>(fs.f_type) == smb2Magic);
    const bool root = up >= 0 && ::fstat(fd, &self) == 0 && ::fstat(up, &parent) == 0 && self.st_dev != parent.st_dev;
    if (up >= 0) {
        ::close(up);
    }
    if (!smb || !root) {
        ::close(fd);
        return fail(5, "this folder is not a network drive");
    }
    const std::string pinned = "/proc/self/fd/" + std::to_string(fd);
    int rc = ::umount2(pinned.c_str(), 0);
    // Something still has a file open on it: let it go once they close.
    if (rc != 0 && errno == EBUSY) {
        rc = ::umount2(pinned.c_str(), MNT_DETACH);
    }
    const int err = errno;
    ::close(fd);
    return rc == 0 ? 0 : fail(4, std::strerror(err));
}
}

int main(int argc, char **argv)
{
    if (::geteuid() != 0) {
        return fail(2, "must be started through pkexec");
    }
    Caller caller;
    if (!callerOf(&caller)) {
        return fail(2, "unknown caller");
    }
    if (argc < 2) {
        return fail(2, "usage: mount|unmount ...");
    }
    const std::string verb = argv[1];
    const std::vector<std::string> args(argv + 2, argv + argc);
    if (verb == "mount") {
        return doMount(args, caller);
    }
    if (verb == "unmount") {
        return doUnmount(args, caller);
    }
    return fail(2, "usage: mount|unmount ...");
}
