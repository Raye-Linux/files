/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The rules archives go through before anything is written (explorer spec
// 3.12): entry names, links, the bomb guard, the names "Compress to" and
// "Extract all" choose, and rayearchive: URLs. No archive is opened here.

#include "archive/archiveformat.h"
#include "archive/archiveguard.h"
#include "archive/archivepaths.h"
#include "archive/archiveurl.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace Raye::Archive;

class ArchivePathsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cleanEntryPath_data();
    void cleanEntryPath();
    void linkTargets_data();
    void linkTargets();
    void resolvesInsideOnDisk();
    void bombGuard();
    void extensions_data();
    void extensions();
    void createExtension();
    void compressionLevels();
    void archiveNames();
    void uniqueNames();
    void mimeTypes();
    void readOnlyMessage();
    void splitPath();
    void urls();
};

void ArchivePathsTest::cleanEntryPath_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("verdict");
    QTest::addColumn<QString>("clean");

    const int ok = int(PathVerdict::Ok);
    const int trav = int(PathVerdict::Traversal);
    const int empty = int(PathVerdict::Empty);
    const int invalid = int(PathVerdict::Invalid);

    QTest::newRow("plain") << QStringLiteral("a.txt") << ok << QStringLiteral("a.txt");
    QTest::newRow("nested") << QStringLiteral("docs/2026/a.txt") << ok << QStringLiteral("docs/2026/a.txt");
    QTest::newRow("folder slash") << QStringLiteral("docs/") << ok << QStringLiteral("docs");
    QTest::newRow("dot parts") << QStringLiteral("./docs/./a.txt") << ok << QStringLiteral("docs/a.txt");
    QTest::newRow("double slash") << QStringLiteral("docs//a.txt") << ok << QStringLiteral("docs/a.txt");
    QTest::newRow("backslash") << QStringLiteral("docs\\a.txt") << ok << QStringLiteral("docs/a.txt");
    QTest::newRow("absolute") << QStringLiteral("/etc/passwd") << ok << QStringLiteral("etc/passwd");
    QTest::newRow("absolute twice") << QStringLiteral("//etc/passwd") << ok << QStringLiteral("etc/passwd");
    QTest::newRow("drive") << QStringLiteral("C:\\Windows\\win.ini") << ok << QStringLiteral("Windows/win.ini");
    QTest::newRow("drive slash") << QStringLiteral("c:/x") << ok << QStringLiteral("x");
    QTest::newRow("colon inside") << QStringLiteral("notes:2026.txt") << ok << QStringLiteral("notes:2026.txt");
    QTest::newRow("dotdot") << QStringLiteral("../evil") << trav << QString();
    QTest::newRow("dotdot inside") << QStringLiteral("a/../../evil") << trav << QString();
    QTest::newRow("dotdot harmless still refused") << QStringLiteral("a/../b") << trav << QString();
    QTest::newRow("dotdot backslash") << QStringLiteral("..\\..\\evil") << trav << QString();
    QTest::newRow("absolute dotdot") << QStringLiteral("/../etc") << trav << QString();
    QTest::newRow("dots in name") << QStringLiteral("..hidden/...") << ok << QStringLiteral("..hidden/...");
    QTest::newRow("empty") << QString() << empty << QString();
    QTest::newRow("root") << QStringLiteral("/") << empty << QString();
    QTest::newRow("dot") << QStringLiteral("./") << empty << QString();
    QTest::newRow("nul") << QStringLiteral("a") + QChar(0) + QStringLiteral("b") << invalid << QString();
    QTest::newRow("long part") << QString(256, QLatin1Char('x')) << invalid << QString();
    QTest::newRow("long utf8 part") << QString(128, QChar(0x00e9)) << invalid << QString();
    QTest::newRow("long path") << QString(2100, QLatin1Char('a')).replace(QLatin1Char('a'), QStringLiteral("a/")) << invalid << QString();
    QTest::newRow("unicode") << QStringLiteral("Été/日本.txt") << ok << QStringLiteral("Été/日本.txt");
}

void ArchivePathsTest::cleanEntryPath()
{
    QFETCH(QString, name);
    QFETCH(int, verdict);
    QFETCH(QString, clean);
    QString out = QStringLiteral("untouched");
    const PathVerdict got = Raye::Archive::cleanEntryPath(name, &out);
    QCOMPARE(int(got), verdict);
    if (got == PathVerdict::Ok) {
        QCOMPARE(out, clean);
    }
}

void ArchivePathsTest::linkTargets_data()
{
    QTest::addColumn<QString>("entry");
    QTest::addColumn<QString>("target");
    QTest::addColumn<bool>("inside");

    QTest::newRow("sibling") << QStringLiteral("lib/libx.so") << QStringLiteral("libx.so.1") << true;
    QTest::newRow("up one, still in") << QStringLiteral("bin/tool") << QStringLiteral("../lib/tool") << true;
    QTest::newRow("to root") << QStringLiteral("a/b/c") << QStringLiteral("../..") << true;
    QTest::newRow("dot") << QStringLiteral("a/l") << QStringLiteral(".") << true;
    QTest::newRow("escape") << QStringLiteral("l") << QStringLiteral("..") << false;
    QTest::newRow("escape deep") << QStringLiteral("a/b/l") << QStringLiteral("../../../etc/passwd") << false;
    QTest::newRow("escape then back") << QStringLiteral("l") << QStringLiteral("../x/../y") << false;
    QTest::newRow("absolute") << QStringLiteral("l") << QStringLiteral("/etc/passwd") << false;
    QTest::newRow("empty") << QStringLiteral("l") << QString() << false;
}

void ArchivePathsTest::linkTargets()
{
    QFETCH(QString, entry);
    QFETCH(QString, target);
    QFETCH(bool, inside);
    QCOMPARE(linkTargetStaysInside(entry, target), inside);
}

void ArchivePathsTest::resolvesInsideOnDisk()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = dir.path() + QStringLiteral("/root");
    QVERIFY(QDir().mkpath(root + QStringLiteral("/a")));
    QVERIFY(QDir().mkpath(dir.path() + QStringLiteral("/outside")));

    QVERIFY(resolvesInside(root, root));
    QVERIFY(resolvesInside(root, root + QStringLiteral("/a")));
    QVERIFY(resolvesInside(root, root + QStringLiteral("/missing/file")));
    QVERIFY(!resolvesInside(root, dir.path() + QStringLiteral("/outside")));
    // the name starts like root but is a sibling folder
    QVERIFY(!resolvesInside(root, root + QStringLiteral("x")));
    QVERIFY(!resolvesInside(root, root + QStringLiteral("/missing/../../outside")));

    // the chain the text check alone lets through: a/b -> "." then
    // a/b/../.. reads as root but is the folder above it on disk
    QCOMPARE(::symlink(".", QFile::encodeName(root + QStringLiteral("/a/b")).constData()), 0);
    QVERIFY(linkTargetStaysInside(QStringLiteral("l"), QStringLiteral("a/b/../..")));
    QVERIFY(!resolvesInside(root, root + QStringLiteral("/a/b/../..")));
    QVERIFY(resolvesInside(root, root + QStringLiteral("/a/b/..")));

    QCOMPARE(::symlink("../../outside", QFile::encodeName(root + QStringLiteral("/a/out")).constData()), 0);
    QVERIFY(!resolvesInside(root, root + QStringLiteral("/a/out")));
    QVERIFY(!resolvesInside(root, root + QStringLiteral("/a/out/new")));
}

void ArchivePathsTest::bombGuard()
{
    const qint64 mb = 1024 * 1024;
    // a small archive that unpacks to more than 2 GB: stopped
    BombGuard bomb(10 * mb);
    QVERIFY(bomb.addBytes(BombGuard::sizeLimit));
    QVERIFY(!bomb.tripped());
    QVERIFY(!bomb.addBytes(1));
    QVERIFY(bomb.tripped());
    QVERIFY(!bomb.addBytes(1));

    // a big archive with ordinary compression: never stopped
    BombGuard big(5 * 1024 * mb);
    QVERIFY(big.addBytes(6 * 1024 * mb));
    QVERIFY(!big.tripped());

    // more than 100 times, but small: fine (a text file compresses well)
    BombGuard text(mb);
    QVERIFY(text.addBytes(500 * mb));

    // off: the user said go on
    BombGuard off(10 * mb, false);
    QVERIFY(off.addBytes(10 * BombGuard::sizeLimit));
    QVERIFY(!off.tripped());

    // a million empty files
    BombGuard many(mb);
    for (int i = 0; i < 10; ++i) {
        QVERIFY(many.addItem());
    }
    BombGuard flood(mb);
    bool stopped = false;
    for (qint64 i = 0; i <= BombGuard::itemLimit && !stopped; ++i) {
        stopped = !flood.addItem();
    }
    QVERIFY(stopped);

    // an empty archive file cannot divide by zero
    BombGuard zero(0);
    QVERIFY(!zero.addBytes(BombGuard::sizeLimit + 1));
}

void ArchivePathsTest::extensions_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("extension");
    QTest::addColumn<QString>("base");
    QTest::addColumn<QString>("format");

    QTest::newRow("zip") << QStringLiteral("Holiday.zip") << QStringLiteral(".zip") << QStringLiteral("Holiday") << QStringLiteral("ZIP");
    QTest::newRow("ZIP caps") << QStringLiteral("OLD.ZIP") << QStringLiteral(".ZIP") << QStringLiteral("OLD") << QStringLiteral("ZIP");
    QTest::newRow("7z") << QStringLiteral("a.b.7z") << QStringLiteral(".7z") << QStringLiteral("a.b") << QStringLiteral("7z");
    QTest::newRow("tar.gz") << QStringLiteral("logs.tar.gz") << QStringLiteral(".tar.gz") << QStringLiteral("logs") << QStringLiteral("TAR");
    QTest::newRow("tgz") << QStringLiteral("logs.tgz") << QStringLiteral(".tgz") << QStringLiteral("logs") << QStringLiteral("TAR");
    QTest::newRow("tar.zst") << QStringLiteral("x-1.0.tar.zst") << QStringLiteral(".tar.zst") << QStringLiteral("x-1.0") << QStringLiteral("TAR");
    QTest::newRow("rar") << QStringLiteral("old.rar") << QStringLiteral(".rar") << QStringLiteral("old") << QStringLiteral("RAR");
    QTest::newRow("iso") << QStringLiteral("disk.iso") << QStringLiteral(".iso") << QStringLiteral("disk") << QStringLiteral("ISO");
    QTest::newRow("cab") << QStringLiteral("drv.cab") << QStringLiteral(".cab") << QStringLiteral("drv") << QStringLiteral("CAB");
    QTest::newRow("not archive") << QStringLiteral("report.docx") << QString() << QStringLiteral("report.docx") << QString();
    QTest::newRow("only extension") << QStringLiteral(".zip") << QStringLiteral(".zip") << QStringLiteral(".zip") << QStringLiteral("ZIP");
    QTest::newRow("no name") << QString() << QString() << QString() << QString();
}

void ArchivePathsTest::extensions()
{
    QFETCH(QString, name);
    QFETCH(QString, extension);
    QFETCH(QString, base);
    QFETCH(QString, format);
    QCOMPARE(archiveExtension(name), extension);
    QCOMPARE(baseNameOf(name), base);
    QCOMPARE(formatName(name), format);
}

void ArchivePathsTest::createExtension()
{
    CreateOptions o;
    QCOMPARE(o.extension(), QStringLiteral(".zip"));
    o.format = Format::SevenZip;
    QCOMPARE(o.extension(), QStringLiteral(".7z"));
    o.format = Format::Tar;
    QCOMPARE(o.extension(), QStringLiteral(".tar"));
    o.tarFilter = TarFilter::Gzip;
    QCOMPARE(o.extension(), QStringLiteral(".tar.gz"));
    o.tarFilter = TarFilter::Bzip2;
    QCOMPARE(o.extension(), QStringLiteral(".tar.bz2"));
    o.tarFilter = TarFilter::Xz;
    QCOMPARE(o.extension(), QStringLiteral(".tar.xz"));
    o.tarFilter = TarFilter::Zstd;
    QCOMPARE(o.extension(), QStringLiteral(".tar.zst"));
}

void ArchivePathsTest::compressionLevels()
{
    CreateOptions o;
    o.level = Level::Store;
    QCOMPARE(o.compressionLevel(), 0);
    o.level = Level::Fastest;
    QCOMPARE(o.compressionLevel(), 1);
    o.level = Level::Fast;
    QCOMPARE(o.compressionLevel(), 3);
    o.level = Level::Normal;
    QCOMPARE(o.compressionLevel(), 5);
    o.level = Level::Ultra;
    QCOMPARE(o.compressionLevel(), 9);
    o.format = Format::Tar;
    o.tarFilter = TarFilter::Zstd;
    o.level = Level::Maximum;
    QCOMPARE(o.compressionLevel(), 7);
    o.tarFilter = TarFilter::None;
    QCOMPARE(o.compressionLevel(), 0);
}

void ArchivePathsTest::archiveNames()
{
    QCOMPARE(archiveBaseNameFor(QStringLiteral("Report.docx"), false), QStringLiteral("Report"));
    QCOMPARE(archiveBaseNameFor(QStringLiteral("Photos"), true), QStringLiteral("Photos"));
    QCOMPARE(archiveBaseNameFor(QStringLiteral("v1.2 notes"), true), QStringLiteral("v1.2 notes"));
    QCOMPARE(archiveBaseNameFor(QStringLiteral("logs.tar.gz"), false), QStringLiteral("logs"));
    QCOMPARE(archiveBaseNameFor(QStringLiteral(".bashrc"), false), QStringLiteral(".bashrc"));
    QCOMPARE(archiveBaseNameFor(QStringLiteral("Makefile"), false), QStringLiteral("Makefile"));

    QVERIFY(isValidFileName(QStringLiteral("Report")));
    QVERIFY(isValidFileName(QStringLiteral("...a")));
    QVERIFY(!isValidFileName(QString()));
    QVERIFY(!isValidFileName(QStringLiteral(".")));
    QVERIFY(!isValidFileName(QStringLiteral("..")));
    QVERIFY(!isValidFileName(QStringLiteral("a/b")));
    QVERIFY(!isValidFileName(QStringLiteral("a") + QChar(0)));
    QVERIFY(!isValidFileName(QString(256, QLatin1Char('n'))));
}

void ArchivePathsTest::uniqueNames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto touch = [&dir](const QString &name) {
        QFile f(dir.filePath(name));
        QVERIFY(f.open(QIODevice::WriteOnly));
    };
    QCOMPARE(uniqueFileName(dir.path(), QStringLiteral("Report.zip")), QStringLiteral("Report.zip"));
    touch(QStringLiteral("Report.zip"));
    QCOMPARE(uniqueFileName(dir.path(), QStringLiteral("Report.zip")), QStringLiteral("Report (2).zip"));
    touch(QStringLiteral("Report (2).zip"));
    QCOMPARE(uniqueFileName(dir.path(), QStringLiteral("Report.zip")), QStringLiteral("Report (3).zip"));
    touch(QStringLiteral("logs.tar.gz"));
    QCOMPARE(uniqueFileName(dir.path(), QStringLiteral("logs.tar.gz")), QStringLiteral("logs (2).tar.gz"));
    // folders count too
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("Photos")));
    QCOMPARE(uniqueFileName(dir.path(), QStringLiteral("Photos")), QStringLiteral("Photos (2)"));
    // a dangling link is taken as well
    QCOMPARE(::symlink("nowhere", QFile::encodeName(dir.filePath(QStringLiteral("Link.zip"))).constData()), 0);
    QCOMPARE(uniqueFileName(dir.path(), QStringLiteral("Link.zip")), QStringLiteral("Link (2).zip"));
}

void ArchivePathsTest::mimeTypes()
{
    for (const char *m : {"application/zip",
                          "application/x-7z-compressed",
                          "application/vnd.rar",
                          "application/x-rar",
                          "application/x-tar",
                          "application/x-compressed-tar",
                          "application/x-bzip2-compressed-tar",
                          "application/x-xz-compressed-tar",
                          "application/x-zstd-compressed-tar",
                          "application/x-cd-image",
                          "application/vnd.ms-cab-compressed"}) {
        QVERIFY2(isBrowsableMimeType(QString::fromLatin1(m)), m);
        QVERIFY(browsableMimeTypes().contains(QString::fromLatin1(m)));
    }
    QVERIFY(!isBrowsableMimeType(QStringLiteral("application/vnd.oasis.opendocument.text")));
    QVERIFY(!isBrowsableMimeType(QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document")));
    QVERIFY(!isBrowsableMimeType(QStringLiteral("application/x-java-archive")));
    QVERIFY(!isBrowsableMimeType(QString()));

    QVERIFY(isWritableMimeType(QStringLiteral("application/zip")));
    QVERIFY(!isWritableMimeType(QStringLiteral("application/vnd.rar")));
    QVERIFY(isWritableMimeType(QStringLiteral("application/x-7z-compressed")));
    QVERIFY(isWritableMimeType(QStringLiteral("application/x-tar")));
    QVERIFY(!isWritableMimeType(QStringLiteral("application/x-compressed-tar")));
}

void ArchivePathsTest::readOnlyMessage()
{
    QCOMPARE(cannotWriteMessage(QStringLiteral("old.rar")), QStringLiteral("Files can't add to RAR archives."));
    QCOMPARE(cannotWriteMessage(QStringLiteral("disk.iso")), QStringLiteral("Files can't add to ISO archives."));
    QCOMPARE(cannotWriteMessage(QStringLiteral("weird")), QStringLiteral("Files can't add to this archive."));
}

void ArchivePathsTest::splitPath()
{
    const QString archive = QStringLiteral("/home/ana/Holiday.zip");
    auto isFile = [&archive](const QString &p) {
        return p == archive;
    };
    ArchiveLocation l = splitArchivePath(QStringLiteral("/home/ana/Holiday.zip/photos/1.jpg"), isFile);
    QVERIFY(l.isValid());
    QCOMPARE(l.archivePath, archive);
    QCOMPARE(l.innerPath, QStringLiteral("photos/1.jpg"));

    l = splitArchivePath(QStringLiteral("/home/ana/Holiday.zip"), isFile);
    QCOMPARE(l.archivePath, archive);
    QCOMPARE(l.innerPath, QString());

    l = splitArchivePath(QStringLiteral("/home/ana/Holiday.zip/"), isFile);
    QCOMPARE(l.archivePath, archive);
    QCOMPARE(l.innerPath, QString());

    l = splitArchivePath(QStringLiteral("/home/ana//Holiday.zip//photos/"), isFile);
    QCOMPARE(l.archivePath, archive);
    QCOMPARE(l.innerPath, QStringLiteral("photos"));

    // ".." in the URL is resolved first and never reaches the archive
    l = splitArchivePath(QStringLiteral("/home/ana/x/../Holiday.zip/a"), isFile);
    QCOMPARE(l.archivePath, archive);
    l = splitArchivePath(QStringLiteral("/home/ana/Holiday.zip/../../etc"), isFile);
    QVERIFY(!l.isValid());

    QVERIFY(!splitArchivePath(QStringLiteral("/home/ana"), isFile).isValid());
    QVERIFY(!splitArchivePath(QStringLiteral("relative/Holiday.zip"), isFile).isValid());
    QVERIFY(!splitArchivePath(QString(), isFile).isValid());
}

void ArchivePathsTest::urls()
{
    const QUrl zip = browseUrl(QUrl::fromLocalFile(QStringLiteral("/home/ana/Holiday.zip")), QStringLiteral("application/zip"));
    QCOMPARE(zip.scheme(), QStringLiteral("rayearchive"));
    QCOMPARE(zip.path(), QStringLiteral("/home/ana/Holiday.zip"));
    QVERIFY(!browseUrl(QUrl::fromLocalFile(QStringLiteral("/home/ana/a.odt")), QStringLiteral("application/vnd.oasis.opendocument.text")).isValid());
    QVERIFY(!browseUrl(QUrl(QStringLiteral("smb://nas/a.zip")), QStringLiteral("application/zip")).isValid());

    const QUrl entry = entryUrl(QStringLiteral("/home/ana/My files #1.zip"), QStringLiteral("a b/c?.txt"));
    QCOMPARE(entry.scheme(), QStringLiteral("rayearchive"));
    QCOMPARE(entry.path(), QStringLiteral("/home/ana/My files #1.zip/a b/c?.txt"));
    QCOMPARE(QUrl(entry.toString()).path(), entry.path());
    QCOMPARE(entryUrl(QStringLiteral("/a.zip"), QString()).path(), QStringLiteral("/a.zip"));
}

QTEST_GUILESS_MAIN(ArchivePathsTest)

#include "test_archivepaths.moc"
