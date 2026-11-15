/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QUrl>

#include <functional>

// What Files calls a location, in the user's words (explorer spec 3.3, 3.10,
// 3.12, 3.14): the crumbs of the address row, the title of a window or tab,
// the icon next to them. Files never shows /home/ann: folders in the home
// read "This PC › Documents › Work", the system drive is a drive like any
// other ("This PC › Raye › usr"), the trash is the Recycle Bin, an archive
// is a folder ("Downloads › photos.zip › 2024").
//
// Nothing here touches the disk or the network: the names come from the
// Context, so the same URL always gives the same crumbs.
namespace Raye::Locations
{

// Files' own pages (raye:home, raye:gallery, raye:thispc).
inline constexpr char16_t pageScheme[] = u"raye";
DOLPHIN_EXPORT QUrl homePageUrl();
DOLPHIN_EXPORT QUrl galleryUrl();
DOLPHIN_EXPORT QUrl thisPcUrl();
DOLPHIN_EXPORT QUrl recycleBinUrl();
DOLPHIN_EXPORT QUrl networkUrl();

enum class Kind {
    Folder, // any other folder, local or not
    HomePage,
    Gallery,
    ThisPc,
    UserFolder, // the account's own folder (/home/ann)
    KnownFolder, // Desktop, Documents, Downloads, Music, Pictures, Videos
    Drive, // the root of a drive
    Phones, // phones and cameras (mtp:/, afc:/)
    Phone, // one phone or camera
    Network,
    Computer, // a computer on the network
    Share, // a shared folder on a computer
    RecycleBin,
    Archive, // a compressed folder, at its root
    Recent,
    Search,
};

enum class KnownFolder {
    Desktop,
    Documents,
    Downloads,
    Music,
    Pictures,
    Videos,
};

// A mounted drive, the way the drive naming of This PC calls it.
struct Drive {
    QString mountPoint;
    QString name;
    QString iconName;
    bool removable = false;
    // A network drive: it names its folder even inside the home.
    bool network = false;
};

// Where the names come from. Context::system() reads the running session.
struct DOLPHIN_EXPORT Context {
    QString homePath;
    // Shown for the account's own folder: the full name, or the login.
    QString userName;
    // Absolute paths of the known folders that exist on this account.
    QHash<KnownFolder, QString> knownFolders;
    // Mounted drives; "/" is the Raye drive. Missing: one is made up.
    QList<Drive> drives;
    // Answers whether a local path is a file, to cut an archive URL in
    // two (archive and path inside). Unset: the file name extension decides.
    std::function<bool(const QString &)> isFile;

    static Context system();
};

// Where Context::system() gets the drives. The drive naming of This PC
// (FX-15 DriveNames) plugs its list in here; until then a plain one reads
// the mounted volumes.
using DriveLister = std::function<QList<Drive>()>;
DOLPHIN_EXPORT void setDriveLister(const DriveLister &lister);
DOLPHIN_EXPORT QList<Drive> listDrives();

// One step of the address: what it opens, what it says, its icon.
struct Crumb {
    QUrl url;
    QString text;
    QString iconName;
    Kind kind = Kind::Folder;
    // A real folder: its chevron lists the folders in it, files dropped on
    // it are copied or moved there. Files' pages are neither.
    bool isFolder = true;

    bool operator==(const Crumb &other) const
    {
        return url == other.url && text == other.text && iconName == other.iconName && kind == other.kind && isFolder == other.isFolder;
    }
};

class DOLPHIN_EXPORT LocationNames
{
public:
    explicit LocationNames(Context context = Context::system());

    const Context &context() const
    {
        return m_context;
    }

    // From the first crumb ("This PC") to the location itself.
    QList<Crumb> crumbs(const QUrl &url) const;

    // The last crumb: the title of the window and of the tab.
    QString displayName(const QUrl &url) const;
    QString iconName(const QUrl &url) const;
    Kind kind(const QUrl &url) const;

    // Where Up goes: the crumb before the last; empty at a first crumb.
    QUrl upUrl(const QUrl &url) const;

    // The folders a crumb's chevron lists when they are not a folder
    // listing: This PC lists the known folders and the drives. Empty for
    // a real folder (the address row lists those itself).
    QList<Crumb> pageChildren(const QUrl &url) const;

    // What the address box shows to be typed over: the real path of a
    // local folder ("/home/ann/Documents"), the page name of a page.
    QString editText(const QUrl &url) const;

    // What the user typed in the address box, relative to the current
    // location: a path, "~", a URL, \\server\share, or a page name
    // ("This PC", "Ce PC"...). Invalid when it is empty.
    QUrl fromEditText(const QString &text, const QUrl &current = QUrl()) const;

    // The names, in the user's language.
    static QString knownFolderName(KnownFolder folder);
    static QString knownFolderIcon(KnownFolder folder);
    static QString systemDriveName();

private:
    QList<Crumb> localCrumbs(const QString &path, const QString &scheme) const;
    QList<Crumb> archiveCrumbs(const QUrl &url) const;
    QList<Crumb> trashCrumbs(const QUrl &url) const;
    QList<Crumb> networkCrumbs(const QUrl &url) const;
    QList<Crumb> phoneCrumbs(const QUrl &url) const;
    QList<Crumb> searchCrumbs(const QUrl &url) const;
    QList<Drive> drives() const;

    Context m_context;
};

}
