/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "droprules.h"

#include <QDir>
#include <QFileInfo>

#include <sys/stat.h>

namespace
{
// The device of the nearest folder that exists: a target folder may be a
// mount point, and a source that has just gone away still says where it was.
bool deviceOf(const QString &path, dev_t *device)
{
    QString p = QDir::cleanPath(path);
    for (int guard = 0; guard < 256 && !p.isEmpty(); ++guard) {
        struct stat st;
        if (::stat(QFile::encodeName(p).constData(), &st) == 0) {
            *device = st.st_dev;
            return true;
        }
        const QString up = QFileInfo(p).path();
        if (up == p) {
            break;
        }
        p = up;
    }
    return false;
}
}

namespace Raye::NavPane
{

Qt::DropAction dropAction(Qt::KeyboardModifiers modifiers, bool sameDrive, bool toRecycleBin)
{
    if (toRecycleBin) {
        return Qt::MoveAction;
    }
    const bool shift = modifiers & Qt::ShiftModifier;
    const bool ctrl = modifiers & Qt::ControlModifier;
    const bool alt = modifiers & Qt::AltModifier;
    if (alt || (ctrl && shift)) {
        return Qt::LinkAction;
    }
    if (shift) {
        return Qt::MoveAction;
    }
    if (ctrl) {
        return Qt::CopyAction;
    }
    return sameDrive ? Qt::MoveAction : Qt::CopyAction;
}

bool sameDrive(const QList<QUrl> &sources, const QUrl &target)
{
    if (sources.isEmpty() || !target.isValid()) {
        return false;
    }
    if (target.isLocalFile()) {
        dev_t targetDevice;
        if (!deviceOf(target.toLocalFile(), &targetDevice)) {
            return false;
        }
        for (const QUrl &source : sources) {
            dev_t d;
            if (!source.isLocalFile() || !deviceOf(source.toLocalFile(), &d) || d != targetDevice) {
                return false;
            }
        }
        return true;
    }
    for (const QUrl &source : sources) {
        if (source.scheme() != target.scheme() || source.host() != target.host() || source.port() != target.port()) {
            return false;
        }
    }
    return true;
}

bool isPointlessDrop(const QList<QUrl> &sources, const QUrl &target, Qt::DropAction action)
{
    const QUrl t = target.adjusted(QUrl::StripTrailingSlash);
    for (const QUrl &source : sources) {
        const QUrl s = source.adjusted(QUrl::StripTrailingSlash);
        if (s == t || s.isParentOf(t)) {
            return true;
        }
        if (action == Qt::MoveAction && s.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash) == t) {
            return true;
        }
    }
    return false;
}

}
