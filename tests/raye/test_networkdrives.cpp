/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Network drives: reading \\server\share, what the root helper accepts,
// the kept list, errors in plain words, and the Map network drive window.

#include "netdrive/helperrules.h"
#include "netdrive/mapdrivedialog.h"
#include "netdrive/networkdrives.h"

#include <KLocalizedString>

#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace Raye::NetDrives;

class NetworkDrivesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setLanguages({QStringLiteral("en")});
    }

    void parsesAddresses()
    {
        Address a = parseAddress(QStringLiteral("\\\\nas\\photos\\2026\\June"));
        QCOMPARE(a.server, QStringLiteral("nas"));
        QCOMPARE(a.share, QStringLiteral("photos"));
        QCOMPARE(a.path, QStringLiteral("2026/June"));
        QCOMPARE(parseAddress(QStringLiteral("//192.168.50.222/data")), (Address{QStringLiteral("192.168.50.222"), QStringLiteral("data"), QString()}));
        a = parseAddress(QStringLiteral("smb://alex@nas/photos/a"));
        QCOMPARE(a.share, QStringLiteral("photos"));
        QCOMPARE(a.path, QStringLiteral("a"));
        QCOMPARE(userOf(QStringLiteral("smb://alex@nas/photos")), QStringLiteral("alex"));
        QCOMPARE(parseAddress(QStringLiteral("  \\\\nas\\photos  ")).share, QStringLiteral("photos"));
    }

    void refusesWhatIsNotAShare()
    {
        QVERIFY(!parseAddress(QStringLiteral("\\\\nas")).isValid());
        QVERIFY(!parseAddress(QStringLiteral("/home/alex/photos")).isValid());
        QVERIFY(!parseAddress(QStringLiteral("nas\\photos")).isValid());
        QVERIFY(!parseAddress(QStringLiteral("sftp://nas/photos")).isValid());
        QVERIFY(!parseAddress(QStringLiteral("\\\\bad host\\photos")).isValid());
        QVERIFY(!parseAddress(QStringLiteral("\\\\nas\\a,uid=0")).isValid());
        QVERIFY(!parseAddress(QStringLiteral("\\\\-o\\photos")).isValid());
    }

    void writesAddresses()
    {
        const Address a{QStringLiteral("nas"), QStringLiteral("photos"), QStringLiteral("2026/June")};
        QCOMPARE(uncText(a), QStringLiteral("\\\\nas\\photos\\2026\\June"));
        QCOMPARE(smbUrl(a), QUrl(QStringLiteral("smb://nas/photos/2026/June")));
        QCOMPARE(defaultMountPoint(QStringLiteral("/home/alex"), QStringLiteral("photos")), QStringLiteral("/home/alex/Network/photos"));
    }

    void namesProtocols()
    {
        QCOMPARE(protocolName(QStringLiteral("cifs")), QStringLiteral("SMB"));
        QCOMPARE(protocolName(QStringLiteral("smb")), QStringLiteral("SMB"));
        QCOMPARE(protocolName(QStringLiteral("nfs4")), QStringLiteral("NFS"));
        QCOMPARE(protocolName(QStringLiteral("fuse.sshfs")), QStringLiteral("SFTP"));
        QCOMPARE(protocolName(QStringLiteral("webdavs")), QStringLiteral("WebDAV"));
        QCOMPARE(protocolName(QStringLiteral("ext4")), QString());
        QVERIFY(isNetworkFileSystem(QStringLiteral("cifs")));
        QVERIFY(!isNetworkFileSystem(QStringLiteral("smb")));
        QVERIFY(!isNetworkFileSystem(QStringLiteral("btrfs")));
    }

    void readsMountInfo()
    {
        const QByteArray info =
            "22 1 0:21 / / rw,relatime shared:1 - btrfs /dev/vda2 rw\n"
            "40 22 0:44 / /home/alex/Network/My\\040Photos rw,nosuid,nodev - cifs //nas/photos rw,vers=3.1.1\n"
            "41 22 0:45 / /mnt/home rw - nfs4 nas:/export/home rw\n";
        const QList<Mount> mounts = parseMountInfo(info);
        QCOMPARE(mounts.size(), 2);
        QCOMPARE(mounts.first().mountPoint, QStringLiteral("/home/alex/Network/My Photos"));
        QCOMPARE(mounts.first().source, QStringLiteral("//nas/photos"));
        QCOMPARE(mountAt(QStringLiteral("/home/alex/Network/My Photos/"), mounts).type, QStringLiteral("cifs"));
        QVERIFY(mountAt(QStringLiteral("/home/alex"), mounts).type.isEmpty());
        QCOMPARE(addressOfSource(QStringLiteral("//nas/photos")).share, QStringLiteral("photos"));
        QCOMPARE(addressOfSource(QStringLiteral("nas:/export/home")).server, QStringLiteral("nas"));
    }

    void keepsMappingsWithoutPassword()
    {
        QTemporaryDir dir;
        const QString rc = dir.filePath(QStringLiteral("raye-networkdrivesrc"));
        Mapping m;
        m.name = QStringLiteral("Photos");
        m.address = {QStringLiteral("nas"), QStringLiteral("photos"), QString()};
        m.mountPoint = QStringLiteral("/home/alex/Network/Photos/");
        m.user = QStringLiteral("alex");
        m.domain = QStringLiteral("HOME");
        m.remember = true;
        saveMapping(m, rc);
        const QList<Mapping> back = loadMappings(rc);
        QCOMPARE(back.size(), 1);
        QCOMPARE(back.first().mountPoint, QStringLiteral("/home/alex/Network/Photos"));
        QCOMPARE(back.first().user, QStringLiteral("alex"));
        QVERIFY(back.first().remember);
        QCOMPARE(mappingAt(QStringLiteral("/home/alex/Network/Photos"), rc).name, QStringLiteral("Photos"));
        QFile file(rc);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().toLower().contains("pass"));
        removeMapping(m.mountPoint, rc);
        QVERIFY(loadMappings(rc).isEmpty());
        QCOMPARE(walletKey(m), QStringLiteral("smb://alex@nas/photos"));
    }

    void explainsFailures()
    {
        QCOMPARE(failureOf(0, QString()), Failure::None);
        QCOMPARE(failureOf(4, QStringLiteral("mount error(13): Permission denied")), Failure::Credentials);
        QCOMPARE(failureOf(4, QStringLiteral("mount error(2): No such file or directory")), Failure::NotFound);
        QCOMPARE(failureOf(4, QStringLiteral("mount error(112): Host is down")), Failure::Unreachable);
        QCOMPARE(failureOf(4, QStringLiteral("mount error: could not resolve address for nas: Unknown error")), Failure::Unreachable);
        QCOMPARE(failureOf(126, QString()), Failure::NotAllowed);
        QCOMPARE(failureOf(3, QString()), Failure::MountPoint);
        QCOMPARE(failureOf(4, QStringLiteral("something else")), Failure::Other);
        const Address a{QStringLiteral("nas"), QStringLiteral("photos"), QString()};
        QVERIFY(failureText(Failure::Credentials, a).contains(QStringLiteral("\\\\nas\\photos")));
        QVERIFY(failureText(Failure::Unreachable, a).contains(QStringLiteral("\\\\nas")));
    }

    void helperChecksServerAndShare()
    {
        using namespace Raye::NetDrives::Helper;
        QVERIFY(validServer("nas"));
        QVERIFY(validServer("192.168.50.222"));
        QVERIFY(validServer("nas.home.lan"));
        QVERIFY(validServer("fe80::1"));
        QVERIFY(!validServer(""));
        QVERIFY(!validServer("-o"));
        QVERIFY(!validServer("a b"));
        QVERIFY(!validServer("a;b"));
        QVERIFY(!validServer("nas/photos"));
        QVERIFY(validShare("My Photos"));
        QVERIFY(!validShare("a/b"));
        QVERIFY(!validShare("a\\b"));
        QVERIFY(!validShare("a,uid=0"));
        QVERIFY(!validShare(".."));
        QVERIFY(validCredential(""));
        QVERIFY(validCredential("Alex Martin"));
        QVERIFY(!validCredential("alex,uid=0"));
        QVERIFY(!validCredential("a=b"));
        QVERIFY(!validCredential("a\nb"));
    }

    void helperMountsForTheCallerOnly()
    {
        using namespace Raye::NetDrives::Helper;
        const std::string guest = cifsOptions(1000, 1000, "", "");
        QVERIFY(guest.find("uid=1000,gid=1000") == 0);
        QVERIFY(guest.find(",guest") != std::string::npos);
        for (const char *must : {"nosuid", "nodev", "file_mode=0700", "dir_mode=0700", "forceuid"}) {
            QVERIFY2(guest.find(must) != std::string::npos, must);
        }
        const std::string user = cifsOptions(1000, 1000, "alex", "HOME");
        QVERIFY(user.find(",username=alex,domain=HOME") != std::string::npos);
        QVERIFY(user.find("guest") == std::string::npos);
        QVERIFY(user.find("password") == std::string::npos);
    }

    void helperChecksTheFolder()
    {
        using namespace Raye::NetDrives::Helper;
        QVERIFY(allowedMountPoint("/home/alex/Network/photos", "/home/alex", "alex"));
        QVERIFY(allowedMountPoint("/media/alex/photos", "/home/alex", "alex"));
        QVERIFY(!allowedMountPoint("/home/alex", "/home/alex", "alex"));
        QVERIFY(!allowedMountPoint("/home/alexander/x", "/home/alex", "alex"));
        QVERIFY(!allowedMountPoint("/media/alex", "/home/alex", "alex"));
        QVERIFY(!allowedMountPoint("/media/bob/x", "/home/alex", "alex"));
        QVERIFY(!allowedMountPoint("/etc/ssh", "/home/alex", "alex"));
        QVERIFY(!allowedMountPoint("/home/alex/../../etc", "/home/alex", "alex"));
        QVERIFY(!allowedMountPoint("/home/alex/Network/", "/home/alex", "alex"));
        QVERIFY(!allowedMountPoint("Network/x", "/home/alex", "alex"));
        QVERIFY(!allowedMountPoint("/x", "/", "alex"));
        QVERIFY(!allowedMountPoint("/media/../etc", "/home/alex", ".."));
    }

    void helperRefusesUnknownCallers()
    {
        const QString helper = QStringLiteral(RAYE_FILES_BINARY_DIR "/bin/raye-files-netdrive-helper");
        QVERIFY2(QFile::exists(helper), qPrintable(helper));
        const auto run = [&helper](const QString &pkexecUid) {
            QProcess p;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.remove(QStringLiteral("PKEXEC_UID"));
            if (!pkexecUid.isNull()) {
                env.insert(QStringLiteral("PKEXEC_UID"), pkexecUid);
            }
            p.setProcessEnvironment(env);
            p.start(helper, {QStringLiteral("mount"), QStringLiteral("nas"), QStringLiteral("photos"), QStringLiteral("/etc"), QString(), QString()});
            p.closeWriteChannel();
            p.waitForFinished();
            return p.exitCode();
        };
        // Not root, no caller, or root itself as the caller: refused before
        // anything is read or mounted.
        QCOMPARE(run(QString()), 2);
        QCOMPARE(run(QStringLiteral("0")), 2);
        QCOMPARE(run(QStringLiteral("abc")), 2);
    }

    void mapWindowFillsNameAndPlace()
    {
        MapDriveModel model(QStringLiteral("/home/alex"));
        model.setFolder(QStringLiteral("\\\\nas\\photos"));
        QCOMPARE(model.name(), QStringLiteral("photos"));
        QCOMPARE(model.location(), QStringLiteral("/home/alex/Network/photos"));
        model.setName(QStringLiteral("Holiday pictures"));
        QCOMPARE(model.location(), QStringLiteral("/home/alex/Network/Holiday pictures"));
        model.setFolder(QStringLiteral("\\\\nas\\music"));
        QCOMPARE(model.name(), QStringLiteral("Holiday pictures"));
        QVERIFY(model.canFinish());
        model.setFolder(QStringLiteral("nas"));
        QVERIFY(!model.canFinish());
    }

    void mapWindowConnects()
    {
        QTemporaryDir home;
        Mapping seen;
        QString seenPassword;
        MapDriveModel model(home.path(), [&](const Mapping &m, const QString &password, QObject *, const auto &done) {
            seen = m;
            seenPassword = password;
            done(Failure::None, QString());
        });
        QSignalSpy finished(&model, &MapDriveModel::finished);
        model.setFolder(QStringLiteral("\\\\nas\\photos"));
        model.setOtherCredentials(true);
        model.setProperty("user", QStringLiteral("HOME\\alex"));
        model.setProperty("password", QStringLiteral("s3cret"));
        model.finish();
        QCOMPARE(finished.size(), 1);
        QCOMPARE(seen.user, QStringLiteral("alex"));
        QCOMPARE(seen.domain, QStringLiteral("HOME"));
        QCOMPARE(seenPassword, QStringLiteral("s3cret"));
        QCOMPARE(seen.mountPoint, home.path() + QStringLiteral("/Network/photos"));
        QVERIFY(seen.reconnect);
        // The password does not stay in the window once connected.
        QVERIFY(model.property("password").toString().isEmpty());
    }

    void mapWindowAsksForCredentials()
    {
        QTemporaryDir home;
        MapDriveModel model(home.path(), [](const Mapping &m, const QString &, QObject *, const auto &done) {
            done(Failure::Credentials, failureText(Failure::Credentials, m.address));
        });
        model.setFolder(QStringLiteral("\\\\nas\\photos"));
        QVERIFY(!model.otherCredentials());
        model.finish();
        QVERIFY(model.otherCredentials());
        QVERIFY(model.error().contains(QStringLiteral("\\\\nas\\photos")));
        QVERIFY(model.error().contains(QStringLiteral("needs a user name and password")));
        QVERIFY(!model.busy());
    }

    void mapWindowRefusesBadFolders()
    {
        QTemporaryDir home;
        int calls = 0;
        MapDriveModel model(home.path(), [&calls](const Mapping &, const QString &, QObject *, const auto &done) {
            ++calls;
            done(Failure::None, QString());
        });
        model.setFolder(QStringLiteral("\\\\nas\\photos"));
        model.setLocation(QStringLiteral("/etc/photos"));
        model.finish();
        QVERIFY(!model.error().isEmpty());
        // A folder that already holds files.
        QDir(home.path()).mkpath(QStringLiteral("Network/photos"));
        QFile f(home.filePath(QStringLiteral("Network/photos/keep.txt")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        model.setLocation(home.filePath(QStringLiteral("Network/photos")));
        model.finish();
        QVERIFY(model.error().contains(QStringLiteral("already has files")));
        QCOMPARE(calls, 0);
    }
};

QTEST_MAIN(NetworkDrivesTest)

#include "test_networkdrives.moc"
