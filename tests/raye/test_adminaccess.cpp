/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Which failures Files offers again through kio-admin, and what it asks.

#include "raye/jobs/adminaccess.h"

#include <KIO/CopyJob>
#include <KIO/Global>
#include <KLocalizedString>

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace Raye::Jobs::AdminAccess;

class AdminAccessTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("raye-files");
    }

    void accessErrors_data()
    {
        QTest::addColumn<int>("error");
        QTest::addColumn<bool>("access");
        QTest::newRow("access denied") << int(KIO::ERR_ACCESS_DENIED) << true;
        QTest::newRow("write access denied") << int(KIO::ERR_WRITE_ACCESS_DENIED) << true;
        QTest::newRow("cannot enter folder") << int(KIO::ERR_CANNOT_ENTER_DIRECTORY) << true;
        QTest::newRow("cannot read") << int(KIO::ERR_CANNOT_OPEN_FOR_READING) << true;
        QTest::newRow("disk full") << int(KIO::ERR_DISK_FULL) << false;
        QTest::newRow("does not exist") << int(KIO::ERR_DOES_NOT_EXIST) << false;
        QTest::newRow("already exists") << int(KIO::ERR_FILE_ALREADY_EXIST) << false;
        QTest::newRow("cancelled") << int(KIO::ERR_USER_CANCELED) << false;
        QTest::newRow("no error") << 0 << false;
    }

    void accessErrors()
    {
        QFETCH(int, error);
        QFETCH(bool, access);
        QCOMPARE(isAccessError(error), access);
    }

    void adminUrls()
    {
        QCOMPARE(toAdmin(QUrl::fromLocalFile(QStringLiteral("/etc/fstab"))), QUrl(QStringLiteral("admin:/etc/fstab")));
        QCOMPARE(toAdmin(QUrl::fromLocalFile(QStringLiteral("/opt/my app/a b.conf"))).path(), QStringLiteral("/opt/my app/a b.conf"));
        QVERIFY(toAdmin(QUrl(QStringLiteral("smb://server/share/x"))).isEmpty());
        QVERIFY(toAdmin(QUrl(QStringLiteral("admin:/etc"))).isEmpty());
    }

    void writableForTheUi()
    {
        QVERIFY(allowsWrite(true, false, false));
        QVERIFY(allowsWrite(false, true, true));
        QVERIFY(!allowsWrite(false, true, false));
        QVERIFY(!allowsWrite(false, false, true));
    }

    void redo_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<QList<QUrl>>("sources");
        QTest::addColumn<QUrl>("destination");
        QTest::addColumn<int>("error");
        QTest::addColumn<bool>("admin");
        QTest::addColumn<bool>("offered");

        const QList<QUrl> home{QUrl::fromLocalFile(QStringLiteral("/home/ann/a.conf"))};
        const QUrl etc = QUrl::fromLocalFile(QStringLiteral("/etc"));
        QTest::newRow("copy into /etc") << int(Kind::Copy) << home << etc << int(KIO::ERR_WRITE_ACCESS_DENIED) << true << true;
        QTest::newRow("move into /etc") << int(Kind::Move) << home << etc << int(KIO::ERR_ACCESS_DENIED) << true << true;
        QTest::newRow("no kio-admin") << int(Kind::Copy) << home << etc << int(KIO::ERR_WRITE_ACCESS_DENIED) << false << false;
        QTest::newRow("disk full") << int(Kind::Copy) << home << etc << int(KIO::ERR_DISK_FULL) << true << false;
        QTest::newRow("to a network share") << int(Kind::Copy) << home << QUrl(QStringLiteral("smb://nas/share")) << int(KIO::ERR_ACCESS_DENIED) << true
                                            << false;
        QTest::newRow("from a network share") << int(Kind::Copy) << QList<QUrl>{QUrl(QStringLiteral("sftp://host/x"))} << etc
                                              << int(KIO::ERR_ACCESS_DENIED) << true << false;
        QTest::newRow("delete in /etc") << int(Kind::Delete) << QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("/etc/x"))} << QUrl()
                                        << int(KIO::ERR_ACCESS_DENIED) << true << true;
        QTest::newRow("rename in /opt") << int(Kind::Rename) << QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("/opt/a"))}
                                        << QUrl::fromLocalFile(QStringLiteral("/opt/b")) << int(KIO::ERR_ACCESS_DENIED) << true << true;
        QTest::newRow("not remembered") << int(Kind::None) << home << etc << int(KIO::ERR_ACCESS_DENIED) << true << false;
        QTest::newRow("nothing to redo") << int(Kind::Copy) << QList<QUrl>{} << etc << int(KIO::ERR_ACCESS_DENIED) << true << false;
    }

    void redo()
    {
        QFETCH(int, kind);
        QFETCH(QList<QUrl>, sources);
        QFETCH(QUrl, destination);
        QFETCH(int, error);
        QFETCH(bool, admin);
        QFETCH(bool, offered);
        QCOMPARE(canRedo(Operation{Kind(kind), sources, destination}, error, admin), offered);
    }

    void redoneAsAdmin()
    {
        const Operation op{Kind::Move,
                           {QUrl::fromLocalFile(QStringLiteral("/home/ann/a")), QUrl::fromLocalFile(QStringLiteral("/home/ann/b"))},
                           QUrl::fromLocalFile(QStringLiteral("/opt/app"))};
        const Operation admin = asAdmin(op);
        QCOMPARE(admin.kind, Kind::Move);
        QCOMPARE(admin.sources, (QList<QUrl>{QUrl(QStringLiteral("admin:/home/ann/a")), QUrl(QStringLiteral("admin:/home/ann/b"))}));
        QCOMPARE(admin.destination, QUrl(QStringLiteral("admin:/opt/app")));
        QVERIFY(asAdmin(Operation{Kind::Delete, {QUrl::fromLocalFile(QStringLiteral("/etc/x"))}, {}}).destination.isEmpty());
    }

    void rememberedOnTheJob()
    {
        QTemporaryDir from;
        QTemporaryDir to;
        QFile f(from.filePath(QStringLiteral("a.txt")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        KIO::CopyJob *job = KIO::move({QUrl::fromLocalFile(f.fileName())}, QUrl::fromLocalFile(to.path()), KIO::HideProgressInfo);
        job->setAutoDelete(false);
        const Operation op = copyOperation(job);
        QCOMPARE(op.kind, Kind::Move);
        QCOMPARE(op.sources, QList<QUrl>{QUrl::fromLocalFile(f.fileName())});
        QCOMPARE(op.destination, QUrl::fromLocalFile(to.path()));

        QCOMPARE(recall(job).kind, Kind::None);
        remember(job, op);
        const Operation back = recall(job);
        QCOMPARE(back.kind, op.kind);
        QCOMPARE(back.sources, op.sources);
        QCOMPARE(back.destination, op.destination);
        job->kill();
        delete job;
    }

    void newItemsInAReadOnlyFolder()
    {
        QTemporaryDir dir;
        const QUrl url = QUrl::fromLocalFile(dir.path());
        QCOMPARE(newItemFolder(url), url);
        QVERIFY(QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::ExeOwner));
        if (::geteuid() != 0) {
            QCOMPARE(newItemFolder(url), available() ? toAdmin(url) : url);
        }
        QVERIFY(QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QUrl remote(QStringLiteral("sftp://host/srv"));
        QCOMPARE(newItemFolder(remote), remote);
    }

    void questions()
    {
        const Question open = openQuestion(false);
        QVERIFY(open.message.contains(QStringLiteral("this file")));
        QVERIFY(open.skipText.isEmpty());
        QVERIFY(openQuestion(true).message.contains(QStringLiteral("this folder")));

        const QUrl etc = QUrl::fromLocalFile(QStringLiteral("/etc"));
        const Question copy = redoQuestion(Operation{Kind::Copy, {}, etc}, false, true);
        QCOMPARE(copy.title, QStringLiteral("Destination Folder Access Denied"));
        QCOMPARE(copy.message, QStringLiteral("You'll need to provide administrator permission to copy to this folder."));
        QCOMPARE(copy.continueText, QStringLiteral("Continue"));
        QCOMPARE(copy.skipText, QStringLiteral("Skip"));
        QCOMPARE(copy.cancelText, QStringLiteral("Cancel"));

        QVERIFY(redoQuestion(Operation{Kind::Delete, {}, {}}, true, false).message.endsWith(QStringLiteral("delete this folder.")));
        QVERIFY(redoQuestion(Operation{Kind::Delete, {}, {}}, false, true).message.endsWith(QStringLiteral("delete these items.")));
        QCOMPARE(redoQuestion(Operation{Kind::Delete, {}, {}}, true, false).title, QStringLiteral("Folder Access Denied"));
        // One item renamed or one folder made: nothing to skip.
        QVERIFY(redoQuestion(Operation{Kind::Rename, {}, {}}, false, false).skipText.isEmpty());
        QVERIFY(redoQuestion(Operation{Kind::NewFolder, {}, etc}, true, false).skipText.isEmpty());
    }
};

QTEST_MAIN(AdminAccessTest)

#include "test_adminaccess.moc"
