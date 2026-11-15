/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Gallery: the pictures under the folders, newest first, one group a month.

#include "pages/gallerymodel.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using Raye::Pages::GalleryModel;

namespace
{
void touch(const QString &path, const QDateTime &when)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.flush();
    f.setFileTime(when, QFileDevice::FileModificationTime);
    f.close();
}
}

class GalleryModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        QStandardPaths::setTestModeEnabled(true);
    }

    void scanFindsPicturesNewestFirst()
    {
        QTemporaryDir dir;
        const QDateTime nov(QDate(2026, 11, 3), QTime(10, 0));
        const QDateTime oct(QDate(2026, 10, 20), QTime(10, 0));
        touch(dir.filePath(QStringLiteral("a.jpg")), oct);
        touch(dir.filePath(QStringLiteral("trip/b.PNG")), nov);
        touch(dir.filePath(QStringLiteral("notes.txt")), nov);
        touch(dir.filePath(QStringLiteral(".hidden.png")), nov);
        const auto pictures = GalleryModel::scan({dir.path()});
        QCOMPARE(pictures.size(), 2);
        QCOMPARE(pictures.at(0).url.fileName(), QStringLiteral("b.PNG"));
        QCOMPARE(pictures.at(1).url.fileName(), QStringLiteral("a.jpg"));
    }

    void groupsByMonth()
    {
        const QList<GalleryModel::Picture> pictures = {
            {QUrl::fromLocalFile(QStringLiteral("/p/1.jpg")), QDateTime(QDate(2026, 11, 3), QTime(9, 0)), 1.5, false, -1},
            {QUrl::fromLocalFile(QStringLiteral("/p/2.jpg")), QDateTime(QDate(2026, 11, 1), QTime(9, 0)), 1.5, false, -1},
            {QUrl::fromLocalFile(QStringLiteral("/p/3.jpg")), QDateTime(QDate(2025, 11, 30), QTime(9, 0)), 1.5, false, -1},
        };
        const QVariantList groups = GalleryModel::group(pictures);
        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups.at(0).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("November 2026"));
        QCOMPARE(groups.at(0).toMap().value(QStringLiteral("items")).toList().size(), 2);
        QCOMPARE(groups.at(1).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("November 2025"));
    }

    void modelLoadsInTheBackground()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("a.jpg")), QDateTime::currentDateTime());
        GalleryModel model({dir.path()});
        QVERIFY(model.loading());
        QTRY_VERIFY(!model.loading());
        QCOMPARE(model.count(), 1);
    }

    void videosToo()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("clip.mp4")), QDateTime(QDate(2026, 3, 1), QTime(9, 0)));
        touch(dir.filePath(QStringLiteral("a.jpg")), QDateTime(QDate(2026, 2, 1), QTime(9, 0)));
        const auto items = GalleryModel::scan({dir.path()});
        QCOMPARE(items.size(), 2);
        QVERIFY(items.at(0).video);
        QVERIFY(!items.at(1).video);
    }

    void noFixedCeiling()
    {
        QTemporaryDir dir;
        for (int i = 0; i < 1000; ++i) {
            touch(dir.filePath(QStringLiteral("%1.jpg").arg(i)), QDateTime(QDate(2026, 1, 1), QTime(0, 0)).addSecs(i));
        }
        QCOMPARE(GalleryModel::scan({dir.path()}).size(), 1000);
    }

    void loadsMoreAsYouScroll()
    {
        QTemporaryDir dir;
        for (int i = 0; i < 300; ++i) {
            touch(dir.filePath(QStringLiteral("%1.jpg").arg(i)), QDateTime(QDate(2026, 1, 1), QTime(0, 0)).addSecs(i));
        }
        GalleryModel model({dir.path()});
        QTRY_VERIFY(!model.loading());
        QCOMPARE(model.count(), 300);
        QCOMPARE(model.shown(), GalleryModel::batchSize);
        QVERIFY(model.hasMore());
        model.loadMore();
        QCOMPARE(model.shown(), 2 * GalleryModel::batchSize);
        model.loadMore();
        model.loadMore();
        QCOMPARE(model.shown(), 300);
        QVERIFY(!model.hasMore());
        int items = 0;
        for (const QVariant &g : model.groups()) {
            items += g.toMap().value(QStringLiteral("items")).toList().size();
        }
        QCOMPARE(items, 300);
    }

    void justifiedRowsFillTheWidth()
    {
        const QList<qreal> aspects = {1.5, 1.0, 0.75, 1.5, 1.0, 1.0, 1.33, 0.66, 2.0};
        const qreal width = 600, gap = 4;
        const auto rows = GalleryModel::justify(aspects, width, 180, gap);
        QVERIFY(rows.size() >= 2);
        int used = 0;
        for (int r = 0; r < rows.size(); ++r) {
            const auto &row = rows.at(r);
            QCOMPARE(row.start, used);
            used += row.count;
            qreal sum = 0;
            for (int i = row.start; i < row.start + row.count; ++i) {
                sum += aspects.at(i) * row.height;
            }
            sum += gap * (row.count - 1);
            if (r + 1 < rows.size()) {
                QVERIFY2(qAbs(sum - width) < 1, qPrintable(QString::number(sum)));
                QVERIFY(row.height > 90 && row.height < 360);
            } else {
                // the last row keeps the usual height, left-aligned
                QCOMPARE(row.height, 180.0);
                QVERIFY(sum <= width);
            }
        }
        QCOMPARE(used, aspects.size());
    }

    void durations()
    {
        QCOMPARE(GalleryModel::durationText(42), QStringLiteral("0:42"));
        QCOMPARE(GalleryModel::durationText(222), QStringLiteral("3:42"));
        QCOMPARE(GalleryModel::durationText(3725), QStringLiteral("1:02:05"));
        QCOMPARE(GalleryModel::durationText(-1), QString());
    }

    void collectionFolders()
    {
        QTemporaryDir dir;
        const QString extra = dir.filePath(QStringLiteral("Phone"));
        QDir().mkpath(extra);
        GalleryModel model({dir.path()});
        QTRY_VERIFY(!model.loading());
        model.setFolderIncluded(extra, true);
        QVERIFY(GalleryModel::configuredFolders().contains(extra));
        model.setFolderIncluded(extra, false);
        QVERIFY(!GalleryModel::configuredFolders().contains(extra));
    }
};

QTEST_GUILESS_MAIN(GalleryModelTest)

#include "test_gallerymodel.moc"
