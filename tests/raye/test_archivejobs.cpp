/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The archive jobs (explorer spec 3.12, 3.13): extract and compress in a
// thread with progress and cancel, all or nothing, and names already in
// the destination kept.

#include "archive/archivejobs.h"
#include "archive/archivereader.h"
#include "data/archives/fixtures.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <clocale>
#include <unistd.h>

using namespace Raye::Archive;

namespace
{
void writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QStringList hiddenLeftovers(const QString &dir)
{
    return QDir(dir).entryList(QDir::Hidden | QDir::AllEntries | QDir::NoDotAndDotDot).filter(QStringLiteral(".raye-"));
}
}

class ArchiveJobsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void createThenExtract();
    void extractKeepsExistingNames();
    void extractOverwrite();
    void refusedLeavesNothing();
    void passwordJob();
    void killStops();
    void listJob();
    void moveIntoNeverFollowsLinks();

private:
    QString archive(const QString &name, const QByteArray &data);
    QTemporaryDir m_temp;
    QString m_zip;
};

void ArchiveJobsTest::initTestCase()
{
    std::setlocale(LC_ALL, "C.UTF-8");
    QVERIFY(m_temp.isValid());
}

QString ArchiveJobsTest::archive(const QString &name, const QByteArray &data)
{
    const QString path = m_temp.path() + QStringLiteral("/archives/") + name;
    writeFile(path, data);
    return path;
}

void ArchiveJobsTest::createThenExtract()
{
    const QString src = m_temp.path() + QStringLiteral("/src");
    writeFile(src + QStringLiteral("/Report.docx"), QByteArray("report ").repeated(5000));
    writeFile(src + QStringLiteral("/Photos/a.jpg"), QByteArray(70000, 'p'));

    m_zip = m_temp.path() + QStringLiteral("/Report.zip");
    auto *create = new ArchiveCreateJob({src + QStringLiteral("/Report.docx"), src + QStringLiteral("/Photos")}, m_zip, CreateOptions());
    QSignalSpy described(create, &KJob::description);
    QVERIFY2(create->exec(), qPrintable(create->errorString()));
    QCOMPARE(described.size(), 1);
    QCOMPARE(described.first().at(1).toString(), QStringLiteral("Compressing"));
    QVERIFY(QFileInfo(m_zip).size() > 0);
    QCOMPARE(create->totalAmount(KJob::Bytes), qulonglong(35000 + 70000));
    QCOMPARE(create->processedAmount(KJob::Bytes), create->totalAmount(KJob::Bytes));

    const QString dest = m_temp.path() + QStringLiteral("/out/Report");
    auto *extract = new ArchiveExtractJob(m_zip, dest);
    QSignalSpy extractDescribed(extract, &KJob::description);
    QVERIFY2(extract->exec(), qPrintable(extract->errorString()));
    QCOMPARE(extractDescribed.first().at(1).toString(), QStringLiteral("Extracting"));
    QCOMPARE(readAll(dest + QStringLiteral("/Report.docx")), QByteArray("report ").repeated(5000));
    QCOMPARE(readAll(dest + QStringLiteral("/Photos/a.jpg")), QByteArray(70000, 'p'));
    QCOMPARE(hiddenLeftovers(dest), QStringList());
    QStringList items = extract->extractedItems();
    items.sort();
    QCOMPARE(items, (QStringList{dest + QStringLiteral("/Photos"), dest + QStringLiteral("/Report.docx")}));
    // progress counts the bytes written out, like a copy
    QCOMPARE(extract->totalAmount(KJob::Bytes), qulonglong(35000 + 70000));
    QCOMPARE(extract->processedAmount(KJob::Bytes), extract->totalAmount(KJob::Bytes));
}

void ArchiveJobsTest::extractKeepsExistingNames()
{
    const QString dest = m_temp.path() + QStringLiteral("/out/Report");
    writeFile(dest + QStringLiteral("/Photos/mine.jpg"), "mine");
    auto *again = new ArchiveExtractJob(m_zip, dest);
    QVERIFY(again->exec());
    // the old file stays, the new one gets " (2)"; folders meet
    QVERIFY(QFile::exists(dest + QStringLiteral("/Report.docx")));
    QCOMPARE(readAll(dest + QStringLiteral("/Report (2).docx")), QByteArray("report ").repeated(5000));
    QCOMPARE(readAll(dest + QStringLiteral("/Photos/mine.jpg")), QByteArray("mine"));
    QVERIFY(QFile::exists(dest + QStringLiteral("/Photos/a (2).jpg")));
    QVERIFY(!QFile::exists(dest + QStringLiteral("/Photos (2)")));
}

void ArchiveJobsTest::extractOverwrite()
{
    const QString dest = m_temp.path() + QStringLiteral("/out/over");
    writeFile(dest + QStringLiteral("/Report.docx"), "old");
    auto *job = new ArchiveExtractJob(m_zip, dest);
    job->setOverwrite(true);
    QVERIFY(job->exec());
    QCOMPARE(readAll(dest + QStringLiteral("/Report.docx")), QByteArray("report ").repeated(5000));
    QVERIFY(!QFile::exists(dest + QStringLiteral("/Report (2).docx")));
}

void ArchiveJobsTest::refusedLeavesNothing()
{
    const QString evil = archive(QStringLiteral("evil.tar"),
                                 Fixtures::tar({Fixtures::file(QStringLiteral("fine.txt"), "fine"), Fixtures::file(QStringLiteral("../evil.txt"), "pwned")}));
    const QString dest = m_temp.path() + QStringLiteral("/out/evil");
    auto *job = new ArchiveExtractJob(evil, dest);
    QVERIFY(!job->exec());
    QCOMPARE(job->archiveError(), Error::Unsafe);
    QCOMPARE(job->error(), int(KJob::UserDefinedError) + int(Error::Unsafe));
    QVERIFY(job->errorText().contains(QLatin1String("can't be extracted safely")));
    // the folder it made is gone, nothing beside it either
    QVERIFY(!QFileInfo::exists(dest));
    QVERIFY(!QFileInfo::exists(m_temp.path() + QStringLiteral("/out/evil.txt")));

    // into a folder that was already there: it stays, empty of leftovers
    const QString existing = m_temp.path() + QStringLiteral("/out/existing");
    QVERIFY(QDir().mkpath(existing));
    auto *job2 = new ArchiveExtractJob(evil, existing);
    QVERIFY(!job2->exec());
    QVERIFY(QFileInfo(existing).isDir());
    QCOMPARE(QDir(existing).entryList(QDir::Hidden | QDir::AllEntries | QDir::NoDotAndDotDot), QStringList());
}

void ArchiveJobsTest::passwordJob()
{
    const QString secret = archive(QStringLiteral("secret.zip"), Fixtures::zip({Fixtures::file(QStringLiteral("plan.txt"), "the plan")}, QByteArray("raye")));
    const QString dest = m_temp.path() + QStringLiteral("/out/secret");
    auto *ask = new ArchiveExtractJob(secret, dest);
    QVERIFY(!ask->exec());
    QCOMPARE(ask->archiveError(), Error::PasswordNeeded);
    QCOMPARE(ask->errorText(), QStringLiteral("This archive is protected. Enter the password."));

    auto *ok = new ArchiveExtractJob(secret, dest);
    ok->setPassword(QStringLiteral("raye"));
    QVERIFY(ok->exec());
    QCOMPARE(readAll(dest + QStringLiteral("/plan.txt")), QByteArray("the plan"));
}

void ArchiveJobsTest::killStops()
{
    // big enough to still be running when the kill comes
    const QString src = m_temp.path() + QStringLiteral("/bigsrc");
    QByteArray noise(4 * 1024 * 1024, Qt::Uninitialized);
    for (int i = 0; i < noise.size(); ++i) {
        noise[i] = char((i * 2654435761u) >> 13);
    }
    for (int i = 0; i < 8; ++i) {
        writeFile(src + QStringLiteral("/f%1.bin").arg(i), noise);
    }
    const QString big = m_temp.path() + QStringLiteral("/big.tar.xz");
    CreateOptions options;
    options.format = Format::Tar;
    options.tarFilter = TarFilter::Xz;
    options.level = Level::Maximum;
    auto *create = new ArchiveCreateJob({src}, big, options);
    create->setAutoDelete(false);
    QSignalSpy result(create, &KJob::result);
    create->start();
    QTest::qWait(50);
    QVERIFY(create->kill(KJob::EmitResult));
    QCOMPARE(result.size(), 1);
    QCOMPARE(create->error(), int(KJob::KilledJobError));
    delete create;
    // the thread notices, stops and removes its half-written file
    QTRY_COMPARE_WITH_TIMEOUT(hiddenLeftovers(m_temp.path()), QStringList(), 20000);
    QVERIFY(!QFile::exists(big));
}

void ArchiveJobsTest::listJob()
{
    auto *job = new ArchiveListJob(m_zip);
    job->setAutoDelete(false);
    QVERIFY(job->exec());
    QStringList paths;
    for (const Entry &e : job->entries()) {
        paths.append(e.path);
    }
    QVERIFY(paths.contains(QStringLiteral("Photos/a.jpg")));
    QCOMPARE(job->skippedUnsafe(), 0);
    delete job;

    auto *missing = new ArchiveListJob(m_temp.path() + QStringLiteral("/none.zip"));
    QVERIFY(!missing->exec());
    QCOMPARE(missing->archiveError(), Error::CantOpen);
}

void ArchiveJobsTest::moveIntoNeverFollowsLinks()
{
    const QString base = m_temp.path() + QStringLiteral("/move");
    const QString staging = base + QStringLiteral("/staging");
    const QString dest = base + QStringLiteral("/dest");
    const QString elsewhere = base + QStringLiteral("/elsewhere");
    writeFile(staging + QStringLiteral("/docs/new.txt"), "new");
    QVERIFY(QDir().mkpath(dest));
    QVERIFY(QDir().mkpath(elsewhere));
    // "docs" in the destination is a link to a folder somewhere else
    QCOMPARE(::symlink(QFile::encodeName(elsewhere).constData(), QFile::encodeName(dest + QStringLiteral("/docs")).constData()), 0);

    QStringList placed;
    QVERIFY(moveInto(staging, dest, true, &placed));
    QVERIFY(!QFile::exists(elsewhere + QStringLiteral("/new.txt")));
    QCOMPARE(readAll(dest + QStringLiteral("/docs/new.txt")), QByteArray("new"));
    QVERIFY(!QFileInfo(dest + QStringLiteral("/docs")).isSymLink());
}

QTEST_GUILESS_MAIN(ArchiveJobsTest)

#include "test_archivejobs.moc"
