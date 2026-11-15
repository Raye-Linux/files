/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "typenames.h"

#include <KLocalizedString>

#include <QHash>
#include <QMutex>

#include <array>
#include <QMimeDatabase>

namespace
{
// The catalog script (Messages.sh) collects I18NC_NOOP strings; the
// translation is looked up when a name is asked for.
#define I18NC_NOOP(context, text) context, text

struct Entry {
    const char *mimeType;
    const char *context;
    const char *text;
};

// Windows' own words where Windows has one, sentence case like the rest of
// Raye. Several MIME types can share a name: the archive and executable
// families keep one word each, whichever tool wrote the file.
constexpr Entry table[] = {
    {"inode/directory", I18NC_NOOP("@item file type", "File folder")},
    {"text/plain", I18NC_NOOP("@item file type", "Text document")},
    {"text/x-log", I18NC_NOOP("@item file type", "Log file")},
    {"text/markdown", I18NC_NOOP("@item file type", "Markdown file")},
    {"text/csv", I18NC_NOOP("@item file type", "CSV file")},
    {"text/html", I18NC_NOOP("@item file type", "HTML document")},
    {"text/css", I18NC_NOOP("@item file type", "CSS file")},
    {"text/javascript", I18NC_NOOP("@item file type", "JavaScript file")},
    {"application/javascript", I18NC_NOOP("@item file type", "JavaScript file")},
    {"application/json", I18NC_NOOP("@item file type", "JSON file")},
    {"application/xml", I18NC_NOOP("@item file type", "XML document")},
    {"text/xml", I18NC_NOOP("@item file type", "XML document")},
    {"application/yaml", I18NC_NOOP("@item file type", "YAML file")},
    {"application/x-yaml", I18NC_NOOP("@item file type", "YAML file")},
    {"text/x-python", I18NC_NOOP("@item file type", "Python file")},
    {"text/x-python3", I18NC_NOOP("@item file type", "Python file")},
    {"text/x-csrc", I18NC_NOOP("@item file type", "C source file")},
    {"text/x-c++src", I18NC_NOOP("@item file type", "C++ source file")},
    {"text/x-chdr", I18NC_NOOP("@item file type", "C header file")},
    {"text/x-c++hdr", I18NC_NOOP("@item file type", "C++ header file")},
    {"text/rtf", I18NC_NOOP("@item file type", "Rich text document")},
    {"application/rtf", I18NC_NOOP("@item file type", "Rich text document")},
    {"text/calendar", I18NC_NOOP("@item file type", "Calendar file")},
    {"text/vcard", I18NC_NOOP("@item file type", "Contact file")},

    {"image/png", I18NC_NOOP("@item file type", "PNG file")},
    {"image/jpeg", I18NC_NOOP("@item file type", "JPG file")},
    {"image/gif", I18NC_NOOP("@item file type", "GIF file")},
    {"image/bmp", I18NC_NOOP("@item file type", "Bitmap image")},
    {"image/webp", I18NC_NOOP("@item file type", "WEBP file")},
    {"image/avif", I18NC_NOOP("@item file type", "AVIF file")},
    {"image/heif", I18NC_NOOP("@item file type", "HEIC file")},
    {"image/tiff", I18NC_NOOP("@item file type", "TIFF file")},
    {"image/svg+xml", I18NC_NOOP("@item file type", "SVG document")},
    {"image/vnd.microsoft.icon", I18NC_NOOP("@item file type", "Icon")},
    {"image/vnd.adobe.photoshop", I18NC_NOOP("@item file type", "PSD file")},

    {"audio/mpeg", I18NC_NOOP("@item file type", "MP3 file")},
    {"audio/flac", I18NC_NOOP("@item file type", "FLAC file")},
    {"audio/x-wav", I18NC_NOOP("@item file type", "WAV file")},
    {"audio/ogg", I18NC_NOOP("@item file type", "OGG file")},
    {"audio/x-vorbis+ogg", I18NC_NOOP("@item file type", "OGG file")},
    {"audio/x-opus+ogg", I18NC_NOOP("@item file type", "OPUS file")},
    {"audio/mp4", I18NC_NOOP("@item file type", "M4A file")},
    {"audio/aac", I18NC_NOOP("@item file type", "AAC file")},
    {"video/mp4", I18NC_NOOP("@item file type", "MP4 file")},
    {"video/x-matroska", I18NC_NOOP("@item file type", "MKV file")},
    {"video/webm", I18NC_NOOP("@item file type", "WEBM file")},
    {"video/quicktime", I18NC_NOOP("@item file type", "MOV file")},
    {"video/x-msvideo", I18NC_NOOP("@item file type", "AVI file")},

    {"application/pdf", I18NC_NOOP("@item file type", "PDF document")},
    {"application/epub+zip", I18NC_NOOP("@item file type", "EPUB book")},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.document", I18NC_NOOP("@item file type", "Word document")},
    {"application/msword", I18NC_NOOP("@item file type", "Word 97-2003 document")},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", I18NC_NOOP("@item file type", "Excel worksheet")},
    {"application/vnd.ms-excel", I18NC_NOOP("@item file type", "Excel 97-2003 worksheet")},
    {"application/vnd.openxmlformats-officedocument.presentationml.presentation", I18NC_NOOP("@item file type", "PowerPoint presentation")},
    {"application/vnd.ms-powerpoint", I18NC_NOOP("@item file type", "PowerPoint 97-2003 presentation")},
    {"application/vnd.oasis.opendocument.text", I18NC_NOOP("@item file type", "OpenDocument text")},
    {"application/vnd.oasis.opendocument.spreadsheet", I18NC_NOOP("@item file type", "OpenDocument spreadsheet")},
    {"application/vnd.oasis.opendocument.presentation", I18NC_NOOP("@item file type", "OpenDocument presentation")},

    {"application/zip", I18NC_NOOP("@item file type", "Compressed (zipped) folder")},
    {"application/x-7z-compressed", I18NC_NOOP("@item file type", "7Z archive")},
    {"application/vnd.rar", I18NC_NOOP("@item file type", "RAR archive")},
    {"application/x-tar", I18NC_NOOP("@item file type", "TAR archive")},
    {"application/x-compressed-tar", I18NC_NOOP("@item file type", "TAR.GZ archive")},
    {"application/x-xz-compressed-tar", I18NC_NOOP("@item file type", "TAR.XZ archive")},
    {"application/x-bzip2-compressed-tar", I18NC_NOOP("@item file type", "TAR.BZ2 archive")},
    {"application/x-zstd-compressed-tar", I18NC_NOOP("@item file type", "TAR.ZST archive")},
    {"application/gzip", I18NC_NOOP("@item file type", "GZ archive")},
    {"application/x-cd-image", I18NC_NOOP("@item file type", "Disc image file")},
    {"application/vnd.efi.iso", I18NC_NOOP("@item file type", "Disc image file")},

    {"application/x-msdownload", I18NC_NOOP("@item file type", "Application")},
    {"application/x-ms-dos-executable", I18NC_NOOP("@item file type", "Application")},
    {"application/x-ms-ne-executable", I18NC_NOOP("@item file type", "Application")},
    {"application/x-executable", I18NC_NOOP("@item file type", "Application")},
    {"application/vnd.appimage", I18NC_NOOP("@item file type", "Application")},
    {"application/x-msi", I18NC_NOOP("@item file type", "Windows Installer package")},
    {"application/x-sharedlib", I18NC_NOOP("@item file type", "Application extension")},
    {"application/x-bat", I18NC_NOOP("@item file type", "Windows batch file")},
    {"application/x-shellscript", I18NC_NOOP("@item file type", "Shell script")},
    {"application/x-desktop", I18NC_NOOP("@item file type", "Shortcut")},
    {"application/x-ms-shortcut", I18NC_NOOP("@item file type", "Shortcut")},
    {"application/vnd.debian.binary-package", I18NC_NOOP("@item file type", "Software package")},
    {"application/x-java-archive", I18NC_NOOP("@item file type", "JAR file")},
    {"application/vnd.sqlite3", I18NC_NOOP("@item file type", "SQLite database")},

    {"font/ttf", I18NC_NOOP("@item file type", "TrueType font file")},
    {"font/otf", I18NC_NOOP("@item file type", "OpenType font file")},
    {"font/woff", I18NC_NOOP("@item file type", "Web font file")},
    {"font/woff2", I18NC_NOOP("@item file type", "Web font file")},
};

// Index of each table row by MIME type, with the canonical name of every
// row added too, so a table written with an alias still matches.
const QHash<QString, int> &index()
{
    static const QHash<QString, int> hash = [] {
        QHash<QString, int> h;
        QMimeDatabase db;
        const int n = int(std::size(table));
        for (int i = 0; i < n; ++i) {
            const QString written = QString::fromLatin1(table[i].mimeType);
            h.insert(written, i);
            const QMimeType type = db.mimeTypeForName(written);
            if (type.isValid() && !h.contains(type.name())) {
                h.insert(type.name(), i);
            }
        }
        return h;
    }();
    return hash;
}

int rowOf(const QString &mimeType)
{
    if (mimeType.isEmpty()) {
        return -1;
    }
    const QHash<QString, int> &h = index();
    auto it = h.constFind(mimeType);
    if (it != h.constEnd()) {
        return it.value();
    }
    // An alias ("image/x-png", "application/x-zip-compressed"): the database
    // knows its real name.
    const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
    if (type.isValid() && type.name() != mimeType) {
        it = h.constFind(type.name());
        if (it != h.constEnd()) {
            return it.value();
        }
    }
    return -1;
}
}

namespace Raye::TypeNames
{

QString name(const QString &mimeType, const QString &fallback)
{
    const int row = rowOf(mimeType);
    if (row >= 0) {
        // Translated once per row and language: the Type column asks for
        // the same few names thousands of times.
        static QMutex mutex;
        static QStringList languages;
        static std::array<QString, std::size(table)> cache;
        QMutexLocker lock(&mutex);
        const QStringList current = KLocalizedString::languages();
        if (current != languages) {
            languages = current;
            cache.fill(QString());
        }
        QString &text = cache[row];
        if (text.isEmpty()) {
            text = i18nc(table[row].context, table[row].text);
        }
        return text;
    }
    if (!fallback.isEmpty()) {
        return fallback;
    }
    const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
    return type.isValid() ? type.comment() : QString();
}

bool hasName(const QString &mimeType)
{
    return rowOf(mimeType) >= 0;
}

QStringList mimeTypes()
{
    QStringList list;
    for (const Entry &entry : table) {
        list.append(QString::fromLatin1(entry.mimeType));
    }
    return list;
}

}
