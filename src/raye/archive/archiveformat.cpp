/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archiveformat.h"

#include <KLocalizedString>

#include <raye/archive/format.h>

#include <QDir>
#include <QFile>

#include <array>
#include <sys/stat.h>

namespace Raye::Archive
{

namespace
{
struct Known {
    const char *extension;
    const char *format;
};

// Longest first: ".tar.gz" must win over ".gz".
constexpr std::array<Known, 30> knownExtensions{{
    {".tar.gz", "TAR"},
    {".tar.bz2", "TAR"},
    {".tar.xz", "TAR"},
    {".tar.zst", "TAR"},
    {".tar.lzma", "TAR"},
    {".tar.lz", "TAR"},
    {".tar.Z", "TAR"},
    {".tbz2", "TAR"},
    {".tzst", "TAR"},
    {".tgz", "TAR"},
    {".tbz", "TAR"},
    {".txz", "TAR"},
    {".zip", "ZIP"},
    {".tar", "TAR"},
    {".rar", "RAR"},
    {".iso", "ISO"},
    {".cab", "CAB"},
    {".7z", "7z"},
    {".tlz", "TAR"},
    {".cpio", "CPIO"},
    {".deb", "DEB"},
    {".rpm", "RPM"},
    {".gz", "GZ"},
    {".bz2", "BZ2"},
    {".xz", "XZ"},
    {".zst", "ZST"},
    {".lz4", "LZ4"},
    {".lz", "LZ"},
    {".lzma", "LZMA"},
    {".Z", "Z"},
}};

const Known *knownFor(const QString &fileName)
{
    for (const Known &k : knownExtensions) {
        if (fileName.endsWith(QLatin1String(k.extension), Qt::CaseInsensitive)) {
            return &k;
        }
    }
    return nullptr;
}

const QStringList &browsable()
{
    static const QStringList types{
        QStringLiteral("application/zip"),
        QStringLiteral("application/x-7z-compressed"),
        QStringLiteral("application/vnd.rar"),
        QStringLiteral("application/x-rar"),
        QStringLiteral("application/x-tar"),
        QStringLiteral("application/x-compressed-tar"),
        QStringLiteral("application/x-bzip-compressed-tar"),
        QStringLiteral("application/x-bzip2-compressed-tar"),
        QStringLiteral("application/x-xz-compressed-tar"),
        QStringLiteral("application/x-zstd-compressed-tar"),
        QStringLiteral("application/x-lzma-compressed-tar"),
        QStringLiteral("application/x-lzip-compressed-tar"),
        QStringLiteral("application/x-cd-image"),
        QStringLiteral("application/vnd.efi.iso"),
        QStringLiteral("application/vnd.ms-cab-compressed"),
        QStringLiteral("application/x-tarz"),
        QStringLiteral("application/x-lz4-compressed-tar"),
        QStringLiteral("application/x-cpio"),
        QStringLiteral("application/x-archive"),
        QStringLiteral("application/gzip"),
        QStringLiteral("application/x-bzip"),
        QStringLiteral("application/x-bzip2"),
        QStringLiteral("application/x-xz"),
        QStringLiteral("application/zstd"),
        QStringLiteral("application/x-lzma"),
        QStringLiteral("application/x-lzip"),
        QStringLiteral("application/x-lz4"),
        QStringLiteral("application/x-compress"),
        QStringLiteral("application/x-7z-compressed-tar"),
        QStringLiteral("application/x-zip-compressed"),
        QStringLiteral("application/x-rar-compressed"),
    };
    return types;
}
}

QString CreateOptions::extension() const
{
    switch (format) {
    case Format::Zip:
        return QStringLiteral(".zip");
    case Format::SevenZip:
        return QStringLiteral(".7z");
    case Format::Tar:
        switch (tarFilter) {
        case TarFilter::None:
            return QStringLiteral(".tar");
        case TarFilter::Gzip:
            return QStringLiteral(".tar.gz");
        case TarFilter::Bzip2:
            return QStringLiteral(".tar.bz2");
        case TarFilter::Xz:
            return QStringLiteral(".tar.xz");
        case TarFilter::Zstd:
            return QStringLiteral(".tar.zst");
        }
    }
    return QStringLiteral(".zip");
}

int CreateOptions::compressionLevel() const
{
    if (format == Format::Tar && tarFilter == TarFilter::None) {
        return 0;
    }
    return int(level);
}

QStringList methodsFor(Format format)
{
    switch (format) {
    case Format::Zip:
        return {QStringLiteral("Deflate"), QStringLiteral("BZip2"), QStringLiteral("LZMA"), QStringLiteral("Zstd"), QStringLiteral("XZ")};
    case Format::SevenZip:
        return {QStringLiteral("LZMA2"), QStringLiteral("LZMA"), QStringLiteral("PPMd"), QStringLiteral("BZip2"), QStringLiteral("Deflate")};
    case Format::Tar:
        return {};
    }
    return {};
}

QString detectFormat(const QString &path)
{
    try {
        const raye::archive::Detection d = raye::archive::detect(std::filesystem::path(QFile::encodeName(path).toStdString()));
        if (d.format == raye::archive::Format::Unknown) {
            return QString();
        }
        return QString::fromLatin1(raye::archive::format_name(d.format));
    } catch (...) {
        return QString();
    }
}

bool isBrowsableMimeType(const QString &mimeType)
{
    return !mimeType.isEmpty() && browsable().contains(mimeType);
}

QStringList browsableMimeTypes()
{
    return browsable();
}

bool isWritableMimeType(const QString &mimeType)
{
    return mimeType == QLatin1String("application/zip") || mimeType == QLatin1String("application/x-7z-compressed")
        || mimeType == QLatin1String("application/x-tar");
}

QString formatName(const QString &fileName)
{
    const Known *k = knownFor(fileName);
    return k ? QString::fromLatin1(k->format) : QString();
}

QString cannotWriteMessage(const QString &fileName)
{
    const QString format = formatName(fileName);
    if (format.isEmpty()) {
        return i18nc("@info", "Files can't add to this archive.");
    }
    return i18nc("@info %1 is a format name: RAR, 7z, ISO", "Files can't add to %1 archives.", format);
}

QString archiveExtension(const QString &fileName)
{
    const Known *k = knownFor(fileName);
    return k ? fileName.right(int(qstrlen(k->extension))) : QString();
}

QString baseNameOf(const QString &archiveFileName)
{
    const QString extension = archiveExtension(archiveFileName);
    if (extension.isEmpty() || extension.size() >= archiveFileName.size()) {
        return archiveFileName;
    }
    return archiveFileName.chopped(extension.size());
}

QString archiveBaseNameFor(const QString &firstItemName, bool isDir)
{
    if (isDir) {
        return firstItemName;
    }
    const QString archiveExt = archiveExtension(firstItemName);
    if (!archiveExt.isEmpty() && archiveExt.size() < firstItemName.size()) {
        return firstItemName.chopped(archiveExt.size());
    }
    const int dot = firstItemName.lastIndexOf(QLatin1Char('.'));
    // ".bashrc" has no extension, it is all name
    return dot > 0 ? firstItemName.left(dot) : firstItemName;
}

QString uniqueFileName(const QString &folder, const QString &fileName)
{
    auto taken = [&folder](const QString &name) {
        struct stat st;
        // lstat: a dangling link holds the name too
        return ::lstat(QFile::encodeName(QDir(folder).filePath(name)).constData(), &st) == 0;
    };
    if (!taken(fileName)) {
        return fileName;
    }
    QString extension = archiveExtension(fileName);
    if (extension.isEmpty() || extension.size() >= fileName.size()) {
        const int dot = fileName.lastIndexOf(QLatin1Char('.'));
        extension = dot > 0 ? fileName.mid(dot) : QString();
    }
    const QString base = fileName.chopped(extension.size());
    for (int n = 2; n < 10000; ++n) {
        const QString candidate = QStringLiteral("%1 (%2)%3").arg(base).arg(n).arg(extension);
        if (!taken(candidate)) {
            return candidate;
        }
    }
    return fileName;
}

bool isValidFileName(const QString &name)
{
    return !name.isEmpty() && name != QLatin1String(".") && name != QLatin1String("..") && !name.contains(QLatin1Char('/'))
        && !name.contains(QChar(0)) && name.toUtf8().size() <= 255;
}

}
