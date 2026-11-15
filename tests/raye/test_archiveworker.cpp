/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The rayearchive: worker (explorer spec 3.12): the tree it shows, links
// kept inside the archive, and the worker itself run by KIO the way the
// view, drag and drop and the Open dialogs use it: list, read, add to a
// ZIP, take out of it, and the refusal on a RAR.

#include "archive/archivereader.h"
#include "archive/archiveurl.h"
#include "archive/archivewriter.h"
#include "data/archives/fixtures.h"
#include "kioworker/archiveindex.h"

#include <KIO/DeleteJob>
#include <KIO/ListJob>
#include <KIO/MkdirJob>
#include <KIO/StatJob>
#include <KIO/StoredTransferJob>
#include <KProtocolInfo>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <clocale>
#include <sys/stat.h>

using namespace Raye::Archive;

namespace
{
Entry make(const QString &path, Entry::Type type = Entry::File, const QString &link = QString())
{
    Entry e;
    e.path = path;
    e.type = type;
    e.linkTarget = link;
    e.size = type == Entry::File ? 10 : -1;
    return e;
}

QStringList names(const QList<Entry> &entries)
{
    QStringList out;
    for (const Entry &e : entries) {
        out.append(e.path);
    }
    out.sort();
    return out;
}

QStringList listNames(const QUrl &url, QString *error = nullptr)
{
    QStringList out;
    KIO::ListJob *job = KIO::listDir(url, KIO::HideProgressInfo);
    QObject::connect(job, &KIO::ListJob::entries, job, [&out](KIO::Job *, const KIO::UDSEntryList &list) {
        for (const KIO::UDSEntry &e : list) {
            const QString name = e.stringValue(KIO::UDSEntry::UDS_NAME);
            if (name != QLatin1String(".")) {
                out.append(name + (e.isDir() ? QStringLiteral("/") : QString()));
            }
        }
    });
    if (!job->exec() && error) {
        *error = job->errorString();
    }
    out.sort();
    return out;
}
}

class ArchiveWorkerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void impliedFolders();
    void firstEntryWins();
    void links();
    void canAddAt();
    void uds();
    void workerList();
    void workerGetAndStat();
    void workerAddAndRemove();
    void workerRefusesRar();
    void workerPasswordCancelled();

private:
    bool workerAvailable() const;
    QTemporaryDir m_temp;
    QString m_zip;
};

void ArchiveWorkerTest::initTestCase()
{
    std::setlocale(LC_ALL, "C.UTF-8");
    QVERIFY(m_temp.isValid());
    // The worker this build made, not one installed on the system.
    QCoreApplication::addLibraryPath(qEnvironmentVariable("RAYE_FILES_BINARY_DIR", QStringLiteral(RAYE_FILES_BINARY_DIR)) + QStringLiteral("/bin"));
}

bool ArchiveWorkerTest::workerAvailable() const
{
    // Files always builds the worker: not finding it then
    // is a failure, not a reason to skip.
    if (Reader::available() && qEnvironmentVariableIsSet("RAYE_TESTS_STRICT")) {
        return true;
    }
    return Reader::available() && KProtocolInfo::isKnownProtocol(QStringLiteral("rayearchive"));
}

void ArchiveWorkerTest::impliedFolders()
{
    ArchiveIndex index;
    index.build({make(QStringLiteral("a/b/c.txt")), make(QStringLiteral("top.txt")), make(QStringLiteral("a/d"), Entry::Folder)});
    QCOMPARE(names(index.children(QString())), (QStringList{QStringLiteral("a"), QStringLiteral("top.txt")}));
    QCOMPARE(names(index.children(QStringLiteral("a"))), (QStringList{QStringLiteral("a/b"), QStringLiteral("a/d")}));
    QCOMPARE(names(index.children(QStringLiteral("a/b"))), QStringList{QStringLiteral("a/b/c.txt")});
    QVERIFY(index.find(QStringLiteral("a"))->type == Entry::Folder);
    QVERIFY(index.find(QString())->type == Entry::Folder);
    QVERIFY(!index.find(QStringLiteral("nope")));
    QVERIFY(index.children(QStringLiteral("top.txt")).isEmpty());

    // a real folder entry that comes after its files keeps its details
    Entry real = make(QStringLiteral("a"), Entry::Folder);
    real.permissions = 0700;
    index.build({make(QStringLiteral("a/x")), real});
    QCOMPARE(index.find(QStringLiteral("a"))->permissions, 0700u);
    QCOMPARE(names(index.children(QString())), QStringList{QStringLiteral("a")});
}

void ArchiveWorkerTest::firstEntryWins()
{
    ArchiveIndex index;
    Entry first = make(QStringLiteral("x.txt"));
    first.size = 1;
    Entry second = make(QStringLiteral("x.txt"));
    second.size = 2;
    index.build({first, second});
    QCOMPARE(index.find(QStringLiteral("x.txt"))->size, 1);
    QCOMPARE(index.children(QString()).size(), 1);
}

void ArchiveWorkerTest::links()
{
    ArchiveIndex index;
    index.build({make(QStringLiteral("lib/libx.so.1")),
                 make(QStringLiteral("lib/libx.so"), Entry::Symlink, QStringLiteral("libx.so.1")),
                 make(QStringLiteral("docs"), Entry::Folder),
                 make(QStringLiteral("docs/a.txt")),
                 make(QStringLiteral("d"), Entry::Symlink, QStringLiteral("docs")),
                 make(QStringLiteral("out"), Entry::Symlink, QStringLiteral("../../etc")),
                 make(QStringLiteral("abs"), Entry::Symlink, QStringLiteral("/etc")),
                 make(QStringLiteral("loop1"), Entry::Symlink, QStringLiteral("loop2")),
                 make(QStringLiteral("loop2"), Entry::Symlink, QStringLiteral("loop1"))});
    QString r;
    QVERIFY(index.resolve(QStringLiteral("lib/libx.so"), &r));
    QCOMPARE(r, QStringLiteral("lib/libx.so.1"));
    QVERIFY(index.resolve(QStringLiteral("d/a.txt"), &r));
    QCOMPARE(r, QStringLiteral("docs/a.txt"));
    QVERIFY(index.resolve(QStringLiteral("d"), &r));
    QCOMPARE(r, QStringLiteral("docs"));
    QVERIFY(index.resolve(QString(), &r));
    QCOMPARE(r, QString());
    QVERIFY(index.resolve(QStringLiteral("docs/.."), &r));
    QCOMPARE(r, QString());
    QVERIFY(index.resolve(QStringLiteral("docs/new.txt"), &r));
    QCOMPARE(r, QStringLiteral("docs/new.txt"));
    QVERIFY(!index.resolve(QStringLiteral("out"), &r));
    QVERIFY(!index.resolve(QStringLiteral("out/passwd"), &r));
    QVERIFY(!index.resolve(QStringLiteral("abs/passwd"), &r));
    QVERIFY(!index.resolve(QStringLiteral(".."), &r));
    QVERIFY(!index.resolve(QStringLiteral("loop1"), &r));
    QVERIFY(!index.resolve(QStringLiteral("missing/deeper"), &r));
}

void ArchiveWorkerTest::canAddAt()
{
    ArchiveIndex index;
    index.build({make(QStringLiteral("docs/a.txt")), make(QStringLiteral("file.txt")), make(QStringLiteral("l"), Entry::Symlink, QStringLiteral("docs"))});
    QVERIFY(index.canAddAt(QStringLiteral("new.txt")));
    QVERIFY(index.canAddAt(QStringLiteral("docs/new.txt")));
    QVERIFY(index.canAddAt(QStringLiteral("brand/new/folder.txt")));
    QVERIFY(!index.canAddAt(QStringLiteral("file.txt/inside")));
    QVERIFY(!index.canAddAt(QStringLiteral("l/through.txt")));
    QVERIFY(!index.canAddAt(QStringLiteral("../escape")));
    QVERIFY(!index.canAddAt(QStringLiteral("docs//x")));
    QVERIFY(!index.canAddAt(QString()));
}

void ArchiveWorkerTest::uds()
{
    ArchiveIndex index;
    Entry file = make(QStringLiteral("docs/a.txt"));
    file.size = 1234;
    file.permissions = 0640;
    file.modified = QDateTime::fromSecsSinceEpoch(1700000000);
    index.build({file, make(QStringLiteral("d"), Entry::Symlink, QStringLiteral("docs")), make(QStringLiteral("broken"), Entry::Symlink, QStringLiteral("nowhere"))});

    KIO::UDSEntry u = udsEntry(index, *index.find(QStringLiteral("docs/a.txt")), QString());
    QCOMPARE(u.stringValue(KIO::UDSEntry::UDS_NAME), QStringLiteral("a.txt"));
    QCOMPARE(u.numberValue(KIO::UDSEntry::UDS_SIZE), 1234);
    QCOMPARE(u.numberValue(KIO::UDSEntry::UDS_ACCESS), 0640);
    QCOMPARE(u.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME), 1700000000);
    QVERIFY(!u.isDir());

    u = udsEntry(index, *index.find(QStringLiteral("docs")), QString());
    QVERIFY(u.isDir());
    QCOMPARE(u.stringValue(KIO::UDSEntry::UDS_MIME_TYPE), QStringLiteral("inode/directory"));

    u = udsEntry(index, *index.find(QStringLiteral("d")), QString());
    QVERIFY(u.isDir());
    QVERIFY(u.isLink());
    QCOMPARE(u.stringValue(KIO::UDSEntry::UDS_LINK_DEST), QStringLiteral("docs"));

    u = udsEntry(index, *index.find(QStringLiteral("broken")), QString());
    QVERIFY(!u.isDir());

    u = udsEntry(index, *index.find(QString()), QStringLiteral("Holiday.zip"));
    QCOMPARE(u.stringValue(KIO::UDSEntry::UDS_NAME), QStringLiteral("Holiday.zip"));
    QVERIFY(u.isDir());
}

void ArchiveWorkerTest::workerList()
{
    if (!workerAvailable()) {
        QSKIP("the rayearchive worker is not built here");
    }
    const QString src = m_temp.path() + QStringLiteral("/src/Docs");
    QVERIFY(QDir().mkpath(src + QStringLiteral("/sub")));
    QFile a(src + QStringLiteral("/a.txt"));
    QVERIFY(a.open(QIODevice::WriteOnly));
    a.write("hello\n");
    a.close();
    QFile b(src + QStringLiteral("/sub/Été.txt"));
    QVERIFY(b.open(QIODevice::WriteOnly));
    b.write("deep\n");
    b.close();
    m_zip = m_temp.path() + QStringLiteral("/My files #1.zip");
    Writer writer;
    QVERIFY(writer.create(m_zip, {src}, CreateOptions()));

    QString error;
    QCOMPARE(listNames(entryUrl(m_zip, QString()), &error), QStringList{QStringLiteral("Docs/")});
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(listNames(entryUrl(m_zip, QStringLiteral("Docs"))), (QStringList{QStringLiteral("a.txt"), QStringLiteral("sub/")}));
    QCOMPARE(listNames(entryUrl(m_zip, QStringLiteral("Docs/sub"))), QStringList{QStringLiteral("Été.txt")});

    // browsing into a file that is not an archive fails cleanly
    QFile notArchive(m_temp.path() + QStringLiteral("/plain.zip"));
    QVERIFY(notArchive.open(QIODevice::WriteOnly));
    notArchive.write(QByteArray("not a zip").repeated(30));
    notArchive.close();
    error.clear();
    listNames(entryUrl(notArchive.fileName(), QString()), &error);
    QVERIFY(!error.isEmpty());
}

void ArchiveWorkerTest::workerGetAndStat()
{
    if (!workerAvailable()) {
        QSKIP("the rayearchive worker is not built here");
    }
    KIO::StoredTransferJob *get = KIO::storedGet(entryUrl(m_zip, QStringLiteral("Docs/sub/Été.txt")), KIO::NoReload, KIO::HideProgressInfo);
    QVERIFY2(get->exec(), qPrintable(get->errorString()));
    QCOMPARE(get->data(), QByteArray("deep\n"));

    KIO::StatJob *stat = KIO::stat(entryUrl(m_zip, QStringLiteral("Docs/a.txt")), KIO::HideProgressInfo);
    QVERIFY(stat->exec());
    QCOMPARE(stat->statResult().numberValue(KIO::UDSEntry::UDS_SIZE), 6);
    QVERIFY(!stat->statResult().isDir());

    KIO::StatJob *root = KIO::stat(entryUrl(m_zip, QString()), KIO::HideProgressInfo);
    QVERIFY(root->exec());
    QVERIFY(root->statResult().isDir());

    KIO::StatJob *missing = KIO::stat(entryUrl(m_zip, QStringLiteral("Docs/nope")), KIO::HideProgressInfo);
    QVERIFY(!missing->exec());
    QCOMPARE(missing->error(), int(KIO::ERR_DOES_NOT_EXIST));
}

void ArchiveWorkerTest::workerAddAndRemove()
{
    if (!workerAvailable()) {
        QSKIP("the rayearchive worker is not built here");
    }
    // drag in: a folder, then a file into it
    KIO::SimpleJob *mk = KIO::mkdir(entryUrl(m_zip, QStringLiteral("Added")));
    QVERIFY2(mk->exec(), qPrintable(mk->errorString()));
    KIO::StoredTransferJob *put = KIO::storedPut(QByteArray("dropped\n"), entryUrl(m_zip, QStringLiteral("Added/new.txt")), -1, KIO::HideProgressInfo);
    QVERIFY2(put->exec(), qPrintable(put->errorString()));
    QCOMPARE(listNames(entryUrl(m_zip, QStringLiteral("Added"))), QStringList{QStringLiteral("new.txt")});

    // the same name again asks first; with Overwrite it replaces
    KIO::StoredTransferJob *again = KIO::storedPut(QByteArray("x"), entryUrl(m_zip, QStringLiteral("Added/new.txt")), -1, KIO::HideProgressInfo);
    QVERIFY(!again->exec());
    QCOMPARE(again->error(), int(KIO::ERR_FILE_ALREADY_EXIST));
    KIO::StoredTransferJob *replace = KIO::storedPut(QByteArray("v2\n"), entryUrl(m_zip, QStringLiteral("Added/new.txt")), -1, KIO::Overwrite | KIO::HideProgressInfo);
    QVERIFY(replace->exec());
    KIO::StoredTransferJob *get = KIO::storedGet(entryUrl(m_zip, QStringLiteral("Added/new.txt")), KIO::NoReload, KIO::HideProgressInfo);
    QVERIFY(get->exec());
    QCOMPARE(get->data(), QByteArray("v2\n"));

    // drag out with move: the entry goes
    KIO::DeleteJob *del = KIO::del(entryUrl(m_zip, QStringLiteral("Docs/a.txt")), KIO::HideProgressInfo);
    QVERIFY2(del->exec(), qPrintable(del->errorString()));
    QCOMPARE(listNames(entryUrl(m_zip, QStringLiteral("Docs"))), QStringList{QStringLiteral("sub/")});

    // the archive is still a good ZIP for anyone else
    Reader reader(m_zip);
    QList<Entry> entries;
    QVERIFY(reader.list(&entries));
    QVERIFY(reader.read(QStringLiteral("Docs/sub/Été.txt"), [](const char *, qint64) {
        return true;
    }));
}

void ArchiveWorkerTest::workerRefusesRar()
{
    if (!workerAvailable()) {
        QSKIP("the rayearchive worker is not built here");
    }
    const QString rar = m_temp.path() + QStringLiteral("/old.rar");
    QFile f(rar);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(Fixtures::rar({Fixtures::file(QStringLiteral("readme.txt"), "from a rar\n")}));
    f.close();
    QCOMPARE(listNames(entryUrl(rar, QString())), QStringList{QStringLiteral("readme.txt")});
    KIO::StoredTransferJob *put = KIO::storedPut(QByteArray("x"), entryUrl(rar, QStringLiteral("new.txt")), -1, KIO::HideProgressInfo);
    QVERIFY(!put->exec());
    QVERIFY2(put->errorString().contains(QLatin1String("Files can't add to RAR archives.")), qPrintable(put->errorString()));
}

void ArchiveWorkerTest::workerPasswordCancelled()
{
    if (!workerAvailable()) {
        QSKIP("the rayearchive worker is not built here");
    }
    const QString zip = m_temp.path() + QStringLiteral("/secret.zip");
    QFile f(zip);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(Fixtures::zip({Fixtures::file(QStringLiteral("plan.txt"), "the plan\n")}, QByteArray("raye")));
    f.close();
    // listing needs no password; reading asks, and with nobody to answer
    // (no password dialog in a test) it ends as cancelled, never as data
    QCOMPARE(listNames(entryUrl(zip, QString())), QStringList{QStringLiteral("plan.txt")});
    KIO::StoredTransferJob *get = KIO::storedGet(entryUrl(zip, QStringLiteral("plan.txt")), KIO::NoReload, KIO::HideProgressInfo);
    get->setUiDelegate(nullptr);
    QVERIFY(!get->exec());
    QVERIFY(get->data().isEmpty());
}

QTEST_GUILESS_MAIN(ArchiveWorkerTest)

#include "test_archiveworker.moc"
