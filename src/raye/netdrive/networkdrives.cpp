/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "networkdrives.h"
#include "helperrules.h"

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <kwallet.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include <memory>

namespace Raye::NetDrives
{
namespace
{
const QString defaultConfig = QStringLiteral("raye-networkdrivesrc");

KConfig openConfig(const QString &path)
{
    return path.isEmpty() ? KConfig(defaultConfig, KConfig::SimpleConfig) : KConfig(path, KConfig::SimpleConfig);
}

QString unescapeMount(const QByteArray &field)
{
    QByteArray out;
    for (int i = 0; i < field.size(); ++i) {
        if (field.at(i) == '\\' && i + 3 < field.size()) {
            bool ok = false;
            const int c = field.mid(i + 1, 3).toInt(&ok, 8);
            if (ok) {
                out.append(char(c));
                i += 3;
                continue;
            }
        }
        out.append(field.at(i));
    }
    return QString::fromUtf8(out);
}
}

bool Address::isValid() const
{
    return isValidServer(server) && isValidShare(share);
}

bool isValidServer(const QString &server)
{
    return Helper::validServer(server.toStdString());
}

bool isValidShare(const QString &share)
{
    return Helper::validShare(share.toStdString());
}

bool isValidName(const QString &name)
{
    const QString n = name.trimmed();
    return !n.isEmpty() && n.size() <= 100 && n != QLatin1String(".") && n != QLatin1String("..") && !n.contains(QLatin1Char('/'))
        && !n.contains(QChar(0));
}

Address parseAddress(const QString &input)
{
    QString text = input.trimmed();
    if (text.startsWith(QLatin1String("smb://"), Qt::CaseInsensitive)) {
        const QUrl url(text);
        if (!url.isValid() || url.host().isEmpty()) {
            return {};
        }
        text = url.host() + url.path();
    } else if (text.contains(QLatin1String("://"))) {
        return {};
    } else {
        text.replace(QLatin1Char('\\'), QLatin1Char('/'));
        // \\server\share or //server/share; a plain path is a local folder.
        if (!text.startsWith(QLatin1String("//"))) {
            return {};
        }
        while (text.startsWith(QLatin1Char('/'))) {
            text.remove(0, 1);
        }
    }
    QStringList parts = text.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return {};
    }
    Address a;
    a.server = parts.takeFirst();
    a.share = parts.takeFirst();
    a.path = parts.join(QLatin1Char('/'));
    return a.isValid() ? a : Address();
}

QString userOf(const QString &text)
{
    const QString t = text.trimmed();
    if (!t.startsWith(QLatin1String("smb://"), Qt::CaseInsensitive)) {
        return QString();
    }
    return QUrl(t).userName();
}

QString uncText(const Address &address)
{
    QString out = QStringLiteral("\\\\%1\\%2").arg(address.server, address.share);
    if (!address.path.isEmpty()) {
        out += QLatin1Char('\\') + QString(address.path).replace(QLatin1Char('/'), QLatin1Char('\\'));
    }
    return out;
}

QUrl smbUrl(const Address &address)
{
    QUrl url;
    url.setScheme(QStringLiteral("smb"));
    url.setHost(address.server);
    url.setPath(QLatin1Char('/') + address.share + (address.path.isEmpty() ? QString() : QLatin1Char('/') + address.path));
    return url;
}

QString defaultMountPoint(const QString &home, const QString &name)
{
    return QDir::cleanPath(home + QStringLiteral("/Network/") + name.trimmed());
}

QString protocolName(const QString &typeOrScheme)
{
    const QString t = typeOrScheme.toLower();
    if (t == QLatin1String("cifs") || t == QLatin1String("smb3") || t == QLatin1String("smbfs") || t == QLatin1String("smb")) {
        return QStringLiteral("SMB");
    }
    if (t.startsWith(QLatin1String("nfs"))) {
        return QStringLiteral("NFS");
    }
    if (t == QLatin1String("fuse.sshfs") || t == QLatin1String("sshfs") || t == QLatin1String("sftp") || t == QLatin1String("fish")) {
        return QStringLiteral("SFTP");
    }
    if (t == QLatin1String("davfs") || t == QLatin1String("fuse.davfs2") || t == QLatin1String("webdav") || t == QLatin1String("webdavs")) {
        return QStringLiteral("WebDAV");
    }
    if (t == QLatin1String("ftp") || t == QLatin1String("ftps")) {
        return QStringLiteral("FTP");
    }
    return QString();
}

bool isNetworkFileSystem(const QString &type)
{
    static const QStringList types = {QStringLiteral("cifs"), QStringLiteral("smb3"), QStringLiteral("smbfs"), QStringLiteral("nfs"),
                                      QStringLiteral("nfs4"), QStringLiteral("fuse.sshfs"), QStringLiteral("sshfs"), QStringLiteral("davfs"),
                                      QStringLiteral("fuse.davfs2")};
    return types.contains(type);
}

QList<Mount> parseMountInfo(const QByteArray &mountinfo)
{
    QList<Mount> out;
    const QList<QByteArray> lines = mountinfo.split('\n');
    for (const QByteArray &line : lines) {
        const QList<QByteArray> f = line.split(' ');
        const int dash = f.indexOf("-");
        if (f.size() < 5 || dash < 0 || dash + 2 >= f.size()) {
            continue;
        }
        const QString type = QString::fromLatin1(f.at(dash + 1));
        if (!isNetworkFileSystem(type)) {
            continue;
        }
        out.append({unescapeMount(f.at(dash + 2)), unescapeMount(f.at(4)), type});
    }
    return out;
}

namespace
{
std::function<QList<Mount>()> &mountReader()
{
    static std::function<QList<Mount>()> reader;
    return reader;
}
}

void setMountReader(const std::function<QList<Mount>()> &reader)
{
    mountReader() = reader;
}

QList<Mount> networkMounts()
{
    if (mountReader()) {
        return mountReader()();
    }
    QFile file(QStringLiteral("/proc/self/mountinfo"));
    return file.open(QIODevice::ReadOnly) ? parseMountInfo(file.readAll()) : QList<Mount>();
}

Mount mountAt(const QString &path, const QList<Mount> &mounts)
{
    const QString clean = QDir::cleanPath(path);
    Mount found;
    for (const Mount &m : mounts) {
        if (QDir::cleanPath(m.mountPoint) == clean) {
            found = m;
        }
    }
    return found;
}

Address addressOfSource(const QString &source)
{
    if (source.startsWith(QLatin1String("//"))) {
        return parseAddress(source);
    }
    // NFS: server:/export/path
    const int colon = source.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        Address a;
        a.server = source.left(colon);
        a.share = source.mid(colon + 1);
        return a;
    }
    return {};
}

QList<Mapping> loadMappings(const QString &configPath)
{
    const KConfig config = openConfig(configPath);
    QList<Mapping> out;
    const QStringList groups = config.groupList();
    for (const QString &g : groups) {
        const KConfigGroup group = config.group(g);
        Mapping m;
        m.mountPoint = g;
        m.name = group.readEntry("Name", QFileInfo(g).fileName());
        m.address.server = group.readEntry("Server");
        m.address.share = group.readEntry("Share");
        m.address.path = group.readEntry("Path");
        m.user = group.readEntry("User");
        m.domain = group.readEntry("Domain");
        m.reconnect = group.readEntry("Reconnect", true);
        m.remember = group.readEntry("Remember", false);
        if (m.address.isValid()) {
            out.append(m);
        }
    }
    return out;
}

void saveMapping(const Mapping &mapping, const QString &configPath)
{
    KConfig config = openConfig(configPath);
    KConfigGroup group = config.group(QDir::cleanPath(mapping.mountPoint));
    group.writeEntry("Name", mapping.name);
    group.writeEntry("Server", mapping.address.server);
    group.writeEntry("Share", mapping.address.share);
    group.writeEntry("Path", mapping.address.path);
    group.writeEntry("User", mapping.user);
    group.writeEntry("Domain", mapping.domain);
    group.writeEntry("Reconnect", mapping.reconnect);
    group.writeEntry("Remember", mapping.remember);
    config.sync();
}

void removeMapping(const QString &mountPoint, const QString &configPath)
{
    KConfig config = openConfig(configPath);
    config.deleteGroup(QDir::cleanPath(mountPoint));
    config.sync();
}

Mapping mappingAt(const QString &mountPoint, const QString &configPath)
{
    const QString clean = QDir::cleanPath(mountPoint);
    const QList<Mapping> all = loadMappings(configPath);
    for (const Mapping &m : all) {
        if (QDir::cleanPath(m.mountPoint) == clean) {
            return m;
        }
    }
    return {};
}

Failure failureOf(int exitCode, const QString &errorText)
{
    if (exitCode == 0) {
        return Failure::None;
    }
    // pkexec: 126 the person said no, 127 not allowed.
    if (exitCode == 126 || exitCode == 127) {
        return Failure::NotAllowed;
    }
    if (exitCode == 3) {
        return Failure::MountPoint;
    }
    static const QRegularExpression code(QStringLiteral("mount error\\((\\d+)\\)"));
    const QRegularExpressionMatch m = code.match(errorText);
    const int err = m.hasMatch() ? m.captured(1).toInt() : 0;
    switch (err) {
    case 13: // EACCES
    case 1: // EPERM
    case 126: // ENOKEY
        return Failure::Credentials;
    case 2: // ENOENT
    case 6: // ENXIO
        return Failure::NotFound;
    case 101: // ENETUNREACH
    case 110: // ETIMEDOUT
    case 111: // ECONNREFUSED
    case 112: // EHOSTDOWN
    case 113: // EHOSTUNREACH
        return Failure::Unreachable;
    }
    if (errorText.contains(QLatin1String("could not resolve address"), Qt::CaseInsensitive)
        || errorText.contains(QLatin1String("unable to find suitable address"), Qt::CaseInsensitive)) {
        return Failure::Unreachable;
    }
    return Failure::Other;
}

QString failureText(Failure failure, const Address &address)
{
    const QString where = uncText(address);
    switch (failure) {
    case Failure::None:
        return QString();
    case Failure::Credentials:
        return i18nc("@info", "The user name or password for %1 is not right.", where);
    case Failure::NotFound:
        return i18nc("@info", "%1 has no shared folder with that name.", where);
    case Failure::Unreachable:
        return i18nc("@info", "Can't reach %1. Check the name and that the computer is on.", QStringLiteral("\\\\") + address.server);
    case Failure::MountPoint:
        return i18nc("@info", "The drive needs an empty folder of yours, in your home folder.");
    case Failure::NotAllowed:
        return i18nc("@info", "You aren't allowed to connect network drives.");
    case Failure::Other:
        break;
    }
    return i18nc("@info", "Couldn't connect to %1.", where);
}

QString walletFolder()
{
    return QStringLiteral("Raye network drives");
}

QString walletKey(const Mapping &mapping)
{
    QUrl url = smbUrl({mapping.address.server, mapping.address.share, QString()});
    url.setUserName(mapping.user);
    return url.toString();
}

Notifier *Notifier::instance()
{
    static Notifier notifier;
    return &notifier;
}

QString helperPath()
{
    const QString env = qEnvironmentVariable("RAYE_NETDRIVE_HELPER");
    return env.isEmpty() ? QStringLiteral("/usr/libexec/raye-files-netdrive-helper") : env;
}

namespace
{
QStringList mountArguments(const Mapping &m)
{
    return {helperPath(),
            QStringLiteral("mount"),
            m.address.server,
            m.address.share,
            QDir::cleanPath(m.mountPoint),
            m.user,
            m.user.isEmpty() ? QString() : m.domain};
}

QString pkexec()
{
    return qEnvironmentVariable("RAYE_NETDRIVE_PKEXEC", QStringLiteral("pkexec"));
}

void run(const QStringList &args, const QByteArray &input, QObject *context, const Done &done)
{
    auto *p = new QProcess(context);
    p->setProcessChannelMode(QProcess::SeparateChannels);
    QObject::connect(p, &QProcess::finished, context, [p, done](int code, QProcess::ExitStatus status) {
        const QString err = QString::fromLocal8Bit(p->readAllStandardError()).trimmed();
        p->deleteLater();
        done(status == QProcess::NormalExit ? code : 4, err);
    });
    QObject::connect(p, &QProcess::errorOccurred, context, [p, done](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            p->deleteLater();
            done(127, QString());
        }
    });
    p->start(pkexec(), args);
    p->write(input);
    p->closeWriteChannel();
}
}

void mount(const Mapping &mapping, const QString &password, QObject *context, const Done &done)
{
    run(mountArguments(mapping), password.toUtf8(), context, done);
}

void unmount(const QString &mountPoint, QObject *context, const Done &done)
{
    run({helperPath(), QStringLiteral("unmount"), QDir::cleanPath(mountPoint)}, QByteArray(), context, done);
}

int mountNow(const Mapping &mapping, const QString &password, QString *errorText)
{
    QProcess p;
    p.start(pkexec(), mountArguments(mapping));
    p.write(password.toUtf8());
    p.closeWriteChannel();
    if (!p.waitForFinished(90000)) {
        p.kill();
        return 4;
    }
    if (errorText) {
        *errorText = QString::fromLocal8Bit(p.readAllStandardError()).trimmed();
    }
    return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : 4;
}

namespace
{
KWallet::Wallet *openWallet()
{
    KWallet::Wallet *w = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), 0, KWallet::Wallet::Synchronous);
    if (!w) {
        return nullptr;
    }
    if (!w->hasFolder(walletFolder())) {
        w->createFolder(walletFolder());
    }
    w->setFolder(walletFolder());
    return w;
}
}

bool storePassword(const Mapping &mapping, const QString &password)
{
    std::unique_ptr<KWallet::Wallet> w(openWallet());
    return w && w->writePassword(walletKey(mapping), password) == 0;
}

bool readPassword(const Mapping &mapping, QString *password)
{
    std::unique_ptr<KWallet::Wallet> w(openWallet());
    return w && w->readPassword(walletKey(mapping), *password) == 0;
}

void forgetPassword(const Mapping &mapping)
{
    std::unique_ptr<KWallet::Wallet> w(openWallet());
    if (w) {
        w->removeEntry(walletKey(mapping));
    }
}

void connectDrive(const Mapping &mapping, const QString &password, QObject *context, const std::function<void(Failure, const QString &)> &done)
{
    const QString folder = QDir::cleanPath(mapping.mountPoint);
    const bool made = !QFileInfo::exists(folder);
    if (!QDir().mkpath(folder)) {
        done(Failure::MountPoint, failureText(Failure::MountPoint, mapping.address));
        return;
    }
    mount(mapping, password, context, [=](int code, const QString &err) {
        const Failure f = failureOf(code, err);
        if (f != Failure::None) {
            if (made) {
                QDir().rmdir(folder);
            }
            done(f, failureText(f, mapping.address));
            return;
        }
        saveMapping(mapping);
        if (mapping.remember && !mapping.user.isEmpty()) {
            storePassword(mapping, password);
        }
        Q_EMIT Notifier::instance()->changed();
        done(Failure::None, QString());
    });
}

void disconnectDrive(const QString &mountPoint, QObject *context, const std::function<void(Failure, const QString &)> &done)
{
    const QString folder = QDir::cleanPath(mountPoint);
    const Mapping mapping = mappingAt(folder);
    const auto forget = [=] {
        removeMapping(folder);
        if (mapping.remember) {
            forgetPassword(mapping);
        }
        QDir().rmdir(folder);
        Q_EMIT Notifier::instance()->changed();
    };
    if (mountAt(folder, networkMounts()).type.isEmpty()) {
        // Not connected right now: only the mapping is left to drop.
        forget();
        done(Failure::None, QString());
        return;
    }
    unmount(folder, context, [=](int code, const QString &err) {
        const Failure f = failureOf(code, err);
        if (f != Failure::None) {
            done(f, i18nc("@info", "Couldn't disconnect %1. Close the files that are open on it, then try again.", mapping.name.isEmpty() ? folder : mapping.name));
            return;
        }
        forget();
        done(Failure::None, QString());
    });
}

}
