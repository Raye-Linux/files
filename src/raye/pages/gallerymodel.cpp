/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gallerymodel.h"
#include "homemodel.h"

#include <KIO/Global>
#include <QProcess>

#include <KConfigGroup>
#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/SimpleExtractionResult>
#include <KSharedConfig>

#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QMimeDatabase>
#include <QLocale>
#include <QStandardPaths>
#include <QtConcurrentRun>

#include <algorithm>

namespace Raye::Pages
{
namespace
{
bool isPicture(const QString &suffix)
{
    static const QStringList suffixes = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("webp"), QStringLiteral("gif"),
        QStringLiteral("bmp"), QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("avif"), QStringLiteral("tif"),
        QStringLiteral("tiff"),
    };
    return suffixes.contains(suffix.toLower());
}

bool isVideo(const QString &suffix)
{
    static const QStringList suffixes = {
        QStringLiteral("mp4"), QStringLiteral("m4v"), QStringLiteral("mov"), QStringLiteral("mkv"), QStringLiteral("webm"),
        QStringLiteral("avi"), QStringLiteral("3gp"),
    };
    return suffixes.contains(suffix.toLower());
}

// Width over height, read from the header only; a picture turned by its
// EXIF tag counts as turned.
qreal pictureAspect(const QString &path)
{
    QImageReader reader(path);
    QSize size = reader.size();
    if (!size.isValid() || size.height() == 0) {
        return 1.5;
    }
    if (reader.transformation() & QImageIOHandler::TransformationRotate90) {
        size.transpose();
    }
    return qBound(0.25, qreal(size.width()) / size.height(), 4.0);
}

void readVideo(GalleryModel::Picture &p)
{
    static thread_local KFileMetaData::ExtractorCollection extractors;
    const QString path = p.url.toLocalFile();
    const QString mime = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchExtension).name();
    KFileMetaData::SimpleExtractionResult result(path, mime, KFileMetaData::ExtractionResult::ExtractMetaData);
    const QList<KFileMetaData::Extractor *> list = extractors.fetchExtractors(mime);
    for (KFileMetaData::Extractor *e : list) {
        e->extract(&result);
    }
    const auto props = result.properties();
    const int w = props.value(KFileMetaData::Property::Width).toInt();
    const int h = props.value(KFileMetaData::Property::Height).toInt();
    if (w > 0 && h > 0) {
        p.aspect = qBound(0.25, qreal(w) / h, 4.0);
    } else {
        p.aspect = 16.0 / 9.0;
    }
    const QVariant duration = props.value(KFileMetaData::Property::Duration);
    p.duration = duration.isValid() ? qRound(duration.toDouble()) : -1;
}
}

GalleryModel::GalleryModel(const QStringList &folders, QObject *parent)
    : QObject(parent)
    , m_fixedFolders(!folders.isEmpty())
    , m_folders(folders.isEmpty() ? configuredFolders() : folders)
{
    reload();
}

QStringList GalleryModel::configuredFolders()
{
    const KConfigGroup g(KSharedConfig::openConfig(QStringLiteral("dolphinrc")), QStringLiteral("Raye"));
    QStringList folders = g.readEntry("GalleryFolders", QStringList());
    folders.removeAll(QString());
    if (folders.isEmpty()) {
        folders << QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }
    return folders;
}

QList<GalleryModel::Picture> GalleryModel::scan(const QStringList &folders)
{
    QList<Picture> found;
    for (const QString &folder : folders) {
        QDirIterator it(folder, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo info = it.fileInfo();
            if (info.fileName().startsWith(QLatin1Char('.'))) {
                continue;
            }
            if (isPicture(info.suffix())) {
                found.append({QUrl::fromLocalFile(info.absoluteFilePath()), info.lastModified(), pictureAspect(info.absoluteFilePath()), false, -1});
            } else if (isVideo(info.suffix())) {
                Picture p{QUrl::fromLocalFile(info.absoluteFilePath()), info.lastModified(), 16.0 / 9.0, true, -1};
                readVideo(p);
                found.append(p);
            }
        }
    }
    std::sort(found.begin(), found.end(), [](const Picture &a, const Picture &b) {
        return a.date > b.date;
    });
    return found;
}

QVariantList GalleryModel::group(const QList<Picture> &pictures)
{
    QVariantList groups;
    QVariantList items;
    QString title;
    const QLocale locale;
    for (const Picture &p : pictures) {
        QString month = locale.standaloneMonthName(p.date.date().month(), QLocale::LongFormat) + QLatin1Char(' ') + QString::number(p.date.date().year());
        if (!month.isEmpty()) {
            month[0] = month[0].toUpper();
        }
        if (month != title && !items.isEmpty()) {
            groups.append(QVariantMap{{QStringLiteral("title"), title}, {QStringLiteral("items"), items}});
            items.clear();
        }
        title = month;
        items.append(QVariantMap{{QStringLiteral("url"), p.url},
                                 {QStringLiteral("name"), p.url.fileName()},
                                 {QStringLiteral("aspect"), p.aspect},
                                 {QStringLiteral("video"), p.video},
                                 {QStringLiteral("duration"), durationText(p.duration)}});
    }
    if (!items.isEmpty()) {
        groups.append(QVariantMap{{QStringLiteral("title"), title}, {QStringLiteral("items"), items}});
    }
    return groups;
}

void GalleryModel::reload()
{
    m_loading = true;
    Q_EMIT changed();
    auto *watcher = new QFutureWatcher<QList<Picture>>(this);
    connect(watcher, &QFutureWatcher<QList<Picture>>::finished, this, [this, watcher] {
        m_items = watcher->result();
        watcher->deleteLater();
        m_count = m_items.size();
        m_shown = qMin<int>(batchSize, m_items.size());
        m_loading = false;
        regroup();
    });
    watcher->setFuture(QtConcurrent::run(&GalleryModel::scan, m_folders));
}

void GalleryModel::regroup()
{
    m_groups = group(m_items.mid(0, m_shown));
    Q_EMIT changed();
}

void GalleryModel::loadMore()
{
    if (m_loading || !hasMore()) {
        return;
    }
    m_shown = qMin<int>(m_shown + batchSize, m_items.size());
    regroup();
}

QList<GalleryModel::Row> GalleryModel::justify(const QList<qreal> &aspects, qreal width, qreal targetHeight, qreal gap)
{
    QList<Row> rows;
    if (width <= 0 || targetHeight <= 0) {
        return rows;
    }
    int start = 0;
    qreal sum = 0;
    for (int i = 0; i < aspects.size(); ++i) {
        sum += aspects.at(i);
        const int count = i - start + 1;
        // Full once the row at the target height is as wide as the page.
        if (sum * targetHeight + gap * (count - 1) >= width) {
            rows.append({start, count, (width - gap * (count - 1)) / sum});
            start = i + 1;
            sum = 0;
        }
    }
    if (start < aspects.size()) {
        rows.append({start, int(aspects.size()) - start, targetHeight});
    }
    return rows;
}

QVariantList GalleryModel::rows(const QVariantList &aspects, qreal width, qreal targetHeight, qreal gap) const
{
    QList<qreal> list;
    list.reserve(aspects.size());
    for (const QVariant &a : aspects) {
        list.append(a.toReal());
    }
    QVariantList out;
    const QList<Row> rows = justify(list, width, targetHeight, gap);
    for (const Row &r : rows) {
        out.append(QVariantMap{{QStringLiteral("start"), r.start}, {QStringLiteral("count"), r.count}, {QStringLiteral("height"), r.height}});
    }
    return out;
}

QString GalleryModel::durationText(int seconds)
{
    if (seconds < 0) {
        return QString();
    }
    const int h = seconds / 3600;
    const int m = (seconds / 60) % 60;
    const int s = seconds % 60;
    const QString ss = QStringLiteral("%1").arg(s, 2, 10, QLatin1Char('0'));
    if (h > 0) {
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(ss);
    }
    return QStringLiteral("%1:%2").arg(m).arg(ss);
}

QVariantList GalleryModel::collection() const
{
    // Pictures and Videos are always offered; any other folder once chosen.
    QStringList offered{QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)};
    for (const QString &f : std::as_const(m_folders)) {
        if (!offered.contains(f)) {
            offered << f;
        }
    }
    QVariantList out;
    for (const QString &f : std::as_const(offered)) {
        out.append(QVariantMap{{QStringLiteral("path"), f}, {QStringLiteral("name"), QFileInfo(f).fileName()}, {QStringLiteral("included"), m_folders.contains(f)}});
    }
    return out;
}

void GalleryModel::setFolderIncluded(const QString &path, bool included)
{
    QStringList folders = configuredFolders();
    if (folders.contains(path) == included) {
        return;
    }
    if (included) {
        folders << path;
    } else {
        folders.removeAll(path);
        // Never empty: an empty list means Pictures.
        if (folders.isEmpty()) {
            return;
        }
    }
    KConfigGroup g(KSharedConfig::openConfig(QStringLiteral("dolphinrc")), QStringLiteral("Raye"));
    g.writeEntry("GalleryFolders", folders);
    g.sync();
    if (!m_fixedFolders) {
        m_folders = folders;
        reload();
    }
    Q_EMIT collectionChanged();
}

void GalleryModel::open(const QUrl &url)
{
    Q_EMIT openRequested(url);
}

void GalleryModel::openLocation(const QUrl &url)
{
    Q_EMIT openLocationRequested(KIO::upUrl(url), url);
}

void GalleryModel::properties(const QUrl &url)
{
    Q_EMIT propertiesRequested(url);
}

void GalleryModel::copyAsPath(const QUrl &url)
{
    copyPathToClipboard(url);
}

void GalleryModel::setAsBackground(const QUrl &url)
{
    if (url.isLocalFile()) {
        // An absolute path: it can never be read as an option.
        QProcess::startDetached(QStringLiteral("plasma-apply-wallpaperimage"), {url.toLocalFile()});
    }
}

void GalleryModel::addFolder()
{
    Q_EMIT addFolderRequested();
}
}
