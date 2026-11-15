/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Location names (explorer spec 3.3, 3.10, 3.12, 3.14): the crumbs of the
// address row for every kind of place Files opens, in English and French,
// with the URL each crumb opens, plus Up, the edit text and what the user
// may type in the address box.

#include "locations/locationnames.h"

#include <KLocalizedString>

#include <QTest>

#include <clocale>

using namespace Raye::Locations;

namespace
{
Context annContext()
{
    Context c;
    c.homePath = QStringLiteral("/home/ann");
    c.userName = QStringLiteral("Ann");
    c.knownFolders = {
        {KnownFolder::Desktop, QStringLiteral("/home/ann/Desktop")},
        {KnownFolder::Documents, QStringLiteral("/home/ann/Documents")},
        {KnownFolder::Downloads, QStringLiteral("/home/ann/Downloads")},
        {KnownFolder::Music, QStringLiteral("/home/ann/Music")},
        {KnownFolder::Pictures, QStringLiteral("/home/ann/Pictures")},
        {KnownFolder::Videos, QStringLiteral("/home/ann/Videos")},
    };
    c.drives = {
        {QStringLiteral("/"), QStringLiteral("Raye"), QStringLiteral("drive-harddisk-system"), false},
        {QStringLiteral("/media/ann/USBSTICK"), QStringLiteral("USB Drive"), QStringLiteral("drive-removable-media-usb"), true},
        {QStringLiteral("/run/media/ann/SD"), QStringLiteral("SD card"), QStringLiteral("media-flash-sd-mmc"), true},
        {QStringLiteral("/home/ann/Network/photos"), QStringLiteral("Holiday photos"), QStringLiteral("folder-network"), false, true},
    };
    return c;
}

QStringList texts(const QList<Crumb> &crumbs)
{
    QStringList out;
    for (const Crumb &c : crumbs) {
        out << c.text;
    }
    return out;
}

QStringList urls(const QList<Crumb> &crumbs)
{
    QStringList out;
    for (const Crumb &c : crumbs) {
        out << c.url.toString();
    }
    return out;
}

bool useLanguage(const char *lang)
{
    const QString l = QString::fromLatin1(lang);
    const QString name = l == QLatin1String("en") ? QStringLiteral("C.UTF-8") : l + QLatin1Char('_') + l.toUpper() + QStringLiteral(".UTF-8");
    const bool found = std::setlocale(LC_MESSAGES, qPrintable(name)) != nullptr;
    KLocalizedString::addDomainLocaleDir("raye-files", QStringLiteral(RAYE_FILES_BINARY_DIR "/locale"));
    KLocalizedString::setLanguages({l});
    return found;
}
}

class RayeLocationNamesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void crumbs_data();
    void crumbs();
    void crumbUrls_data();
    void crumbUrls();
    void kinds_data();
    void kinds();
    void french_data();
    void french();
    void pagesAndFoldersAreTold();
    void upGoesOneCrumbBack_data();
    void upGoesOneCrumbBack();
    void editText();
    void typedText_data();
    void typedText();
    void frenchPageNamesCanBeTyped();
    void thisPcListsKnownFoldersAndDrives();
    void missingDrivesStillShowRaye();
    void archiveCutByTheFileWhenAsked();
    void driveListerIsUsed();
};

void RayeLocationNamesTest::init()
{
    useLanguage("en");
}

void RayeLocationNamesTest::crumbs_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QStringList>("expected");

    auto row = [](const char *name, const char *url, const QStringList &expected) {
        QTest::newRow(name) << QString::fromUtf8(url) << expected;
    };
    // Files' pages
    row("home page", "raye:home", {"Home"});
    row("gallery", "raye:gallery", {"Gallery"});
    row("this pc", "raye:thispc", {"This PC"});
    // the account's folders
    row("user folder", "file:///home/ann", {"This PC", "Ann"});
    row("documents", "file:///home/ann/Documents", {"This PC", "Documents"});
    row("documents slash", "file:///home/ann/Documents/", {"This PC", "Documents"});
    row("documents work", "file:///home/ann/Documents/Work", {"This PC", "Documents", "Work"});
    row("downloads", "file:///home/ann/Downloads", {"This PC", "Downloads"});
    row("pictures deep", "file:///home/ann/Pictures/2024/Summer", {"This PC", "Pictures", "2024", "Summer"});
    row("desktop", "file:///home/ann/Desktop", {"This PC", "Desktop"});
    row("other home folder", "file:///home/ann/Projects/raye", {"This PC", "Ann", "Projects", "raye"});
    row("dot dot cleaned", "file:///home/ann/Downloads/../Documents", {"This PC", "Documents"});
    row("folder named downloads", "file:///home/ann/Documents/Downloads", {"This PC", "Documents", "Downloads"});
    // the drives
    row("root", "file:///", {"This PC", "Raye"});
    row("usr share", "file:///usr/share", {"This PC", "Raye", "usr", "share"});
    row("home parent", "file:///home", {"This PC", "Raye", "home"});
    row("someone else", "file:///home/bob", {"This PC", "Raye", "home", "bob"});
    row("usb stick", "file:///media/ann/USBSTICK/photos", {"This PC", "USB Drive", "photos"});
    row("sd card", "file:///run/media/ann/SD", {"This PC", "SD card"});
    row("network drive", "file:///home/ann/Network/photos/2024", {"This PC", "Holiday photos", "2024"});
    row("network folder", "file:///home/ann/Network", {"This PC", "Ann", "Network"});
    row("admin", "admin:/etc", {"This PC", "Raye", "etc"});
    row("bare path", "/usr", {"This PC", "Raye", "usr"});
    // the Recycle Bin
    row("recycle bin", "trash:/", {"Recycle Bin"});
    row("recycle bin folder", "trash:/0-photos/a", {"Recycle Bin", "photos", "a"});
    // archives
    row("archive inside", "rayearchive:/home/ann/Downloads/photos.zip/2024", {"This PC", "Downloads", "photos.zip", "2024"});
    row("archive root", "rayearchive:/home/ann/Downloads/photos.zip", {"This PC", "Downloads", "photos.zip"});
    row("tar archive", "rayearchive:/tmp/x.tar.gz/a/b", {"This PC", "Raye", "tmp", "x.tar.gz", "a", "b"});
    // the network
    row("network", "smb:/", {"Network"});
    row("network remote", "remote:/", {"Network"});
    row("computer", "smb://nas", {"Network", "nas"});
    row("share", "smb://nas/share/sub", {"Network", "nas", "share", "sub"});
    row("sftp", "sftp://ann@host/srv/www", {"Network", "host", "srv", "www"});
    // phones
    row("phones", "mtp:/", {"This PC", "Phones and cameras"});
    row("phone folder", "mtp:/Pixel 8/Internal storage/DCIM", {"This PC", "Pixel 8", "Internal storage", "DCIM"});
    // the rest
    row("desktop scheme", "desktop:/", {"This PC", "Desktop"});
    row("desktop scheme folder", "desktop:/notes", {"This PC", "Desktop", "notes"});
    row("recent", "recentlyused:/", {"Recent"});
    row("search", "filenamesearch:?search=cat&url=file:///home/ann/Documents", {"This PC", "Documents", "Search results in Documents"});
}

void RayeLocationNamesTest::crumbs()
{
    QFETCH(QString, url);
    QFETCH(QStringList, expected);
    const LocationNames names(annContext());
    const QUrl u = url.startsWith(QLatin1Char('/')) ? QUrl::fromLocalFile(url) : QUrl(url);
    QCOMPARE(texts(names.crumbs(u)), expected);
    QCOMPARE(names.displayName(u), expected.last());
}

void RayeLocationNamesTest::crumbUrls_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("documents work") << "file:///home/ann/Documents/Work"
                                    << QStringList{"raye:thispc", "file:///home/ann/Documents", "file:///home/ann/Documents/Work"};
    QTest::newRow("user folder") << "file:///home/ann/Projects" << QStringList{"raye:thispc", "file:///home/ann", "file:///home/ann/Projects"};
    QTest::newRow("system") << "file:///usr/share" << QStringList{"raye:thispc", "file:///", "file:///usr", "file:///usr/share"};
    QTest::newRow("usb") << "file:///media/ann/USBSTICK/photos"
                         << QStringList{"raye:thispc", "file:///media/ann/USBSTICK", "file:///media/ann/USBSTICK/photos"};
    QTest::newRow("trash") << "trash:/0-photos/a" << QStringList{"trash:/", "trash:/0-photos", "trash:/0-photos/a"};
    QTest::newRow("archive") << "rayearchive:/home/ann/Downloads/photos.zip/2024"
                             << QStringList{"raye:thispc",
                                            "file:///home/ann/Downloads",
                                            "rayearchive:/home/ann/Downloads/photos.zip",
                                            "rayearchive:/home/ann/Downloads/photos.zip/2024"};
    QTest::newRow("share") << "smb://nas/share/sub" << QStringList{"smb:/", "smb://nas", "smb://nas/share", "smb://nas/share/sub"};
    QTest::newRow("sftp keeps user") << "sftp://ann@host/srv" << QStringList{"smb:/", "sftp://ann@host", "sftp://ann@host/srv"};
    QTest::newRow("admin") << "admin:/etc" << QStringList{"raye:thispc", "admin:/", "admin:/etc"};
    QTest::newRow("phone") << "mtp:/Pixel 8/DCIM" << QStringList{"raye:thispc", "mtp:/Pixel 8", "mtp:/Pixel 8/DCIM"};
}

void RayeLocationNamesTest::crumbUrls()
{
    QFETCH(QString, url);
    QFETCH(QStringList, expected);
    const LocationNames names(annContext());
    QCOMPARE(urls(names.crumbs(QUrl(url))), expected);
}

void RayeLocationNamesTest::kinds_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<int>("kind");
    QTest::addColumn<QString>("icon");
    QTest::newRow("home page") << "raye:home" << int(Kind::HomePage) << "go-home";
    QTest::newRow("gallery") << "raye:gallery" << int(Kind::Gallery) << "folder-pictures";
    QTest::newRow("this pc") << "raye:thispc" << int(Kind::ThisPc) << "computer";
    QTest::newRow("user") << "file:///home/ann" << int(Kind::UserFolder) << "user-home";
    QTest::newRow("documents") << "file:///home/ann/Documents" << int(Kind::KnownFolder) << "folder-documents";
    QTest::newRow("downloads") << "file:///home/ann/Downloads" << int(Kind::KnownFolder) << "folder-download";
    QTest::newRow("folder") << "file:///home/ann/Documents/Work" << int(Kind::Folder) << "folder";
    QTest::newRow("raye drive") << "file:///" << int(Kind::Drive) << "drive-harddisk-system";
    QTest::newRow("usb") << "file:///media/ann/USBSTICK" << int(Kind::Drive) << "drive-removable-media-usb";
    QTest::newRow("trash") << "trash:/" << int(Kind::RecycleBin) << "user-trash";
    QTest::newRow("archive") << "rayearchive:/home/ann/a.7z" << int(Kind::Archive) << "package-x-generic";
    QTest::newRow("network") << "smb:/" << int(Kind::Network) << "network-workgroup";
    QTest::newRow("computer") << "smb://nas" << int(Kind::Computer) << "network-server";
    QTest::newRow("share") << "smb://nas/share" << int(Kind::Share) << "folder-remote";
    QTest::newRow("phones") << "mtp:/" << int(Kind::Phones) << "smartphone";
    QTest::newRow("phone") << "mtp:/Pixel 8" << int(Kind::Phone) << "smartphone";
    QTest::newRow("recent") << "recentlyused:/" << int(Kind::Recent) << "document-open-recent";
    QTest::newRow("search") << "filenamesearch:?search=a&url=file:///home/ann" << int(Kind::Search) << "edit-find";
}

void RayeLocationNamesTest::kinds()
{
    QFETCH(QString, url);
    QFETCH(int, kind);
    QFETCH(QString, icon);
    const LocationNames names(annContext());
    QCOMPARE(int(names.kind(QUrl(url))), kind);
    QCOMPARE(names.iconName(QUrl(url)), icon);
}

void RayeLocationNamesTest::french_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("home page") << "raye:home" << QStringList{"Accueil"};
    QTest::newRow("gallery") << "raye:gallery" << QStringList{"Galerie"};
    QTest::newRow("this pc") << "raye:thispc" << QStringList{"Ce PC"};
    QTest::newRow("documents") << "file:///home/ann/Documents/Travail" << QStringList{"Ce PC", "Documents", "Travail"};
    QTest::newRow("downloads") << "file:///home/ann/Downloads" << QStringList{"Ce PC", "Téléchargements"};
    QTest::newRow("pictures") << "file:///home/ann/Pictures" << QStringList{"Ce PC", "Images"};
    QTest::newRow("music") << "file:///home/ann/Music" << QStringList{"Ce PC", "Musique"};
    QTest::newRow("videos") << "file:///home/ann/Videos" << QStringList{"Ce PC", "Vidéos"};
    QTest::newRow("desktop") << "file:///home/ann/Desktop" << QStringList{"Ce PC", "Bureau"};
    QTest::newRow("folder keeps its name") << "file:///home/ann/Documents/Downloads" << QStringList{"Ce PC", "Documents", "Downloads"};
    QTest::newRow("system") << "file:///usr" << QStringList{"Ce PC", "Raye", "usr"};
    QTest::newRow("recycle bin") << "trash:/" << QStringList{"Corbeille"};
    QTest::newRow("network") << "smb://nas" << QStringList{"Réseau", "nas"};
    QTest::newRow("phones") << "mtp:/" << QStringList{"Ce PC", "Téléphones et appareils photo"};
    QTest::newRow("recent") << "recentlyused:/" << QStringList{"Récents"};
    QTest::newRow("search") << "filenamesearch:?search=chat&url=file:///home/ann/Pictures"
                            << QStringList{"Ce PC", "Images", "Résultats de recherche dans Images"};
}

void RayeLocationNamesTest::french()
{
    QFETCH(QString, url);
    QFETCH(QStringList, expected);
    if (!useLanguage("fr")) {
        QSKIP("no fr_FR.UTF-8 locale on this system");
    }
    const LocationNames names(annContext());
    QCOMPARE(texts(names.crumbs(QUrl(url))), expected);
}

void RayeLocationNamesTest::pagesAndFoldersAreTold()
{
    const LocationNames names(annContext());
    const QList<Crumb> c = names.crumbs(QUrl(QStringLiteral("file:///home/ann/Documents")));
    QVERIFY(!c.at(0).isFolder); // This PC is a page: no drop, no listing
    QVERIFY(c.at(1).isFolder);
    QVERIFY(!names.crumbs(homePageUrl()).at(0).isFolder);
    QVERIFY(!names.crumbs(galleryUrl()).at(0).isFolder);
    QVERIFY(names.crumbs(recycleBinUrl()).at(0).isFolder); // drop = delete
    QVERIFY(!names.crumbs(QUrl(QStringLiteral("mtp:/"))).at(1).isFolder);
    QVERIFY(names.crumbs(QUrl(QStringLiteral("smb://nas/share"))).at(2).isFolder);
}

void RayeLocationNamesTest::upGoesOneCrumbBack_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("up");
    QTest::newRow("work") << "file:///home/ann/Documents/Work" << "file:///home/ann/Documents";
    QTest::newRow("documents") << "file:///home/ann/Documents" << "raye:thispc";
    QTest::newRow("user") << "file:///home/ann" << "raye:thispc";
    QTest::newRow("usr") << "file:///usr" << "file:///";
    QTest::newRow("this pc") << "raye:thispc" << "";
    QTest::newRow("home page") << "raye:home" << "";
    QTest::newRow("trash") << "trash:/" << "";
    QTest::newRow("trash folder") << "trash:/0-a" << "trash:/";
    QTest::newRow("archive") << "rayearchive:/home/ann/Downloads/p.zip" << "file:///home/ann/Downloads";
}

void RayeLocationNamesTest::upGoesOneCrumbBack()
{
    QFETCH(QString, url);
    QFETCH(QString, up);
    const LocationNames names(annContext());
    QCOMPARE(names.upUrl(QUrl(url)).toString(), up);
}

void RayeLocationNamesTest::editText()
{
    const LocationNames names(annContext());
    QCOMPARE(names.editText(QUrl(QStringLiteral("file:///home/ann/Documents/Work"))), QStringLiteral("/home/ann/Documents/Work"));
    QCOMPARE(names.editText(QUrl(QStringLiteral("file:///"))), QStringLiteral("/"));
    QCOMPARE(names.editText(thisPcUrl()), QStringLiteral("This PC"));
    QCOMPARE(names.editText(homePageUrl()), QStringLiteral("Home"));
    QCOMPARE(names.editText(QUrl(QStringLiteral("smb://nas/share"))), QStringLiteral("smb://nas/share"));
    QCOMPARE(names.editText(recycleBinUrl()), QStringLiteral("Recycle Bin"));
}

void RayeLocationNamesTest::typedText_data()
{
    QTest::addColumn<QString>("typed");
    QTest::addColumn<QString>("current");
    QTest::addColumn<QString>("expected");
    QTest::newRow("absolute") << "/usr/share" << "" << "file:///usr/share";
    QTest::newRow("home tilde") << "~" << "" << "file:///home/ann";
    QTest::newRow("tilde path") << "~/Documents" << "" << "file:///home/ann/Documents";
    QTest::newRow("relative") << "Work" << "file:///home/ann/Documents" << "file:///home/ann/Documents/Work";
    QTest::newRow("relative up") << ".." << "file:///home/ann/Documents" << "file:///home/ann";
    QTest::newRow("spaces trimmed") << "  /tmp  " << "" << "file:///tmp";
    QTest::newRow("url") << "smb://nas/share" << "" << "smb://nas/share";
    QTest::newRow("windows share") << "\\\\nas\\share\\docs" << "" << "smb://nas/share/docs";
    QTest::newRow("page name") << "This PC" << "" << "raye:thispc";
    QTest::newRow("page name any case") << "recycle bin" << "" << "trash:/";
    QTest::newRow("home page") << "Home" << "" << "raye:home";
    QTest::newRow("network") << "Network" << "" << "smb:/";
    QTest::newRow("page url") << "raye:gallery" << "" << "raye:gallery";
    QTest::newRow("empty") << "   " << "" << "";
    QTest::newRow("relative in a page") << "Work" << "raye:thispc" << "";
}

void RayeLocationNamesTest::typedText()
{
    QFETCH(QString, typed);
    QFETCH(QString, current);
    QFETCH(QString, expected);
    const LocationNames names(annContext());
    QCOMPARE(names.fromEditText(typed, QUrl(current)).toString(), expected);
}

void RayeLocationNamesTest::frenchPageNamesCanBeTyped()
{
    if (!useLanguage("fr")) {
        QSKIP("no fr_FR.UTF-8 locale on this system");
    }
    const LocationNames names(annContext());
    QCOMPARE(names.fromEditText(QStringLiteral("Ce PC")).toString(), QStringLiteral("raye:thispc"));
    QCOMPARE(names.fromEditText(QStringLiteral("corbeille")).toString(), QStringLiteral("trash:/"));
    // English names keep working in French, as on Windows.
    QCOMPARE(names.fromEditText(QStringLiteral("This PC")).toString(), QStringLiteral("raye:thispc"));
}

void RayeLocationNamesTest::thisPcListsKnownFoldersAndDrives()
{
    const LocationNames names(annContext());
    const QList<Crumb> children = names.pageChildren(thisPcUrl());
    QCOMPARE(texts(children),
             (QStringList{"Desktop", "Documents", "Downloads", "Music", "Pictures", "Videos", "Raye", "USB Drive", "SD card", "Holiday photos"}));
    for (const Crumb &c : children) {
        QVERIFY(c.isFolder);
        QVERIFY(c.url.isLocalFile());
    }
    QVERIFY(names.pageChildren(QUrl(QStringLiteral("file:///home/ann"))).isEmpty());
    QVERIFY(names.pageChildren(homePageUrl()).isEmpty());
}

void RayeLocationNamesTest::missingDrivesStillShowRaye()
{
    Context c = annContext();
    c.drives.clear();
    const LocationNames names(c);
    QCOMPARE(texts(names.crumbs(QUrl(QStringLiteral("file:///etc")))), (QStringList{"This PC", "Raye", "etc"}));
}

void RayeLocationNamesTest::archiveCutByTheFileWhenAsked()
{
    Context c = annContext();
    // "backup" has no archive extension; the worker knows it is a file.
    c.isFile = [](const QString &path) {
        return path == QLatin1String("/home/ann/backup");
    };
    const LocationNames names(c);
    const QList<Crumb> crumbs = names.crumbs(QUrl(QStringLiteral("rayearchive:/home/ann/backup/etc")));
    QCOMPARE(texts(crumbs), (QStringList{"This PC", "Ann", "backup", "etc"}));
    QCOMPARE(crumbs.at(2).kind, Kind::Archive);
}

void RayeLocationNamesTest::driveListerIsUsed()
{
    setDriveLister([]() {
        return QList<Drive>{{QStringLiteral("/"), QStringLiteral("Raye"), QStringLiteral("drive-harddisk-system"), false},
                            {QStringLiteral("/mnt/win"), QStringLiteral("Windows"), QStringLiteral("drive-harddisk"), false}};
    });
    QCOMPARE(listDrives().size(), 2);
    Context c = annContext();
    c.drives = listDrives();
    const LocationNames names(c);
    QCOMPARE(texts(names.crumbs(QUrl::fromLocalFile(QStringLiteral("/mnt/win/Users")))), (QStringList{"This PC", "Windows", "Users"}));
    setDriveLister({});
    QVERIFY(!listDrives().isEmpty()); // back to the mounted volumes
}

QTEST_GUILESS_MAIN(RayeLocationNamesTest)

#include "test_locationnames.moc"
