/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

// Network drives: a shared folder on another computer, mounted into a
// folder of the person's own (Map network drive). Files keeps the list,
// the password goes to the wallet, and a small root helper started through
// polkit does the mount itself (mount.cifs needs root).
namespace Raye::NetDrives
{

// A shared folder: \\server\share, plus a folder inside it.
struct DOLPHIN_EXPORT Address {
    QString server;
    QString share;
    QString path; // "Photos/2026", no leading slash; may be empty

    bool isValid() const;
    bool operator==(const Address &other) const = default;
};

// "\\server\share\sub", "//server/share" or "smb://user@server/share/sub".
// Invalid for anything else (a bare server, a local path,
// another protocol).
DOLPHIN_EXPORT Address parseAddress(const QString &text);
// The user name of an smb:// address, empty when there is none.
DOLPHIN_EXPORT QString userOf(const QString &text);

DOLPHIN_EXPORT bool isValidServer(const QString &server);
DOLPHIN_EXPORT bool isValidShare(const QString &share);
// A name for the drive's folder: no slash, not "." or "..", not empty.
DOLPHIN_EXPORT bool isValidName(const QString &name);

// "\\server\share"
DOLPHIN_EXPORT QString uncText(const Address &address);
// smb://server/share/path
DOLPHIN_EXPORT QUrl smbUrl(const Address &address);
// The folder a new drive goes in: <home>/Network/<name>.
DOLPHIN_EXPORT QString defaultMountPoint(const QString &home, const QString &name);

// The name people know a network protocol by, from a file system type
// (cifs, nfs4, fuse.sshfs...) or a URL scheme (smb, sftp, webdav...).
// Empty when it is not a network one.
DOLPHIN_EXPORT QString protocolName(const QString &typeOrScheme);
DOLPHIN_EXPORT bool isNetworkFileSystem(const QString &type);

// One line of /proc/self/mountinfo, the parts Files needs.
struct DOLPHIN_EXPORT Mount {
    QString source; // "//server/share", "server:/export"
    QString mountPoint;
    QString type;
};
// The network mounts of a mountinfo file (octal escapes undone).
DOLPHIN_EXPORT QList<Mount> parseMountInfo(const QByteArray &mountinfo);
DOLPHIN_EXPORT QList<Mount> networkMounts();
// Tests read their own mounts instead of /proc/self/mountinfo.
DOLPHIN_EXPORT void setMountReader(const std::function<QList<Mount>()> &reader);
// The mount whose mount point is exactly this path; invalid type if none.
DOLPHIN_EXPORT Mount mountAt(const QString &path, const QList<Mount> &mounts);
// "//server/share/sub" -> server, share; "server:/export" (NFS) -> server.
DOLPHIN_EXPORT Address addressOfSource(const QString &source);

// A mapped drive, as kept in raye-networkdrivesrc (never the password).
struct DOLPHIN_EXPORT Mapping {
    QString name;
    Address address;
    QString mountPoint;
    QString user; // empty: connect as a guest
    QString domain;
    bool reconnect = true;
    bool remember = false; // the password is in the wallet

    bool operator==(const Mapping &other) const = default;
};

// The list of mapped drives. Path empty: the user's raye-networkdrivesrc.
DOLPHIN_EXPORT QList<Mapping> loadMappings(const QString &configPath = QString());
DOLPHIN_EXPORT void saveMapping(const Mapping &mapping, const QString &configPath = QString());
DOLPHIN_EXPORT void removeMapping(const QString &mountPoint, const QString &configPath = QString());
// The mapping kept for this folder, or an empty one.
DOLPHIN_EXPORT Mapping mappingAt(const QString &mountPoint, const QString &configPath = QString());

// What went wrong, in plain words, from the helper's exit code and what
// mount.cifs printed.
enum class Failure {
    None,
    Credentials, // wrong or missing user name or password
    NotFound, // no such share
    Unreachable, // the computer does not answer
    MountPoint, // the folder cannot take a drive
    NotAllowed, // polkit said no
    Other,
};
DOLPHIN_EXPORT Failure failureOf(int exitCode, const QString &errorText);
DOLPHIN_EXPORT QString failureText(Failure failure, const Address &address);

// The wallet entry of a share: folder and key.
DOLPHIN_EXPORT QString walletFolder();
DOLPHIN_EXPORT QString walletKey(const Mapping &mapping);

// Runs the root helper (through pkexec). Done is called once, with the
// helper's exit code and what it printed on error.
using Done = std::function<void(int exitCode, const QString &errorText)>;
DOLPHIN_EXPORT void mount(const Mapping &mapping, const QString &password, QObject *context, const Done &done);
DOLPHIN_EXPORT void unmount(const QString &mountPoint, QObject *context, const Done &done);
// Blocking forms, for the reconnect tool.
DOLPHIN_EXPORT int mountNow(const Mapping &mapping, const QString &password, QString *errorText = nullptr);

// The wallet. Both return false when the wallet cannot be opened.
DOLPHIN_EXPORT bool storePassword(const Mapping &mapping, const QString &password);
DOLPHIN_EXPORT bool readPassword(const Mapping &mapping, QString *password);
DOLPHIN_EXPORT void forgetPassword(const Mapping &mapping);

// Connects the mapping: makes its folder, mounts, keeps it in the list
// and (if asked) its password in the wallet. Disconnect does the reverse
// and removes the folder if it is empty.
DOLPHIN_EXPORT void connectDrive(const Mapping &mapping, const QString &password, QObject *context, const std::function<void(Failure, const QString &)> &done);
DOLPHIN_EXPORT void disconnectDrive(const QString &mountPoint, QObject *context, const std::function<void(Failure, const QString &)> &done);

// Tells the open windows that a drive was connected or disconnected.
class DOLPHIN_EXPORT Notifier : public QObject
{
    Q_OBJECT
public:
    static Notifier *instance();
Q_SIGNALS:
    void changed();
};

// The path of the root helper and of the reconnect tool.
DOLPHIN_EXPORT QString helperPath();
}
