/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "request.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <optional>

namespace Raye::Portal
{

bool FileType::showsEverything() const
{
    if (!mimeTypes.isEmpty()) {
        return false;
    }
    return globs.isEmpty() || globs.contains(QLatin1String("*")) || globs.contains(QLatin1String("*.*"));
}

QString FileType::displayName() const
{
    if (globs.isEmpty() || label.contains(QLatin1Char('*'))) {
        return label;
    }
    return i18nc("@item:inlistbox a file type and its name patterns", "%1 (%2)", label, globs.join(QLatin1String(", ")));
}

QString decodePath(const QByteArray &bytes)
{
    QByteArray path = bytes;
    while (path.endsWith('\0')) {
        path.chop(1);
    }
    return QFile::decodeName(path);
}

QString cleanLabel(const QString &label)
{
    QString out;
    out.reserve(label.size());
    for (int i = 0; i < label.size(); ++i) {
        const QChar c = label.at(i);
        if (c == QLatin1Char('_')) {
            // "__" is a real underscore.
            if (i + 1 < label.size() && label.at(i + 1) == QLatin1Char('_')) {
                out.append(c);
                ++i;
            }
            continue;
        }
        out.append(c);
    }
    return out;
}

QString simplifyGlob(const QString &glob)
{
    static const QRegularExpression pair(QStringLiteral("\\[([a-zA-Z])([a-zA-Z])\\]"));
    QString out = glob;
    int from = 0;
    while (true) {
        const QRegularExpressionMatch m = pair.match(out, from);
        if (!m.hasMatch()) {
            return out;
        }
        const QString a = m.captured(1);
        const QString b = m.captured(2);
        if (a.compare(b, Qt::CaseInsensitive) != 0 || a == b) {
            return glob;
        }
        out.replace(m.capturedStart(), m.capturedLength(), a.toLower());
        from = m.capturedStart() + 1;
    }
}

namespace
{

// "a\"b" for a"b: in quotes, \" is a quote and \\ a backslash.
QString quoted(const QString &name)
{
    QString out = QStringLiteral("\"");
    for (const QChar c : name) {
        if (c == QLatin1Char('"') || c == QLatin1Char('\\')) {
            out.append(QLatin1Char('\\'));
        }
        out.append(c);
    }
    out.append(QLatin1Char('"'));
    return out;
}

// The field as a list of quoted names, or nothing when it is not one.
std::optional<QStringList> quotedList(const QString &text)
{
    QStringList names;
    int at = 0;
    const int size = text.size();
    while (at < size) {
        if (text.at(at).isSpace()) {
            ++at;
            continue;
        }
        if (text.at(at) != QLatin1Char('"')) {
            return std::nullopt;
        }
        QString name;
        bool closed = false;
        for (++at; at < size; ++at) {
            const QChar c = text.at(at);
            if (c == QLatin1Char('\\') && at + 1 < size && (text.at(at + 1) == QLatin1Char('"') || text.at(at + 1) == QLatin1Char('\\'))) {
                name.append(text.at(++at));
            } else if (c == QLatin1Char('"')) {
                closed = true;
                ++at;
                break;
            } else {
                name.append(c);
            }
        }
        if (closed && at < size && !text.at(at).isSpace()) {
            // "report".txt is one name, not a list.
            return std::nullopt;
        }
        // An opening quote and no end: the rest is the name.
        if (!name.isEmpty()) {
            names.append(name);
        }
    }
    return names;
}

}

QString fieldFromNames(const QStringList &names)
{
    if (names.size() == 1 && namesFromField(names.first()) == names) {
        return names.first();
    }
    QStringList out;
    out.reserve(names.size());
    for (const QString &name : names) {
        out.append(quoted(name));
    }
    return out.join(QLatin1Char(' '));
}

QStringList namesFromField(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    if (trimmed.startsWith(QLatin1Char('"'))) {
        if (const std::optional<QStringList> names = quotedList(trimmed)) {
            return *names;
        }
    }
    return {trimmed};
}

QUrl resolveName(const QString &name, const QUrl &folder)
{
    if (name.startsWith(QLatin1String("~/")) || name == QLatin1String("~")) {
        return QUrl::fromLocalFile(QDir::homePath() + name.mid(1));
    }
    if (QDir::isAbsolutePath(name)) {
        return QUrl::fromLocalFile(QDir::cleanPath(name));
    }
    if (folder.isLocalFile()) {
        return QUrl::fromLocalFile(QDir::cleanPath(QDir(folder.toLocalFile()).filePath(name)));
    }
    QUrl url = folder;
    QString path = folder.path();
    if (!path.endsWith(QLatin1Char('/'))) {
        path.append(QLatin1Char('/'));
    }
    url.setPath(path + name);
    return url;
}

QString withTypeExtension(const QString &name, const FileType &type)
{
    // Only the file's own name counts: folder.v1/report has no extension.
    if (name.isEmpty() || name.mid(name.lastIndexOf(QLatin1Char('/')) + 1).contains(QLatin1Char('.'))) {
        return name;
    }
    for (const QString &glob : type.globs) {
        static const QRegularExpression literal(QStringLiteral("^\\*(\\.[A-Za-z0-9_+-]+)$"));
        const QRegularExpressionMatch m = literal.match(simplifyGlob(glob));
        if (m.hasMatch()) {
            return name + m.captured(1);
        }
    }
    return name;
}

}
