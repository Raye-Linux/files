/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archivepaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <cerrno>
#include <climits>
#include <sys/stat.h>
#include <unistd.h>

namespace Raye::Archive
{

namespace
{
// What Linux file systems take: 255 bytes a name, 4096 a path.
constexpr int nameMaxBytes = 255;
constexpr int pathMaxBytes = 4096;
// More links than this on one path and the kernel gives up too (ELOOP).
constexpr int linkHopLimit = 40;

bool isDriveLetter(QStringView part)
{
    return part.size() == 2 && part.at(1) == QLatin1Char(':') && part.at(0).isLetter() && part.at(0).unicode() < 128;
}
}

PathVerdict cleanEntryPath(const QString &name, QString *clean)
{
    if (name.contains(QChar(0))) {
        return PathVerdict::Invalid;
    }
    QString path = name;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));

    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QStringList kept;
    kept.reserve(parts.size());
    for (int i = 0; i < parts.size(); ++i) {
        const QString &part = parts.at(i);
        if (part == QLatin1String(".")) {
            continue;
        }
        if (part == QLatin1String("..")) {
            return PathVerdict::Traversal;
        }
        // "C:" at the very start is a drive, not a folder.
        if (i == 0 && isDriveLetter(part) && !path.startsWith(QLatin1Char('/'))) {
            continue;
        }
        if (part.toUtf8().size() > nameMaxBytes) {
            return PathVerdict::Invalid;
        }
        kept.append(part);
    }
    if (kept.isEmpty()) {
        return PathVerdict::Empty;
    }
    const QString joined = kept.join(QLatin1Char('/'));
    if (joined.toUtf8().size() > pathMaxBytes) {
        return PathVerdict::Invalid;
    }
    *clean = joined;
    return PathVerdict::Ok;
}

bool linkTargetStaysInside(const QString &entryPath, const QString &target)
{
    if (target.isEmpty() || target.startsWith(QLatin1Char('/')) || target.contains(QChar(0))) {
        return false;
    }
    // Depth of the folder the link sits in.
    int depth = entryPath.split(QLatin1Char('/'), Qt::SkipEmptyParts).size() - 1;
    if (depth < 0) {
        return false;
    }
    const QStringList parts = target.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == QLatin1String(".")) {
            continue;
        }
        if (part == QLatin1String("..")) {
            if (--depth < 0) {
                return false;
            }
        } else {
            ++depth;
        }
    }
    return true;
}

namespace
{
QString readLink(const QString &path, bool *ok)
{
    QByteArray buffer(PATH_MAX, '\0');
    const ssize_t n = ::readlink(QFile::encodeName(path).constData(), buffer.data(), buffer.size());
    if (n < 0 || n >= buffer.size()) {
        *ok = false;
        return QString();
    }
    *ok = true;
    buffer.truncate(n);
    return QFile::decodeName(buffer);
}

bool isInside(const QString &root, const QString &path)
{
    return path == root || path.startsWith(root + QLatin1Char('/')) || root == QLatin1String("/");
}

// Walks path one part at a time from "/", following links the way the
// kernel does, and returns where it ends up. Parts that do not exist are
// kept as text. Empty when there are too many links.
QString resolve(const QString &path)
{
    QStringList todo = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QStringList done;
    int hops = 0;
    bool missing = false;
    while (!todo.isEmpty()) {
        const QString part = todo.takeFirst();
        if (part == QLatin1String(".")) {
            continue;
        }
        if (part == QLatin1String("..")) {
            if (!done.isEmpty()) {
                done.removeLast();
            }
            // Back on a path that may exist: links count again (the
            // missing folder could be made later).
            missing = false;
            continue;
        }
        done.append(part);
        if (missing) {
            continue;
        }
        const QString current = QLatin1Char('/') + done.join(QLatin1Char('/'));
        struct stat st;
        if (::lstat(QFile::encodeName(current).constData(), &st) != 0) {
            // Once a part is missing nothing below it exists either.
            missing = true;
            continue;
        }
        if (!S_ISLNK(st.st_mode)) {
            continue;
        }
        if (++hops > linkHopLimit) {
            return QString();
        }
        bool ok = false;
        const QString target = readLink(current, &ok);
        if (!ok) {
            return QString();
        }
        done.removeLast();
        if (target.startsWith(QLatin1Char('/'))) {
            done.clear();
        }
        todo = target.split(QLatin1Char('/'), Qt::SkipEmptyParts) + todo;
    }
    return QLatin1Char('/') + done.join(QLatin1Char('/'));
}
}

bool resolvesInside(const QString &root, const QString &path)
{
    const QString realRoot = QFileInfo(root).canonicalFilePath();
    if (realRoot.isEmpty() || !QDir::isAbsolutePath(path)) {
        return false;
    }
    const QString end = resolve(path);
    return !end.isEmpty() && isInside(realRoot, end);
}

}
