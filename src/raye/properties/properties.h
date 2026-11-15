/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include <functional>

#include <sys/types.h>

// The Properties dialog of Files (explorer spec 3.15): what it says about
// files, folders and drives, and what it writes back (name, read-only,
// hidden, who can open it). Local files only; anything else keeps KDE's
// dialog.
namespace Raye::Properties
{

// Who a permission applies to, and how much they get, in plain words.
enum class Who { Owner, Group, Others };
enum class Access { None = 0, Read = 1, ReadWrite = 2 };

DOLPHIN_EXPORT Access accessFor(mode_t mode, Who who);
// Read gives r (and x on a folder, to open it), Read and write rw (rwx on a
// folder). A program keeps its x bits on a file.
DOLPHIN_EXPORT mode_t withAccess(mode_t mode, Who who, Access access, bool folder);

struct Totals {
    qint64 size = 0;
    qint64 onDisk = 0;
    int files = 0;
    int folders = 0;
};
// Everything under these paths, the paths themselves included. Links are
// counted as themselves, never followed.
DOLPHIN_EXPORT Totals measure(const QStringList &paths);

// "2.4 MB (2,516,582 bytes)"
DOLPHIN_EXPORT QString sizeText(qint64 bytes);
// "12 files, 3 folders"
DOLPHIN_EXPORT QString containsText(int files, int folders);
// "Btrfs", "NTFS", "FAT32": the name people know a file system by.
DOLPHIN_EXPORT QString fileSystemName(const QString &type);
DOLPHIN_EXPORT QString dateText(const QDateTime &date);

// Hidden the way KIO reads it: a dot name, or a line in the folder's
// .hidden file. setHidden edits .hidden, so the name never changes; it
// cannot show a dot name.
DOLPHIN_EXPORT bool isHidden(const QString &path);
DOLPHIN_EXPORT bool setHidden(const QString &path, bool hidden);

// The media properties of a file for the Details tab: [{label, value}].
// Empty for files with nothing to say.
DOLPHIN_EXPORT QVariantList mediaDetails(const QString &path);

class DOLPHIN_EXPORT PropertiesModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count CONSTANT)
    Q_PROPERTY(bool folder READ folder CONSTANT)
    Q_PROPERTY(bool drive READ drive CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(bool canRename READ canRename CONSTANT)
    Q_PROPERTY(QString iconName READ iconName CONSTANT)
    Q_PROPERTY(QString typeText READ typeText CONSTANT)
    Q_PROPERTY(QString openWithName READ openWithName NOTIFY changed)
    Q_PROPERTY(QString openWithIcon READ openWithIcon NOTIFY changed)
    Q_PROPERTY(QString location READ location CONSTANT)
    Q_PROPERTY(QString sizeText READ sizeText NOTIFY measured)
    Q_PROPERTY(QString onDiskText READ onDiskText NOTIFY measured)
    Q_PROPERTY(QString containsText READ containsText NOTIFY measured)
    Q_PROPERTY(bool measuring READ measuring NOTIFY measured)
    Q_PROPERTY(QString created READ created CONSTANT)
    Q_PROPERTY(QString modified READ modified CONSTANT)
    Q_PROPERTY(QString accessed READ accessed CONSTANT)
    Q_PROPERTY(bool readOnly READ readOnly WRITE setReadOnly NOTIFY changed)
    Q_PROPERTY(bool hidden READ hidden WRITE setHidden NOTIFY changed)
    Q_PROPERTY(bool canHide READ canHide CONSTANT)
    Q_PROPERTY(bool canChange READ canChange CONSTANT)
    Q_PROPERTY(QString ownerName READ ownerName CONSTANT)
    Q_PROPERTY(QString groupName READ groupName CONSTANT)
    Q_PROPERTY(int ownerAccess READ ownerAccess WRITE setOwnerAccess NOTIFY changed)
    Q_PROPERTY(int groupAccess READ groupAccess WRITE setGroupAccess NOTIFY changed)
    Q_PROPERTY(int othersAccess READ othersAccess WRITE setOthersAccess NOTIFY changed)
    Q_PROPERTY(QVariantList details READ details CONSTANT)
    Q_PROPERTY(QString fileSystem READ fileSystem CONSTANT)
    Q_PROPERTY(QString usedText READ usedText CONSTANT)
    Q_PROPERTY(QString freeText READ freeText CONSTANT)
    Q_PROPERTY(QString capacityText READ capacityText CONSTANT)
    Q_PROPERTY(qreal usage READ usage CONSTANT)
    Q_PROPERTY(bool hasSpace READ hasSpace CONSTANT)
    Q_PROPERTY(bool network READ network CONSTANT)
    Q_PROPERTY(QString server READ server CONSTANT)
    Q_PROPERTY(QString networkFolder READ networkFolder CONSTANT)
    Q_PROPERTY(QString protocol READ protocol CONSTANT)
    Q_PROPERTY(QString mountedAt READ mountedAt CONSTANT)
    Q_PROPERTY(QString connectedAs READ connectedAs CONSTANT)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    struct Space {
        qint64 total = 0;
        qint64 free = 0;
        QString fileSystemType;
    };
    using SpaceReader = std::function<Space(const QString &mountPoint)>;

    // Local files and folders; a drive when the one path is a mount point
    // (or driveName is set). Measures folders in the background.
    explicit PropertiesModel(const QList<QUrl> &urls, QObject *parent = nullptr, const QString &driveName = QString(), const SpaceReader &space = {});

    // Files can show these: every URL is a local path that exists, or one
    // network location (smb://server/share...).
    static bool handles(const QList<QUrl> &urls);

    int count() const { return m_paths.size(); }
    bool folder() const { return m_folder; }
    bool drive() const { return m_drive; }
    QString name() const { return m_name; }
    void setName(const QString &name);
    bool canRename() const;
    QString iconName() const { return m_iconName; }
    QString typeText() const { return m_typeText; }
    QString openWithName() const { return m_openWithName; }
    QString openWithIcon() const { return m_openWithIcon; }
    QString location() const { return m_location; }
    QString sizeText() const;
    QString onDiskText() const;
    QString containsText() const;
    bool measuring() const { return m_measuring; }
    QString created() const { return m_created; }
    QString modified() const { return m_modified; }
    QString accessed() const { return m_accessed; }
    bool readOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly);
    bool hidden() const { return m_hidden; }
    void setHidden(bool hidden);
    bool canHide() const { return m_canHide; }
    bool canChange() const { return m_canChange; }
    QString ownerName() const { return m_ownerName; }
    QString groupName() const { return m_groupName; }
    int ownerAccess() const { return m_access[0]; }
    int groupAccess() const { return m_access[1]; }
    int othersAccess() const { return m_access[2]; }
    void setOwnerAccess(int a) { setAccess(0, a); }
    void setGroupAccess(int a) { setAccess(1, a); }
    void setOthersAccess(int a) { setAccess(2, a); }
    QVariantList details() const { return m_details; }
    QString fileSystem() const { return m_fileSystem; }
    QString usedText() const { return m_usedText; }
    QString freeText() const { return m_freeText; }
    QString capacityText() const { return m_capacityText; }
    qreal usage() const { return m_usage; }
    bool hasSpace() const { return m_capacityText.size() > 0 && !m_usedText.isEmpty(); }
    bool network() const { return m_network; }
    QString server() const { return m_server; }
    QString networkFolder() const { return m_networkFolder; }
    QString protocol() const { return m_protocol; }
    QString mountedAt() const { return m_mountedAt; }
    QString connectedAs() const { return m_connectedAs; }
    bool dirty() const { return m_dirty; }
    QString error() const { return m_error; }
    Totals totals() const { return m_totals; }
    QStringList paths() const { return m_paths; }

    // Writes what changed. False, with error set, when something could not
    // be written; what was written stays written.
    Q_INVOKABLE bool apply();
    Q_INVOKABLE void changeApp();
    Q_INVOKABLE void cleanUp();
    Q_INVOKABLE void advanced();
    // The app chosen in "Change…": shown until the dialog closes.
    void setOpenWith(const QString &name, const QString &icon);

Q_SIGNALS:
    void changed();
    void measured();
    void changeAppRequested();
    void advancedRequested();

private:
    void setAccess(int who, int access);
    void readNetworkMount(const QString &path, const QString &type, const QString &source, const SpaceReader &space);
    void readRemoteUrl(const QUrl &url);
    void markDirty();

    QStringList m_paths;
    bool m_folder = false;
    bool m_drive = false;
    QString m_name;
    QString m_originalName;
    QString m_iconName;
    QString m_typeText;
    QString m_openWithName;
    QString m_openWithIcon;
    QString m_location;
    Totals m_totals;
    bool m_measuring = false;
    QString m_created;
    QString m_modified;
    QString m_accessed;
    bool m_readOnly = false;
    bool m_originalReadOnly = false;
    bool m_hidden = false;
    bool m_originalHidden = false;
    bool m_canHide = false;
    bool m_canChange = false;
    QString m_ownerName;
    QString m_groupName;
    int m_access[3] = {0, 0, 0};
    int m_originalAccess[3] = {0, 0, 0};
    QVariantList m_details;
    QString m_fileSystem;
    QString m_usedText;
    QString m_freeText;
    QString m_capacityText;
    qreal m_usage = 0;
    bool m_network = false;
    QString m_server;
    QString m_networkFolder;
    QString m_protocol;
    QString m_mountedAt;
    QString m_connectedAs;
    bool m_dirty = false;
    QString m_error;
};

}
