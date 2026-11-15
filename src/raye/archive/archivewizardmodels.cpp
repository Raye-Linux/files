/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archivewizardmodels.h"

#include <KLocalizedString>
#include <limits>
#include <array>
#include <QThread>

#include <unistd.h>
#include <QRegularExpression>
#include <QDirIterator>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KFormat>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QQmlEngine>

namespace Raye::Archive
{

namespace
{
QString expandPath(const QString &text, const QString &base)
{
    QString path = text.trimmed();
    if (path.isEmpty()) {
        return QString();
    }
    if (path == QLatin1String("~") || path.startsWith(QLatin1String("~/"))) {
        path = QDir::homePath() + path.mid(1);
    }
    if (!QDir::isAbsolutePath(path)) {
        path = base + QLatin1Char('/') + path;
    }
    return QDir::cleanPath(path);
}

// The deepest part of path that exists.
QFileInfo nearestExisting(const QString &path)
{
    QString current = path;
    while (!current.isEmpty()) {
        const QFileInfo info(current);
        if (info.exists()) {
            return info;
        }
        const QString parent = info.absolutePath();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return QFileInfo();
}
}

// ---- ExtractAllModel --------------------------------------------------------

ExtractAllModel::ExtractAllModel(QObject *parent)
    : QObject(parent)
{
}

QString ExtractAllModel::defaultDestination(const QString &archivePath)
{
    const QFileInfo info(archivePath);
    const QString folder = info.absolutePath();
    QString name = baseNameOf(info.fileName());
    if (name == info.fileName()) {
        // no archive extension: "backup" or a renamed "photo.jpg"
        const int dot = name.lastIndexOf(QLatin1Char('.'));
        name = dot > 0 ? name.left(dot) : i18nc("@item folder an archive without extension goes into", "%1 (extracted)", name);
    }
    // never the archive itself or another file
    if (QFileInfo::exists(folder + QLatin1Char('/') + name) && !QFileInfo(folder + QLatin1Char('/') + name).isDir()) {
        name = uniqueFileName(folder, name);
    }
    return folder + QLatin1Char('/') + name;
}

void ExtractAllModel::setArchivePath(const QString &path)
{
    if (path == m_archive) {
        return;
    }
    m_archive = path;
    Q_EMIT archivePathChanged();
    setDestination(defaultDestination(path));
}

QString ExtractAllModel::archiveName() const
{
    return QFileInfo(m_archive).fileName();
}

void ExtractAllModel::setDestination(const QString &destination)
{
    if (destination == m_destination) {
        return;
    }
    m_destination = destination;
    Q_EMIT destinationChanged();
    setError(QString());
}

void ExtractAllModel::setShowWhenDone(bool show)
{
    if (show != m_show) {
        m_show = show;
        Q_EMIT showWhenDoneChanged();
    }
}

void ExtractAllModel::setError(const QString &error)
{
    if (error != m_error) {
        m_error = error;
        Q_EMIT errorTextChanged();
    }
}

QString ExtractAllModel::resolvedDestination() const
{
    return expandPath(m_destination, QFileInfo(m_archive).absolutePath());
}

bool ExtractAllModel::validate()
{
    const QString path = resolvedDestination();
    if (path.isEmpty()) {
        setError(i18nc("@info", "Enter a folder to extract the files to."));
        return false;
    }
    const QFileInfo existing = nearestExisting(path);
    if (!existing.exists()) {
        setError(i18nc("@info", "This folder can't be used. Choose another one."));
        return false;
    }
    if (!existing.isDir()) {
        setError(i18nc("@info", "There's already a file with this name. Choose another folder."));
        return false;
    }
    if (!existing.isWritable()) {
        setError(i18nc("@info", "You don't have permission to extract to this folder."));
        return false;
    }
    setError(QString());
    return true;
}

// ---- CompressOptionsModel ---------------------------------------------------

namespace
{
constexpr std::array<Level, 6> levels{Level::Store, Level::Fastest, Level::Fast, Level::Normal, Level::Maximum, Level::Ultra};
constexpr std::array<qint64, 11> dictionarySizes{0, 64 << 10, 1 << 20, 4 << 20, 16 << 20, 32 << 20, 64 << 20, 128 << 20, 256 << 20, 512 << 20, qint64(1) << 30};
constexpr std::array<int, 8> wordSizeValues{0, 8, 16, 32, 64, 128, 192, 273};
constexpr qint64 mega = 1000 * 1000;
// 0 auto, non-solid, 16 MB ... 4 GB, one block
constexpr std::array<qint64, 8> solidSizes{0, -1, 16 << 20, 64 << 20, 256 << 20, qint64(1) << 30, qint64(4) << 30, std::numeric_limits<qint64>::max()};

QString sizeText(qint64 bytes)
{
    // like the rest of Files; a dictionary of 32 MiB reads "32 MB", as in 7-Zip
    const bool whole = bytes >= 1024 * 1024 && bytes % (1024 * 1024) == 0;
    return KFormat().formatByteSize(double(bytes), whole ? 0 : 1, KFormat::JEDECBinaryDialect);
}

// The dictionary the engine picks for a level (7z and ZIP LZMA, xz).
qint64 levelDictionary(Level level)
{
    switch (level) {
    case Level::Store:
        return 0;
    case Level::Fastest:
        return 1 << 20;
    case Level::Fast:
        return 4 << 20;
    case Level::Normal:
        return 16 << 20;
    case Level::Maximum:
        return 32 << 20;
    case Level::Ultra:
        return 64 << 20;
    }
    return 16 << 20;
}

void countSources(const QStringList &sources, qint64 *bytes, int *files)
{
    *bytes = 0;
    *files = 0;
    for (const QString &source : sources) {
        const QFileInfo info(source);
        if (info.isDir() && !info.isSymLink()) {
            QDirIterator it(source, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext() && *files < 200000) {
                it.next();
                *bytes += it.fileInfo().size();
                ++*files;
            }
        } else {
            *bytes += info.size();
            ++*files;
        }
    }
}
}

CompressOptionsModel::CompressOptionsModel(QObject *parent)
    : QObject(parent)
{
}

void CompressOptionsModel::touch()
{
    m_error.clear();
    Q_EMIT changed();
}

void CompressOptionsModel::setSources(const QStringList &sources)
{
    m_sources = sources;
    if (!sources.isEmpty()) {
        const QFileInfo first(sources.first());
        m_folder = first.absolutePath();
        const QString base = archiveBaseNameFor(first.fileName(), first.isDir());
        m_name = baseNameOf(uniqueFileName(m_folder, base + extension()));
    }
    countSources(sources, &m_sourceBytes, &m_sourceFiles);
    touch();
}

void CompressOptionsModel::setFormat(int format)
{
    const Format f = Format(qBound(0, format, 2));
    if (f == m_options.format) {
        return;
    }
    m_options.format = f;
    m_options.method.clear();
    // TAR has no Store level: its "no compression" is the None filter. A
    // TAR picked here is a tar.zst, quick and small.
    if (f == Format::Tar) {
        if (m_options.level == Level::Store) {
            m_options.level = Level::Normal;
        }
        if (m_options.tarFilter == TarFilter::None) {
            m_options.tarFilter = TarFilter::Zstd;
        }
        m_options.password.clear();
        m_passwordAgain.clear();
    }
    if (f != Format::SevenZip) {
        m_options.encryptNames = false;
    }
    touch();
}

void CompressOptionsModel::setLevel(int level)
{
    // snapped to the nearest named level
    const int v = qBound(0, level, 9);
    Level best = Level::Store;
    for (Level l : levels) {
        if (qAbs(int(l) - v) < qAbs(int(best) - v)) {
            best = l;
        }
    }
    if (best != m_options.level) {
        m_options.level = best;
        touch();
    }
}

int CompressOptionsModel::levelIndex() const
{
    for (int i = 0; i < int(levels.size()); ++i) {
        if (levels[size_t(i)] == m_options.level) {
            return i;
        }
    }
    return 3;
}

void CompressOptionsModel::setLevelIndex(int index)
{
    setLevel(int(levels[size_t(qBound(0, index, int(levels.size()) - 1))]));
}

void CompressOptionsModel::setTarFilter(int filter)
{
    const TarFilter t = TarFilter(qBound(0, filter, 4));
    if (t != m_options.tarFilter) {
        m_options.tarFilter = t;
        touch();
    }
}

void CompressOptionsModel::setKeepSymlinks(bool keep)
{
    if (keep != m_options.keepSymlinks) {
        m_options.keepSymlinks = keep;
        Q_EMIT changed();
    }
}

void CompressOptionsModel::setName(const QString &name)
{
    if (name != m_name) {
        m_name = name;
        touch();
    }
}

void CompressOptionsModel::setFolder(const QString &folder)
{
    if (folder != m_folder) {
        m_folder = folder;
        touch();
    }
}

void CompressOptionsModel::setPassword(const QString &password)
{
    if (password != m_options.password) {
        m_options.password = password;
        touch();
    }
}

void CompressOptionsModel::setPasswordAgain(const QString &password)
{
    if (password != m_passwordAgain) {
        m_passwordAgain = password;
        touch();
    }
}

void CompressOptionsModel::setEncryptNames(bool encrypt)
{
    encrypt = encrypt && canEncryptNames();
    if (encrypt != m_options.encryptNames) {
        m_options.encryptNames = encrypt;
        Q_EMIT changed();
    }
}

void CompressOptionsModel::setZipEncryption(int index)
{
    const bool zipCrypto = index == 1;
    if (zipCrypto != m_options.zipCrypto) {
        m_options.zipCrypto = zipCrypto;
        Q_EMIT changed();
    }
}

QStringList CompressOptionsModel::methods() const
{
    if (isTar()) {
        return {QStringLiteral("zstd"), QStringLiteral("xz"), QStringLiteral("gzip"), QStringLiteral("bzip2"), i18nc("@item no compression", "None")};
    }
    return methodsFor(m_options.format);
}

int CompressOptionsModel::methodIndex() const
{
    if (isTar()) {
        static constexpr std::array<TarFilter, 5> order{TarFilter::Zstd, TarFilter::Xz, TarFilter::Gzip, TarFilter::Bzip2, TarFilter::None};
        for (int i = 0; i < 5; ++i) {
            if (order[size_t(i)] == m_options.tarFilter) {
                return i;
            }
        }
        return 0;
    }
    return qMax(0, methods().indexOf(m_options.method));
}

void CompressOptionsModel::setMethodIndex(int index)
{
    if (isTar()) {
        static constexpr std::array<TarFilter, 5> order{TarFilter::Zstd, TarFilter::Xz, TarFilter::Gzip, TarFilter::Bzip2, TarFilter::None};
        setTarFilter(int(order[size_t(qBound(0, index, 4))]));
        return;
    }
    const QStringList list = methods();
    const QString method = index <= 0 || index >= list.size() ? QString() : list.at(index);
    if (method != m_options.method) {
        m_options.method = method;
        touch();
    }
}

bool CompressOptionsModel::dictionaryEnabled() const
{
    if (!levelEnabled() || m_options.level == Level::Store) {
        return false;
    }
    if (isTar()) {
        return m_options.tarFilter == TarFilter::Xz || m_options.tarFilter == TarFilter::Zstd;
    }
    const QString method = m_options.method.isEmpty() ? methods().value(0) : m_options.method;
    return method != QLatin1String("Deflate") && method != QLatin1String("BZip2");
}

QStringList CompressOptionsModel::dictionaries() const
{
    QStringList list{i18nc("@item dictionary size picked by the level", "Automatic (%1)", sizeText(levelDictionary(m_options.level)))};
    for (size_t i = 1; i < dictionarySizes.size(); ++i) {
        list.append(sizeText(dictionarySizes[i]));
    }
    return list;
}

int CompressOptionsModel::dictionaryIndex() const
{
    for (int i = 0; i < int(dictionarySizes.size()); ++i) {
        if (dictionarySizes[size_t(i)] == m_options.dictionary) {
            return i;
        }
    }
    return 0;
}

void CompressOptionsModel::setDictionaryIndex(int index)
{
    const qint64 v = dictionarySizes[size_t(qBound(0, index, int(dictionarySizes.size()) - 1))];
    if (v != m_options.dictionary) {
        m_options.dictionary = v;
        touch();
    }
}

qint64 CompressOptionsModel::effectiveDictionary() const
{
    return m_options.dictionary ? m_options.dictionary : levelDictionary(m_options.level);
}

QStringList CompressOptionsModel::wordSizes() const
{
    QStringList list{i18nc("@item picked by the level", "Automatic")};
    for (size_t i = 1; i < wordSizeValues.size(); ++i) {
        list.append(QString::number(wordSizeValues[i]));
    }
    return list;
}

int CompressOptionsModel::wordIndex() const
{
    return qMax(0, int(std::find(wordSizeValues.begin(), wordSizeValues.end(), m_options.wordSize) - wordSizeValues.begin()) % int(wordSizeValues.size()));
}

void CompressOptionsModel::setWordIndex(int index)
{
    const int v = wordSizeValues[size_t(qBound(0, index, int(wordSizeValues.size()) - 1))];
    if (v != m_options.wordSize) {
        m_options.wordSize = v;
        touch();
    }
}

QStringList CompressOptionsModel::solidBlocks() const
{
    QStringList list{i18nc("@item picked by the level", "Automatic"), i18nc("@item solid block size", "Not solid")};
    for (size_t i = 2; i + 1 < solidSizes.size(); ++i) {
        list.append(sizeText(solidSizes[i]));
    }
    list.append(i18nc("@item solid block size", "One block"));
    return list;
}

int CompressOptionsModel::solidIndex() const
{
    if (!m_options.solid) {
        return 1;
    }
    for (int i = 0; i < int(solidSizes.size()); ++i) {
        if (solidSizes[size_t(i)] == m_options.solidBlockSize) {
            return i;
        }
    }
    return 0;
}

void CompressOptionsModel::setSolidIndex(int index)
{
    index = qBound(0, index, int(solidSizes.size()) - 1);
    m_options.solid = index != 1;
    m_options.solidBlockSize = index == 1 ? 0 : solidSizes[size_t(index)];
    touch();
}

QStringList CompressOptionsModel::threadChoices() const
{
    QStringList list{i18nc("@item one per processor", "Automatic (%1)", QThread::idealThreadCount())};
    for (int i = 1; i <= QThread::idealThreadCount(); ++i) {
        list.append(QString::number(i));
    }
    return list;
}

void CompressOptionsModel::setThreadIndex(int index)
{
    index = qBound(0, index, QThread::idealThreadCount());
    if (index != m_options.threads) {
        m_options.threads = index;
        touch();
    }
}

void CompressOptionsModel::setVolumeIndex(int index)
{
    index = qBound(0, index, 4);
    if (index != m_volumeIndex) {
        m_volumeIndex = index;
        touch();
    }
}

void CompressOptionsModel::setCustomVolume(const QString &text)
{
    if (text != m_customVolume) {
        m_customVolume = text;
        touch();
    }
}

void CompressOptionsModel::setUpdateMode(int mode)
{
    const UpdateMode m = UpdateMode(qBound(0, mode, 3));
    if (m != m_options.mode) {
        m_options.mode = m;
        touch();
    }
}

void CompressOptionsModel::setDeleteAfter(bool on)
{
    if (on != m_options.deleteAfter) {
        m_options.deleteAfter = on;
        Q_EMIT changed();
    }
}

void CompressOptionsModel::setStoreAccessed(bool on)
{
    if (on != m_options.storeAccessed) {
        m_options.storeAccessed = on;
        Q_EMIT changed();
    }
}

void CompressOptionsModel::setStoreCreated(bool on)
{
    if (on != m_options.storeCreated) {
        m_options.storeCreated = on;
        Q_EMIT changed();
    }
}

void CompressOptionsModel::setExcludes(const QString &patterns)
{
    if (patterns != m_excludes) {
        m_excludes = patterns;
        Q_EMIT changed();
    }
}

qint64 CompressOptionsModel::parseSize(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("^\\s*([0-9]+(?:[.,][0-9]+)?)\\s*([kmgt]?)(?:i?b|o)?\\s*$"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch()) {
        return 0;
    }
    double v = m.captured(1).replace(QLatin1Char(','), QLatin1Char('.')).toDouble();
    const QString u = m.captured(2).toLower();
    const QChar unit = u.isEmpty() ? QChar() : u.at(0);
    // "100 MB" is what a disc or a mail limit means: decimal, like 7-Zip.
    const double scale = unit == QLatin1Char('k') ? 1e3 : unit == QLatin1Char('m') ? 1e6 : unit == QLatin1Char('g') ? 1e9 : unit == QLatin1Char('t') ? 1e12 : 1e6;
    v *= scale;
    return v >= 64 * 1024 && v < 9e18 ? qint64(v) : 0;
}

namespace
{
// How the 7z writer runs LZMA2: pieces of a few MB on every core, each with
// no more dictionary than it can reach, and never more than half the free
// memory in all.
struct Lzma2Plan {
    qint64 dictionary = 0;
    qint64 perThread = 0;
    int threads = 1;
};

Lzma2Plan planLzma2(int level, qint64 dictionary, qint64 bytes, int threads)
{
    static constexpr qint64 overlaps[10] = {0, 0, 0, 1 << 20, 2 << 20, 2 << 20, 4 << 20, 4 << 20, 4 << 20, 4 << 20};
    static constexpr qint64 floors[10] = {1 << 20, 1 << 20, 1 << 20, 1 << 20, 2 << 20, 2 << 20, 4 << 20, 4 << 20, 4 << 20, 4 << 20};
    level = qBound(0, level, 9);
    const qint64 block = qBound(qint64(1 << 20), dictionary * 4, qint64(256 << 20));
    const qint64 dict = bytes > 0 ? qMin(dictionary, qMax<qint64>(bytes, 4096)) : dictionary;
    const qint64 overlap = qMin(overlaps[level], dict);
    const qint64 floor = floors[level];
    const qint64 chunk = qBound(floor, bytes / qMax(1, threads) + 1, qMax(floor, block));
    Lzma2Plan p;
    p.dictionary = qMax<qint64>(4096, qMin(dict, overlap + chunk));
    p.perThread = p.dictionary * 12 + chunk * 2 + overlap;
    const qint64 budget = qint64(::sysconf(_SC_AVPHYS_PAGES)) * ::sysconf(_SC_PAGESIZE) / 2;
    p.threads = threads;
    if (budget > 0) {
        p.threads = int(qBound(qint64(1), budget / p.perThread, qint64(threads)));
    }
    if (bytes > 0) {
        p.threads = int(qMin<qint64>(p.threads, (bytes + chunk - 1) / chunk));
    }
    p.threads = qMax(1, p.threads);
    return p;
}
}

bool CompressOptionsModel::usesLzma2() const
{
    return m_options.format == Format::SevenZip && (m_options.method.isEmpty() || m_options.method == QLatin1String("LZMA2"))
        && m_options.level != Level::Store;
}

qint64 CompressOptionsModel::compressMemory() const
{
    const int threads = m_options.threads > 0 ? m_options.threads : QThread::idealThreadCount();
    if (m_options.level == Level::Store || !levelEnabled()) {
        return 8 * 1024 * 1024;
    }
    if (usesLzma2()) {
        const Lzma2Plan p = planLzma2(m_options.compressionLevel(), effectiveDictionary(), m_sourceBytes, threads);
        return p.perThread * p.threads + 8 * 1024 * 1024;
    }
    if (m_options.format == Format::Zip && dictionaryEnabled() == false) {
        return qint64(threads) * 4 * 1024 * 1024;
    }
    qint64 window = effectiveDictionary();
    if (m_sourceBytes > 0) {
        window = qMin(window, qMax<qint64>(m_sourceBytes, 4096));
    }
    if (isTar()) {
        switch (m_options.tarFilter) {
        case TarFilter::None:
            return 8 * 1024 * 1024;
        case TarFilter::Gzip:
            return qint64(threads) * 1024 * 1024 + 8 * 1024 * 1024;
        case TarFilter::Bzip2:
            return qint64(threads) * 9 * 1024 * 1024;
        case TarFilter::Zstd:
            return qint64(threads) * (window * 3 + 2 * 1024 * 1024);
        case TarFilter::Xz:
            break;
        }
    }
    // LZMA holds about 11 times its dictionary per stream.
    qint64 dict = effectiveDictionary();
    if (m_sourceBytes > 0) {
        dict = qMin(dict, qMax<qint64>(m_sourceBytes, 4096));
    }
    return (dict * 23 / 2 + 6 * 1024 * 1024) * qMax(1, threads / 2);
}

qint64 CompressOptionsModel::openMemory() const
{
    if (!dictionaryEnabled()) {
        return 2 * 1024 * 1024;
    }
    if (usesLzma2()) {
        const int threads = m_options.threads > 0 ? m_options.threads : QThread::idealThreadCount();
        return planLzma2(m_options.compressionLevel(), effectiveDictionary(), m_sourceBytes, threads).dictionary + 2 * 1024 * 1024;
    }
    qint64 dict = effectiveDictionary();
    if (m_sourceBytes > 0) {
        dict = qMin(dict, qMax<qint64>(m_sourceBytes, 4096));
    }
    return dict + 2 * 1024 * 1024;
}

QString CompressOptionsModel::estimate() const
{
    const QString what = i18ncp("@info %2 is a size", "%2 in %1 file", "%2 in %1 files", m_sourceFiles, sizeText(m_sourceBytes));
    return i18nc("@info %1 is what is compressed, %2 and %3 sizes",
                 "%1 · about %2 of memory to compress, %3 to open",
                 what,
                 sizeText(compressMemory()),
                 sizeText(openMemory()));
}

QStringList CompressOptionsModel::profiles() const
{
    return KSharedConfig::openConfig(QStringLiteral("dolphinrc"))->group(QStringLiteral("ArchiveProfiles")).groupList();
}

void CompressOptionsModel::saveProfile(const QString &name)
{
    const QString clean = name.trimmed();
    if (clean.isEmpty()) {
        return;
    }
    KConfigGroup g = KSharedConfig::openConfig(QStringLiteral("dolphinrc"))->group(QStringLiteral("ArchiveProfiles")).group(clean);
    g.writeEntry("Format", format());
    g.writeEntry("Level", level());
    g.writeEntry("TarFilter", tarFilter());
    g.writeEntry("Method", m_options.method);
    g.writeEntry("Dictionary", m_options.dictionary);
    g.writeEntry("WordSize", m_options.wordSize);
    g.writeEntry("Solid", solidIndex());
    g.writeEntry("Threads", m_options.threads);
    g.writeEntry("Volume", m_volumeIndex);
    g.writeEntry("CustomVolume", m_customVolume);
    g.writeEntry("EncryptNames", m_options.encryptNames);
    g.writeEntry("ZipCrypto", m_options.zipCrypto);
    g.writeEntry("Mode", updateMode());
    g.writeEntry("DeleteAfter", m_options.deleteAfter);
    g.writeEntry("StoreAccessed", m_options.storeAccessed);
    g.writeEntry("StoreCreated", m_options.storeCreated);
    g.writeEntry("KeepSymlinks", m_options.keepSymlinks);
    g.writeEntry("Excludes", m_excludes);
    g.sync();
    Q_EMIT profilesChanged();
}

void CompressOptionsModel::loadProfile(const QString &name)
{
    const KConfigGroup g = KSharedConfig::openConfig(QStringLiteral("dolphinrc"))->group(QStringLiteral("ArchiveProfiles")).group(name);
    if (!g.exists()) {
        return;
    }
    QSignalBlocker blocker(this);
    setFormat(g.readEntry("Format", 0));
    setLevel(g.readEntry("Level", int(Level::Normal)));
    setTarFilter(g.readEntry("TarFilter", int(TarFilter::Zstd)));
    m_options.method = g.readEntry("Method", QString());
    m_options.dictionary = g.readEntry("Dictionary", qint64(0));
    m_options.wordSize = g.readEntry("WordSize", 0);
    setSolidIndex(g.readEntry("Solid", 0));
    setThreadIndex(g.readEntry("Threads", 0));
    setVolumeIndex(g.readEntry("Volume", 0));
    m_customVolume = g.readEntry("CustomVolume", QString());
    setEncryptNames(g.readEntry("EncryptNames", false));
    m_options.zipCrypto = g.readEntry("ZipCrypto", false);
    setUpdateMode(g.readEntry("Mode", 0));
    m_options.deleteAfter = g.readEntry("DeleteAfter", false);
    m_options.storeAccessed = g.readEntry("StoreAccessed", false);
    m_options.storeCreated = g.readEntry("StoreCreated", false);
    m_options.keepSymlinks = g.readEntry("KeepSymlinks", false);
    m_excludes = g.readEntry("Excludes", QString());
    blocker.unblock();
    touch();
}

void CompressOptionsModel::removeProfile(const QString &name)
{
    KSharedConfig::openConfig(QStringLiteral("dolphinrc"))->group(QStringLiteral("ArchiveProfiles")).deleteGroup(name);
    KSharedConfig::openConfig(QStringLiteral("dolphinrc"))->sync();
    Q_EMIT profilesChanged();
}

CreateOptions CompressOptionsModel::options() const
{
    CreateOptions o = m_options;
    switch (m_volumeIndex) {
    case 1:
        o.volumeSize = 100 * mega;
        break;
    case 2:
        o.volumeSize = 700 * mega;
        break;
    case 3:
        // the largest file FAT32 holds
        o.volumeSize = qint64(4) * 1024 * 1024 * 1024 - 1;
        break;
    case 4:
        o.volumeSize = parseSize(m_customVolume);
        break;
    default:
        o.volumeSize = 0;
    }
    o.excludes.clear();
    for (const QString &p : m_excludes.split(QRegularExpression(QStringLiteral("[;\\n]")), Qt::SkipEmptyParts)) {
        if (!p.trimmed().isEmpty()) {
            o.excludes.append(p.trimmed());
        }
    }
    if (isTar()) {
        o.password.clear();
    }
    return o;
}

bool CompressOptionsModel::levelEnabled() const
{
    return !(m_options.format == Format::Tar && m_options.tarFilter == TarFilter::None);
}

QString CompressOptionsModel::archivePath() const
{
    const QString base = m_sources.isEmpty() ? QDir::homePath() : QFileInfo(m_sources.first()).absolutePath();
    return expandPath(m_folder, base) + QLatin1Char('/') + m_name.trimmed() + extension();
}

bool CompressOptionsModel::validate()
{
    QString error;
    const QString name = m_name.trimmed();
    const QString base = m_sources.isEmpty() ? QDir::homePath() : QFileInfo(m_sources.first()).absolutePath();
    const QFileInfo folder(expandPath(m_folder, base));
    if (name.isEmpty()) {
        error = i18nc("@info", "Enter a name for the archive.");
    } else if (!isValidFileName(name + extension())) {
        error = i18nc("@info", "This name can't be used. Choose another one.");
    } else if (!folder.isDir()) {
        error = i18nc("@info", "This folder doesn't exist.");
    } else if (!folder.isWritable()) {
        error = i18nc("@info", "You don't have permission to save in this folder.");
    } else if (m_options.mode == UpdateMode::Create && (QFileInfo::exists(archivePath()) || QFileInfo(archivePath()).isSymLink())) {
        error = i18nc("@info", "There's already a file with this name.");
    } else if (!m_options.password.isEmpty() && m_options.password != m_passwordAgain) {
        error = i18nc("@info", "The passwords don't match.");
    } else if (m_volumeIndex == 4 && parseSize(m_customVolume) == 0) {
        error = i18nc("@info", "Enter a part size like 250 MB.");
    } else if (m_options.mode != UpdateMode::Create && m_volumeIndex != 0) {
        error = i18nc("@info", "An archive split into parts can't be updated. Create a new one.");
    }
    if (error != m_error) {
        m_error = error;
        Q_EMIT changed();
    }
    return error.isEmpty();
}

void registerQmlTypes()
{
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    qmlRegisterType<ExtractAllModel>("org.raye.files.archive", 1, 0, "ExtractAllModel");
    qmlRegisterType<CompressOptionsModel>("org.raye.files.archive", 1, 0, "CompressOptionsModel");
}

}

static void rayeArchiveRegisterAtStartup()
{
    Raye::Archive::registerQmlTypes();
}
Q_COREAPP_STARTUP_FUNCTION(rayeArchiveRegisterAtStartup)
