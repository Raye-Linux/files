/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "detailsmodel.h"

#include "typenames/filenames.h"
#include "typenames/typenames.h"

#include <KFormat>
#include <KLocalizedString>

#include <QDir>
#include <QImageReader>
#include <QLocale>

namespace
{
QString dateText(const QDateTime &time)
{
    return time.isValid() ? QLocale().toString(time, QLocale::ShortFormat) : QString();
}

QString sizeText(KIO::filesize_t size)
{
    // KB and MB as Windows writes them (multiples of 1024).
    const QString human = KFormat().formatByteSize(double(size), 1, KFormat::JEDECBinaryDialect);
    // Windows writes the exact count next to the rounded one.
    return i18nc("@item size, then the exact count of bytes", "%1 (%2 bytes)", human, QLocale().toString(qulonglong(size)));
}

QString typeOf(const KFileItem &item)
{
    if (item.isDir()) {
        return Raye::TypeNames::name(QStringLiteral("inode/directory"));
    }
    return Raye::TypeNames::name(item.mimetype(), item.mimeComment());
}
}

namespace Raye::Preview
{

QString locationText(const QUrl &folder, const QString &homeIn)
{
    if (!folder.isLocalFile()) {
        return folder.toDisplayString(QUrl::PreferLocalFile | QUrl::StripTrailingSlash | QUrl::RemovePassword);
    }
    const QString home = QDir::cleanPath(homeIn.isEmpty() ? QDir::homePath() : homeIn);
    const QString path = QDir::cleanPath(folder.toLocalFile());
    const QString arrow = QStringLiteral(" › ");
    if (path == home) {
        return QFileInfo(home).fileName();
    }
    if (path.startsWith(home + QLatin1Char('/'))) {
        return path.mid(home.size() + 1).split(QLatin1Char('/'), Qt::SkipEmptyParts).join(arrow);
    }
    QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    parts.prepend(i18nc("@item the system drive", "Raye"));
    return parts.join(arrow);
}

QString dateRange(const QDateTime &first, const QDateTime &last)
{
    if (!first.isValid() || !last.isValid()) {
        return QString();
    }
    const QLocale locale;
    if (first.date() == last.date()) {
        return locale.toString(first.date(), QLocale::ShortFormat);
    }
    return i18nc("@item a range of dates", "%1 – %2", locale.toString(first.date(), QLocale::ShortFormat), locale.toString(last.date(), QLocale::ShortFormat));
}

Details detailsFor(const KFileItemList &selection, const KFileItem &folder, const QString &home)
{
    Details d;
    const QString typeLabel = i18nc("@label details pane", "Type");
    const QString sizeLabel = i18nc("@label details pane", "Size");
    const QString locationLabel = i18nc("@label details pane", "Location");
    const QString modifiedLabel = i18nc("@label details pane", "Date modified");
    const QString createdLabel = i18nc("@label details pane", "Date created");

    if (selection.size() > 1) {
        d.multiple = true;
        d.title = i18ncp("@label details pane", "%1 item selected", "%1 items selected", selection.size());
        KIO::filesize_t total = 0;
        bool hasFiles = false;
        QDateTime first;
        QDateTime last;
        for (const KFileItem &item : selection) {
            if (!item.isDir()) {
                total += item.size();
                hasFiles = true;
            }
            const QDateTime t = item.time(KFileItem::ModificationTime);
            if (t.isValid()) {
                if (!first.isValid() || t < first) {
                    first = t;
                }
                if (!last.isValid() || t > last) {
                    last = t;
                }
            }
        }
        if (hasFiles) {
            d.rows.append({i18nc("@label details pane, the size of every selected file", "Total size"), sizeText(total), QUrl()});
        }
        const QString range = dateRange(first, last);
        if (!range.isEmpty()) {
            d.rows.append({modifiedLabel, range, QUrl()});
        }
        return d;
    }

    const KFileItem item = selection.isEmpty() ? folder : selection.first();
    if (item.isNull()) {
        return d;
    }
    d.title = item.isDir() ? item.text() : FileNames::displayName(item.text(), false, false);
    // The name with its extension stays readable in the title for programs.
    if (!item.isDir() && FileNames::alwaysShowsExtension(item.text())) {
        d.title = item.text();
    }
    d.rows.append({typeLabel, typeOf(item), QUrl()});
    if (!item.isDir()) {
        d.rows.append({sizeLabel, sizeText(item.size()), QUrl()});
    }
    const QUrl parent = item.url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    if (!parent.isEmpty() && parent != item.url()) {
        d.rows.append({locationLabel, locationText(parent, home), parent});
    }
    const QString modified = dateText(item.time(KFileItem::ModificationTime));
    if (!modified.isEmpty()) {
        d.rows.append({modifiedLabel, modified, QUrl()});
    }
    const QString created = dateText(item.time(KFileItem::CreationTime));
    if (!created.isEmpty()) {
        d.rows.append({createdLabel, created, QUrl()});
    }
    if (item.isLocalFile() && item.mimetype().startsWith(QLatin1String("image/"))) {
        // The header only: cheap, even for a big picture.
        QImageReader reader(item.localPath());
        const QSize size = reader.size();
        if (size.isValid()) {
            d.rows.append({i18nc("@label details pane", "Dimensions"), i18nc("@item width × height in pixels", "%1 × %2", size.width(), size.height()), QUrl()});
        }
    }
    return d;
}

}
