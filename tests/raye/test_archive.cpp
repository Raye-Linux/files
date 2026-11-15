/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Reading and writing archives (explorer spec 3.12): every format Files
// writes goes there and back with the same bytes, RAR and protected ZIPs
// are read, and archives built to escape the destination, fill the disk or
// write through links are refused with nothing left outside.

#include "archive/archivereader.h"
#include "archive/archivewriter.h"
#include "data/archives/fixtures.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <clocale>
#include <sys/stat.h>
#include <unistd.h>

using namespace Raye::Archive;
using namespace Fixtures;

Q_DECLARE_METATYPE(Raye::Archive::Format)
Q_DECLARE_METATYPE(Raye::Archive::Level)
Q_DECLARE_METATYPE(Raye::Archive::TarFilter)

namespace
{
QByteArray digest(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(&f);
    return h.result().toHex();
}

void writeFile(const QString &path, const QByteArray &data, QFile::Permissions perms = {})
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QCOMPARE(f.write(data), data.size());
    f.close();
    if (perms) {
        QVERIFY(f.setPermissions(perms));
    }
}

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

}

class ArchiveTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void roundTrip_data();
    void roundTrip();
    void independentReaders();
    void listAndRead();
    void rar();
    void password();
    void traversalRefused_data();
    void traversalRefused();
    void absoluteNamesStayInside();
    void linkChainRefused();
    void hardlinkOutsideRefused();
    void hardlinkInside();
    void permissionsAndOddEntries();
    void duplicateNamesLaterWins();
    void bomb();
    void cancel();
    void badFiles();
    void createRefusesExisting();
    void archiveInsideItsSource();
    void rewriteZip();
    void noExtension();
    void rewriteSevenZip();

private:
    QString fixture(const QString &name, const QByteArray &data);
    QString m_dir;
    QString m_source;
    QTemporaryDir m_temp;
};

void ArchiveTest::initTestCase()
{
    // Names are UTF-8 on Raye; the test runs with no LANG set.
    std::setlocale(LC_ALL, "C.UTF-8");
    QVERIFY(m_temp.isValid());
    m_dir = m_temp.path();
    m_source = m_dir + QStringLiteral("/src/Holiday");

    QByteArray photo(300 * 1024, Qt::Uninitialized);
    QRandomGenerator rng(16);
    for (char &c : photo) {
        c = char(rng.bounded(256));
    }
    writeFile(m_source + QStringLiteral("/photos/1.jpg"), photo);
    writeFile(m_source + QStringLiteral("/notes.txt"), QByteArray("Pack the charger.\n").repeated(400));
    writeFile(m_source + QStringLiteral("/Été 日本.txt"), QByteArray("unicode name\n"));
    writeFile(m_source + QStringLiteral("/bin/run.sh"),
              QByteArray("#!/bin/sh\necho hi\n"),
              QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
    writeFile(m_source + QStringLiteral("/.hidden"), QByteArray("dot file"));
    QVERIFY(QDir().mkpath(m_source + QStringLiteral("/empty")));
    QCOMPARE(::symlink("notes.txt", QFile::encodeName(m_source + QStringLiteral("/link-to-notes")).constData()), 0);
    // a link back up: storing what it points to must not loop forever
    QCOMPARE(::symlink("..", QFile::encodeName(m_source + QStringLiteral("/photos/up")).constData()), 0);
}

QString ArchiveTest::fixture(const QString &name, const QByteArray &data)
{
    const QString path = m_dir + QStringLiteral("/fixtures/") + name;
    writeFile(path, data);
    return path;
}

void ArchiveTest::roundTrip_data()
{
    QTest::addColumn<Format>("format");
    QTest::addColumn<Level>("level");
    QTest::addColumn<TarFilter>("filter");
    QTest::addColumn<bool>("keepLinks");

    QTest::newRow("zip normal") << Format::Zip << Level::Normal << TarFilter::None << false;
    QTest::newRow("zip store links") << Format::Zip << Level::Store << TarFilter::None << true;
    QTest::newRow("zip maximum") << Format::Zip << Level::Maximum << TarFilter::None << false;
    QTest::newRow("7z normal") << Format::SevenZip << Level::Normal << TarFilter::None << false;
    QTest::newRow("7z store") << Format::SevenZip << Level::Store << TarFilter::None << false;
    QTest::newRow("7z fast links") << Format::SevenZip << Level::Fast << TarFilter::None << true;
    QTest::newRow("tar links") << Format::Tar << Level::Normal << TarFilter::None << true;
    QTest::newRow("tar.gz") << Format::Tar << Level::Normal << TarFilter::Gzip << false;
    QTest::newRow("tar.bz2") << Format::Tar << Level::Fast << TarFilter::Bzip2 << false;
    QTest::newRow("tar.xz") << Format::Tar << Level::Maximum << TarFilter::Xz << true;
    QTest::newRow("tar.zst") << Format::Tar << Level::Normal << TarFilter::Zstd << true;
}

void ArchiveTest::roundTrip()
{
    QFETCH(Format, format);
    QFETCH(Level, level);
    QFETCH(TarFilter, filter);
    QFETCH(bool, keepLinks);

    CreateOptions options;
    options.format = format;
    options.level = level;
    options.tarFilter = filter;
    options.keepSymlinks = keepLinks;

    const QString tag = QString::fromLatin1(QTest::currentDataTag()).replace(QLatin1Char(' '), QLatin1Char('-'));
    const QString archivePath = m_dir + QStringLiteral("/out/") + tag + options.extension();
    QVERIFY(QDir().mkpath(m_dir + QStringLiteral("/out")));

    const qint64 total = Writer::totalSize({m_source}, options);
    QVERIFY(total > 300 * 1024);
    qint64 lastProgress = 0;
    Writer writer;
    QVERIFY2(writer.create(archivePath,
                           {m_source},
                           options,
                           [&lastProgress](qint64 done) {
                               lastProgress = done;
                               return true;
                           }),
             qPrintable(writer.detail()));
    QCOMPARE(lastProgress, total);
    QVERIFY(QFileInfo(archivePath).size() > 0);
    // nothing left behind under a hidden name
    QCOMPARE(QDir(m_dir + QStringLiteral("/out")).entryList(QDir::Hidden | QDir::Files).filter(QStringLiteral(".raye-archive")).size(), 0);
    if (level == Level::Store && format != Format::Tar) {
        QVERIFY(QFileInfo(archivePath).size() > 300 * 1024);
    }

    const QString root = m_dir + QStringLiteral("/x/") + tag;
    QVERIFY(QDir().mkpath(root));
    Reader reader(archivePath);
    QVERIFY2(reader.extractAll(root), qPrintable(reader.detail()));
    QCOMPARE(reader.error(), Error::None);

    const QString out = root + QStringLiteral("/Holiday");
    QCOMPARE(digest(out + QStringLiteral("/photos/1.jpg")), digest(m_source + QStringLiteral("/photos/1.jpg")));
    QCOMPARE(digest(out + QStringLiteral("/notes.txt")), digest(m_source + QStringLiteral("/notes.txt")));
    QCOMPARE(readAll(out + QStringLiteral("/Été 日本.txt")), QByteArray("unicode name\n"));
    QCOMPARE(readAll(out + QStringLiteral("/.hidden")), QByteArray("dot file"));
    QVERIFY(QFileInfo(out + QStringLiteral("/empty")).isDir());
    QVERIFY(QFileInfo(out + QStringLiteral("/bin/run.sh")).isExecutable());

    const QFileInfo link(out + QStringLiteral("/link-to-notes"));
    if (keepLinks) {
        QVERIFY(link.isSymLink());
        QCOMPARE(QFile::symLinkTarget(link.filePath()), out + QStringLiteral("/notes.txt"));
        QVERIFY(QFileInfo(out + QStringLiteral("/photos/up")).isSymLink());
    } else {
        QVERIFY(!link.isSymLink());
        QCOMPARE(digest(link.filePath()), digest(m_source + QStringLiteral("/notes.txt")));
        // the loop was cut: "up" holds Holiday once, not forever
        QVERIFY(!QFileInfo(out + QStringLiteral("/photos/up")).exists());
    }
}

void ArchiveTest::independentReaders()
{
    const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (python.isEmpty()) {
        if (qEnvironmentVariableIsSet("RAYE_TESTS_STRICT")) {
            QFAIL("no python3 to read the archives with another program");
        }
        QSKIP("no python3 to read the archives with another program");
    }
    const QString zip = m_dir + QStringLiteral("/out/zip-normal.zip");
    const QString tgz = m_dir + QStringLiteral("/out/tar.gz.tar.gz");
    QVERIFY(QFile::exists(zip));
    QVERIFY(QFile::exists(tgz));
    // Python's own readers: names, contents (CRC) and the UTF-8 flag.
    const QString script = QStringLiteral(
        "import sys, zipfile, tarfile\n"
        "z = zipfile.ZipFile(sys.argv[1])\n"
        "assert z.testzip() is None\n"
        "names = set(z.namelist())\n"
        "assert 'Holiday/Été 日本.txt' in names, names\n"
        "assert z.read('Holiday/notes.txt') == b'Pack the charger.\\n' * 400\n"
        "t = tarfile.open(sys.argv[2])\n"
        "assert t.extractfile('Holiday/notes.txt').read() == b'Pack the charger.\\n' * 400\n"
        "assert 'Holiday/Été 日本.txt' in t.getnames()\n"
        "print('ok')\n");
    QProcess p;
    p.start(python, {QStringLiteral("-I"), QStringLiteral("-B"), QStringLiteral("-c"), script, zip, tgz});
    QVERIFY(p.waitForFinished(30000));
    QCOMPARE(p.readAllStandardError(), QByteArray());
    QCOMPARE(p.readAllStandardOutput().trimmed(), QByteArray("ok"));
}

void ArchiveTest::listAndRead()
{
    Reader reader(m_dir + QStringLiteral("/out/7z-normal.7z"));
    QList<Entry> entries;
    QVERIFY(reader.list(&entries));
    QStringList paths;
    for (const Entry &e : std::as_const(entries)) {
        paths.append(e.path + (e.type == Entry::Folder ? QStringLiteral("/") : QString()));
    }
    QVERIFY2(paths.contains(QStringLiteral("Holiday/photos/1.jpg")), qPrintable(paths.join(QLatin1Char(','))));
    QVERIFY(paths.contains(QStringLiteral("Holiday/empty/")));
    for (const Entry &e : std::as_const(entries)) {
        if (e.path == QLatin1String("Holiday/photos/1.jpg")) {
            QCOMPARE(e.size, 300 * 1024);
            QVERIFY(e.modified.isValid());
        }
    }

    QByteArray got;
    QVERIFY(reader.read(QStringLiteral("Holiday/notes.txt"), [&got](const char *d, qint64 n) {
        got.append(d, n);
        return true;
    }));
    QCOMPARE(got, readAll(m_source + QStringLiteral("/notes.txt")));
    QVERIFY(!reader.read(QStringLiteral("Holiday/nope"), [](const char *, qint64) {
        return true;
    }));
    QCOMPARE(reader.error(), Error::NotFound);

    // a link is read as what it points to, inside the archive
    Reader tar(m_dir + QStringLiteral("/out/tar-links.tar"));
    got.clear();
    QVERIFY(tar.read(QStringLiteral("Holiday/link-to-notes"), [&got](const char *d, qint64 n) {
        got.append(d, n);
        return true;
    }));
    QCOMPARE(got, readAll(m_source + QStringLiteral("/notes.txt")));
}

void ArchiveTest::rar()
{
    const QString path = fixture(QStringLiteral("old.rar"),
                                 Fixtures::rar({folder(QStringLiteral("docs")), file(QStringLiteral("docs/readme.txt"), QByteArray("from a rar\n"))}));
    QVERIFY(QFileInfo(path).size() < 4096);
    Reader reader(path);
    QList<Entry> entries;
    QVERIFY2(reader.list(&entries), qPrintable(reader.detail()));
    QCOMPARE(entries.size(), 2);
    const QString root = m_dir + QStringLiteral("/x/rar");
    QVERIFY(QDir().mkpath(root));
    QVERIFY2(reader.extractAll(root), qPrintable(reader.detail()));
    QCOMPARE(readAll(root + QStringLiteral("/docs/readme.txt")), QByteArray("from a rar\n"));
}

void ArchiveTest::password()
{
    const QString path = fixture(QStringLiteral("secret.zip"),
                                 Fixtures::zip({file(QStringLiteral("plan.txt"), QByteArray("the plan\n").repeated(10))}, QByteArray("raye")));
    Reader reader(path);
    QList<Entry> entries;
    QVERIFY(reader.list(&entries));
    QCOMPARE(entries.size(), 1);
    QVERIFY(entries.first().encrypted);

    const QString root = m_dir + QStringLiteral("/x/secret");
    QVERIFY(QDir().mkpath(root));
    QVERIFY(!reader.extractAll(root));
    QCOMPARE(reader.error(), Error::PasswordNeeded);
    QVERIFY(!reader.read(QStringLiteral("plan.txt"), [](const char *, qint64) {
        return true;
    }));
    QCOMPARE(reader.error(), Error::PasswordNeeded);

    QDir(root).removeRecursively();
    QVERIFY(QDir().mkpath(root));
    reader.setPassword(QStringLiteral("wrong"));
    QVERIFY(!reader.extractAll(root));
    QCOMPARE(reader.error(), Error::WrongPassword);

    QDir(root).removeRecursively();
    QVERIFY(QDir().mkpath(root));
    reader.setPassword(QStringLiteral("raye"));
    QVERIFY2(reader.extractAll(root), qPrintable(reader.detail()));
    QCOMPARE(readAll(root + QStringLiteral("/plan.txt")), QByteArray("the plan\n").repeated(10));
}

void ArchiveTest::traversalRefused_data()
{
    QTest::addColumn<QByteArray>("archive");
    QTest::addColumn<int>("expected");

    const int unsafe = int(Error::Unsafe);
    QTest::newRow("tar ../") << Fixtures::tar({file(QStringLiteral("ok.txt"), "fine"), file(QStringLiteral("../evil.txt"), "pwned")}) << unsafe;
    QTest::newRow("tar deep ../") << Fixtures::tar({file(QStringLiteral("a/../../evil.txt"), "pwned")}) << unsafe;
    QTest::newRow("zip ../") << Fixtures::zip({file(QStringLiteral("../../evil.txt"), "pwned")}) << unsafe;
    QTest::newRow("zip backslash ../") << Fixtures::zip({file(QStringLiteral("..\\evil.txt"), "pwned")}) << unsafe;
    QTest::newRow("tar link absolute") << Fixtures::tar({symlink(QStringLiteral("etc"), QStringLiteral("/etc"))}) << unsafe;
    QTest::newRow("tar link up") << Fixtures::tar({symlink(QStringLiteral("out"), QStringLiteral("../outside"))}) << unsafe;
    QTest::newRow("zip link absolute") << Fixtures::zip({symlink(QStringLiteral("pw"), QStringLiteral("/etc/passwd"))}) << unsafe;
    // a link inside the tree, then a file written through it
    QTest::newRow("write through link") << Fixtures::tar({folder(QStringLiteral("sub")),
                                                          symlink(QStringLiteral("s"), QStringLiteral("sub")),
                                                          file(QStringLiteral("s/x.txt"), "through")})
                                        << unsafe;
}

void ArchiveTest::traversalRefused()
{
    QFETCH(QByteArray, archive);
    QFETCH(int, expected);
    const QString tag = QString::fromLatin1(QTest::currentDataTag()).replace(QLatin1Char(' '), QLatin1Char('_')).remove(QLatin1Char('/'));
    const QString path = fixture(QStringLiteral("evil-") + tag, archive);
    // root sits two levels down, so "../" and "../../" land in folders the
    // test can look into
    const QString base = m_dir + QStringLiteral("/evil/") + tag;
    const QString root = base + QStringLiteral("/a/root");
    QVERIFY(QDir().mkpath(root));
    Reader reader(path);
    QVERIFY(!reader.extractAll(root));
    QCOMPARE(int(reader.error()), expected);
    QVERIFY(!QFile::exists(base + QStringLiteral("/a/evil.txt")));
    QVERIFY(!QFile::exists(base + QStringLiteral("/evil.txt")));
    QVERIFY(!QFile::exists(base + QStringLiteral("/a/outside")));
    QVERIFY(!QFile::exists(root + QStringLiteral("/sub/x.txt")));

    // listing hides what it cannot show safely
    QList<Entry> entries;
    reader.list(&entries);
    for (const Entry &e : std::as_const(entries)) {
        QVERIFY(!e.path.contains(QLatin1String("..")));
    }
}

void ArchiveTest::absoluteNamesStayInside()
{
    const QString path = fixture(QStringLiteral("absolute.tar"), Fixtures::tar({file(QStringLiteral("/raye-absolute-test/x.txt"), "inside")}));
    const QString root = m_dir + QStringLiteral("/x/absolute");
    QVERIFY(QDir().mkpath(root));
    Reader reader(path);
    QVERIFY2(reader.extractAll(root), qPrintable(reader.detail()));
    QCOMPARE(readAll(root + QStringLiteral("/raye-absolute-test/x.txt")), QByteArray("inside"));
    QVERIFY(!QFile::exists(QStringLiteral("/raye-absolute-test")));
}

void ArchiveTest::linkChainRefused()
{
    // each link reads as staying inside; together "l" is the folder above
    const QString path = fixture(QStringLiteral("chain.tar"),
                                 Fixtures::tar({folder(QStringLiteral("a")),
                                                symlink(QStringLiteral("a/b"), QStringLiteral(".")),
                                                symlink(QStringLiteral("l"), QStringLiteral("a/b/../.."))}));
    const QString root = m_dir + QStringLiteral("/x/chain");
    QVERIFY(QDir().mkpath(root));
    Reader reader(path);
    QVERIFY(!reader.extractAll(root));
    QCOMPARE(reader.error(), Error::Unsafe);
}

void ArchiveTest::hardlinkOutsideRefused()
{
    const QString root = m_dir + QStringLiteral("/x/hard");
    QVERIFY(QDir().mkpath(root));
    Reader up(fixture(QStringLiteral("hard-up.tar"), Fixtures::tar({hardlink(QStringLiteral("h"), QStringLiteral("../../etc/passwd"))})));
    QVERIFY(!up.extractAll(root));
    QCOMPARE(up.error(), Error::Unsafe);

    // an absolute target is read inside the tree, where it does not exist
    Reader abs(fixture(QStringLiteral("hard-abs.tar"), Fixtures::tar({hardlink(QStringLiteral("h"), QStringLiteral("/etc/passwd"))})));
    QVERIFY(!abs.extractAll(root));
    QVERIFY(!QFile::exists(root + QStringLiteral("/h")));
}

void ArchiveTest::hardlinkInside()
{
    const QString root = m_dir + QStringLiteral("/x/hardok");
    QVERIFY(QDir().mkpath(root));
    Reader reader(fixture(QStringLiteral("hard-ok.tar"),
                          Fixtures::tar({file(QStringLiteral("a.txt"), "shared data"), hardlink(QStringLiteral("b.txt"), QStringLiteral("a.txt"))})));
    QVERIFY2(reader.extractAll(root), qPrintable(reader.detail()));
    QCOMPARE(readAll(root + QStringLiteral("/b.txt")), QByteArray("shared data"));
    QByteArray got;
    QVERIFY(reader.read(QStringLiteral("b.txt"), [&got](const char *d, qint64 n) {
        got.append(d, n);
        return true;
    }));
    QCOMPARE(got, QByteArray("shared data"));
}

void ArchiveTest::permissionsAndOddEntries()
{
    const QString root = m_dir + QStringLiteral("/x/perms");
    QVERIFY(QDir().mkpath(root));
    Reader reader(fixture(QStringLiteral("perms.tar"),
                          Fixtures::tar({file(QStringLiteral("suid"), "x", 04755),
                                         file(QStringLiteral("locked"), "y", 0000),
                                         file(QStringLiteral("open"), "w", 0666),
                                         fifo(QStringLiteral("pipe")),
                                         folder(QStringLiteral("dir"), 0000),
                                         file(QStringLiteral("dir/in.txt"), "z")})));
    QVERIFY2(reader.extractAll(root), qPrintable(reader.detail()));
    struct stat st;
    QCOMPARE(::stat(QFile::encodeName(root + QStringLiteral("/suid")).constData(), &st), 0);
    QCOMPARE(st.st_mode & 07777, mode_t(0755));
    QCOMPARE(::stat(QFile::encodeName(root + QStringLiteral("/locked")).constData(), &st), 0);
    QCOMPARE(st.st_mode & 0777, mode_t(0600));
    // nobody else gets to write
    QCOMPARE(::stat(QFile::encodeName(root + QStringLiteral("/open")).constData(), &st), 0);
    QCOMPARE(st.st_mode & 0777, mode_t(0644));
    QCOMPARE(::stat(QFile::encodeName(root + QStringLiteral("/dir")).constData(), &st), 0);
    QCOMPARE(st.st_mode & 0777, mode_t(0700));
    QCOMPARE(readAll(root + QStringLiteral("/dir/in.txt")), QByteArray("z"));
    QVERIFY(!QFileInfo::exists(root + QStringLiteral("/pipe")));
}

void ArchiveTest::duplicateNamesLaterWins()
{
    const QString root = m_dir + QStringLiteral("/x/dup");
    QVERIFY(QDir().mkpath(root));
    Reader reader(fixture(QStringLiteral("dup.tar"), Fixtures::tar({file(QStringLiteral("a.txt"), "first"), file(QStringLiteral("a.txt"), "second")})));
    QVERIFY(reader.extractAll(root));
    QCOMPARE(readAll(root + QStringLiteral("/a.txt")), QByteArray("second"));
}

void ArchiveTest::bomb()
{
    // 3 GB from a 2 KB file: a sparse entry declares the size with no data
    const qint64 big = qint64(3) * 1024 * 1024 * 1024;
    const QString path = fixture(QStringLiteral("bomb.tar"), Fixtures::tar({sparse(QStringLiteral("zeros.bin"), big)}));
    const QString root = m_dir + QStringLiteral("/x/bomb");
    QVERIFY(QDir().mkpath(root));
    Reader reader(path);
    QVERIFY(!reader.extractAll(root));
    QCOMPARE(reader.error(), Error::TooLarge);

    // the user said go on: it extracts (as a sparse file, no disk used)
    QDir(root).removeRecursively();
    QVERIFY(QDir().mkpath(root));
    reader.setGuardEnabled(false);
    QVERIFY2(reader.extractAll(root), qPrintable(reader.detail()));
    QCOMPARE(QFileInfo(root + QStringLiteral("/zeros.bin")).size(), big);
    QFile::remove(root + QStringLiteral("/zeros.bin"));
}

void ArchiveTest::cancel()
{
    const QString root = m_dir + QStringLiteral("/x/cancel");
    QVERIFY(QDir().mkpath(root));
    Reader reader(m_dir + QStringLiteral("/out/zip-store-links.zip"));
    QVERIFY(!reader.extractAll(root, [](qint64) {
        return false;
    }));
    QCOMPARE(reader.error(), Error::Cancelled);
}

void ArchiveTest::badFiles()
{
    Reader missing(m_dir + QStringLiteral("/nope.zip"));
    QList<Entry> entries;
    QVERIFY(!missing.list(&entries));
    QCOMPARE(missing.error(), Error::CantOpen);

    Reader noise(fixture(QStringLiteral("noise.zip"), QByteArray("this is not an archive at all").repeated(20)));
    QVERIFY(!noise.list(&entries));
    QCOMPARE(noise.error(), Error::NotAnArchive);

    Reader folder(m_dir);
    QVERIFY(!folder.list(&entries));
    QCOMPARE(folder.error(), Error::CantOpen);

    // a TAR cut in the middle of a file
    const QByteArray whole = Fixtures::tar({file(QStringLiteral("big.bin"), QByteArray(20000, 'b'))});
    const QString cut = fixture(QStringLiteral("cut.tar"), whole.left(5000));
    Reader cutReader(cut);
    const QString root = m_dir + QStringLiteral("/x/cut");
    QVERIFY(QDir().mkpath(root));
    QVERIFY(!cutReader.extractAll(root));
    QCOMPARE(cutReader.error(), Error::Corrupt);
}

void ArchiveTest::createRefusesExisting()
{
    Writer writer;
    const QString taken = m_dir + QStringLiteral("/out/zip-normal.zip");
    const QByteArray before = digest(taken);
    QVERIFY(!writer.create(taken, {m_source + QStringLiteral("/notes.txt")}, CreateOptions()));
    QCOMPARE(writer.error(), Error::WriteFailed);
    QCOMPARE(digest(taken), before);
}

void ArchiveTest::archiveInsideItsSource()
{
    // saved in the folder being compressed: it never holds a copy of itself
    const QString src = m_dir + QStringLiteral("/self");
    writeFile(src + QStringLiteral("/a.txt"), "a");
    const QString zip = src + QStringLiteral("/self.zip");
    Writer writer;
    QVERIFY(writer.create(zip, {src}, CreateOptions()));
    Reader reader(zip);
    QList<Entry> entries;
    QVERIFY(reader.list(&entries));
    QStringList paths;
    for (const Entry &e : std::as_const(entries)) {
        paths.append(e.path);
    }
    paths.sort();
    QCOMPARE(paths, (QStringList{QStringLiteral("self"), QStringLiteral("self/a.txt")}));
}

void ArchiveTest::rewriteZip()
{
    const QString zip = m_dir + QStringLiteral("/out/rewrite.zip");
    Writer writer;
    QVERIFY(writer.create(zip, {m_source}, CreateOptions()));
    QVERIFY(QFile::setPermissions(zip, QFile::ReadOwner | QFile::WriteOwner));

    const QString added = m_dir + QStringLiteral("/added.txt");
    writeFile(added, QByteArray("dropped in\n"));
    QVERIFY2(writer.rewriteZip(zip, {{QStringLiteral("Holiday/new/added.txt"), added, false}, {QStringLiteral("Holiday/newfolder"), {}, true}}, {}),
             qPrintable(writer.detail()));

    Reader reader(zip);
    QList<Entry> entries;
    QVERIFY(reader.list(&entries));
    QStringList paths;
    for (const Entry &e : std::as_const(entries)) {
        paths.append(e.path);
    }
    QVERIFY(paths.contains(QStringLiteral("Holiday/new/added.txt")));
    QVERIFY(paths.contains(QStringLiteral("Holiday/newfolder")));
    QVERIFY(paths.contains(QStringLiteral("Holiday/photos/1.jpg")));
    QByteArray got;
    QVERIFY(reader.read(QStringLiteral("Holiday/photos/1.jpg"), [&got](const char *d, qint64 n) {
        got.append(d, n);
        return true;
    }));
    QCOMPARE(QCryptographicHash::hash(got, QCryptographicHash::Sha256).toHex(), digest(m_source + QStringLiteral("/photos/1.jpg")));
    QCOMPARE(QFile::permissions(zip) & (QFile::ReadOwner | QFile::WriteOwner | QFile::ReadOther), QFile::ReadOwner | QFile::WriteOwner);

    // over an existing entry: refused unless asked
    QVERIFY(!writer.rewriteZip(zip, {{QStringLiteral("Holiday/notes.txt"), added, false}}, {}));
    QCOMPARE(writer.error(), Error::WriteFailed);
    QVERIFY(writer.rewriteZip(zip, {{QStringLiteral("Holiday/notes.txt"), added, false}}, {}, true));
    got.clear();
    QVERIFY(reader.read(QStringLiteral("Holiday/notes.txt"), [&got](const char *d, qint64 n) {
        got.append(d, n);
        return true;
    }));
    QCOMPARE(got, QByteArray("dropped in\n"));

    // removing a folder takes what is under it
    QVERIFY(writer.rewriteZip(zip, {}, {QStringLiteral("Holiday/photos")}));
    QVERIFY(reader.list(&entries));
    for (const Entry &e : std::as_const(entries)) {
        QVERIFY2(!e.path.startsWith(QLatin1String("Holiday/photos")), qPrintable(e.path));
    }
    QCOMPARE(QDir(m_dir + QStringLiteral("/out")).entryList(QDir::Hidden | QDir::Files).filter(QStringLiteral(".raye-archive")).size(), 0);

    // a protected ZIP cannot be rewritten without its password
    const QString secret = fixture(QStringLiteral("secret2.zip"), Fixtures::zip({file(QStringLiteral("p.txt"), "x")}, QByteArray("pw")));
    QVERIFY(!writer.rewriteZip(secret, {{QStringLiteral("q.txt"), added, false}}, {}));
    QCOMPARE(writer.error(), Error::Unsupported);

    // and a RAR is not a ZIP
    QVERIFY(!writer.rewriteZip(m_dir + QStringLiteral("/fixtures/old.rar"), {{QStringLiteral("q.txt"), added, false}}, {}));
    QCOMPARE(writer.error(), Error::NotAnArchive);

    // a path with ".." is never added
    QVERIFY(!writer.rewriteZip(zip, {{QStringLiteral("../escape.txt"), added, false}}, {}));
    QCOMPARE(writer.error(), Error::Unsafe);
}

void ArchiveTest::noExtension()
{
    // a ZIP saved without its name: the content decides, not the extension
    QVERIFY(QDir().mkpath(m_dir + QStringLiteral("/out")));
    const QString zip = m_dir + QStringLiteral("/out/plain.zip");
    Writer writer;
    QVERIFY(writer.create(zip, {m_source}, CreateOptions()));
    const QString bare = m_dir + QStringLiteral("/out/download");
    QVERIFY(QFile::rename(zip, bare));
    QCOMPARE(detectFormat(bare), QStringLiteral("zip"));

    Reader reader(bare);
    QVERIFY(Reader::available());
    const QString root = m_dir + QStringLiteral("/x/bare");
    QVERIFY(QDir().mkpath(root));
    QVERIFY2(reader.extractAll(root), qPrintable(reader.detail()));
    QCOMPARE(digest(root + QStringLiteral("/Holiday/photos/1.jpg")), digest(m_source + QStringLiteral("/photos/1.jpg")));
}

void ArchiveTest::rewriteSevenZip()
{
    CreateOptions options;
    options.format = Format::SevenZip;
    QVERIFY(QDir().mkpath(m_dir + QStringLiteral("/out")));
    const QString archive = m_dir + QStringLiteral("/out/rewrite.7z");
    Writer writer;
    QVERIFY2(writer.create(archive, {m_source}, options), qPrintable(writer.detail()));

    const QString added = m_dir + QStringLiteral("/added7.txt");
    writeFile(added, QByteArray("into the 7z\n"));
    QVERIFY2(writer.rewriteZip(archive, {{QStringLiteral("Holiday/added7.txt"), added, false}}, {QStringLiteral("Holiday/notes.txt")}),
             qPrintable(writer.detail()));

    Reader reader(archive);
    QList<Entry> entries;
    QVERIFY(reader.list(&entries));
    QStringList paths;
    for (const Entry &e : std::as_const(entries)) {
        paths.append(e.path);
    }
    QVERIFY(paths.contains(QStringLiteral("Holiday/added7.txt")));
    QVERIFY(!paths.contains(QStringLiteral("Holiday/notes.txt")));
    QByteArray got;
    QVERIFY(reader.read(QStringLiteral("Holiday/photos/1.jpg"), [&got](const char *d, qint64 n) {
        got.append(d, n);
        return true;
    }));
    QCOMPARE(QCryptographicHash::hash(got, QCryptographicHash::Sha256).toHex(), digest(m_source + QStringLiteral("/photos/1.jpg")));
}

QTEST_GUILESS_MAIN(ArchiveTest)

#include "test_archive.moc"
