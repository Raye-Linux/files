/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "filenames.h"

#include <KLocalizedString>

#include <QLocale>
#include <QHash>

#include <algorithm>
#include <QMimeDatabase>
#include <QMutex>
#include <QSet>

namespace
{
// Extensions of things that run or launch something. Compared without case:
// ".EXE" from a FAT stick is still a program.
const QSet<QString> &runnableExtensions()
{
    static const QSet<QString> set = {
        QStringLiteral("exe"),     QStringLiteral("msi"),  QStringLiteral("bat"), QStringLiteral("cmd"), QStringLiteral("com"),
        QStringLiteral("scr"),     QStringLiteral("pif"),  QStringLiteral("lnk"), QStringLiteral("ps1"), QStringLiteral("vbs"),
        QStringLiteral("js"),      QStringLiteral("jar"),  QStringLiteral("sh"),  QStringLiteral("run"), QStringLiteral("bin"),
        QStringLiteral("desktop"), QStringLiteral("appimage"), QStringLiteral("py"), QStringLiteral("pl"), QStringLiteral("deb"),
    };
    return set;
}

// What follows the last dot, if the dot is neither first nor last.
QString lastSuffix(const QString &fileName)
{
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    if (dot < 1 || dot == fileName.size() - 1) {
        return QString();
    }
    return fileName.mid(dot + 1);
}
}

namespace Raye::FileNames
{

QString knownExtension(const QString &fileName)
{
    if (fileName.isEmpty()) {
        return QString();
    }
    // The type database matches globs, which is slow for 10 000 names; the
    // answer for a given ending never changes, so it is kept per ending.
    // Up to three parts are tried, longest first, like suffixForFileName().
    static QMutex mutex;
    static QHash<QString, bool> known;
    static const QMimeDatabase db;

    int dot = fileName.size();
    QString best;
    for (int parts = 0; parts < 3; ++parts) {
        dot = fileName.lastIndexOf(QLatin1Char('.'), dot - 1);
        if (dot < 1 || dot == fileName.size() - 1) {
            break; // ".bashrc": an ending is never the whole name
        }
        QString key = fileName.mid(dot + 1);
        const bool lower = std::none_of(key.cbegin(), key.cend(), [](QChar c) {
            return c.isUpper();
        });
        if (!lower) {
            key = key.toLower();
        }
        bool isKnown;
        {
            QMutexLocker lock(&mutex);
            auto it = known.constFind(key);
            if (it != known.constEnd()) {
                isKnown = it.value();
            } else {
                isKnown = db.suffixForFileName(QLatin1String("x.") + key) == key;
                known.insert(key, isKnown);
            }
        }
        if (isKnown) {
            best = fileName.mid(dot + 1);
        }
    }
    return best;
}

bool alwaysShowsExtension(const QString &fileName)
{
    const QString suffix = lastSuffix(fileName).toLower();
    return !suffix.isEmpty() && runnableExtensions().contains(suffix);
}

QString displayName(const QString &fileName, bool isDir, bool showExtensions, bool isExecutable)
{
    if (isDir || showExtensions || isExecutable || alwaysShowsExtension(fileName)) {
        return fileName;
    }
    const QString extension = knownExtension(fileName);
    if (extension.isEmpty()) {
        return fileName;
    }
    return fileName.left(fileName.size() - extension.size() - 1);
}

int baseNameLength(const QString &fileName, bool isDir)
{
    if (isDir) {
        return fileName.size();
    }
    QString extension = knownExtension(fileName);
    if (extension.isEmpty()) {
        extension = lastSuffix(fileName);
    }
    if (extension.isEmpty()) {
        return fileName.size();
    }
    return fileName.size() - extension.size() - 1;
}

QString sizeColumnText(qint64 bytes)
{
    const qint64 kilobytes = bytes <= 0 ? 0 : (bytes + 1023) / 1024;
    return i18nc("@item file size in whole kilobytes", "%1 KB", QLocale().toString(kilobytes));
}

}
