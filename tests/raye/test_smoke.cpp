/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Smoke test of the Raye Files build: the private library carries the Raye
// name, the version-control library is the system one (so dolphin-plugins
// keeps loading), the desktop file is the Raye one, and the raye-files
// catalog is built and found.

#include "kitemviews/kfileitemmodel.h"
#include "views/versioncontrol/kversioncontrolplugin.h"

#include <KLocalizedString>

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QStringList>
#include <QTest>

#include <clocale>
#include <link.h>

namespace
{
// Every shared library this process has loaded. Reading the symbol's own
// address does not work for exported data: the dynamic linker copies it
// into the executable, and we would always be told "the executable".
QStringList loadedLibraries()
{
    QStringList names;
    dl_iterate_phdr(
        [](struct dl_phdr_info *info, size_t, void *data) -> int {
            if (info->dlpi_name && info->dlpi_name[0] != '\0') {
                static_cast<QStringList *>(data)->append(QString::fromLocal8Bit(info->dlpi_name));
            }
            return 0;
        },
        &names);
    return names;
}

QString loadedLibraryMatching(const QString &fileNamePrefix)
{
    const QStringList libraries = loadedLibraries();
    for (const QString &library : libraries) {
        if (QFileInfo(library).fileName().startsWith(fileNamePrefix)) {
            return library;
        }
    }
    return {};
}

// Reads a desktop file as plain key=value lines of its [Desktop Entry] group,
// localized keys included as written ("Name[fr]").
QMap<QString, QString> readDesktopEntry(const QString &path)
{
    QMap<QString, QString> entries;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return entries;
    }
    bool inEntry = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('['))) {
            inEntry = (line == QLatin1String("[Desktop Entry]"));
            continue;
        }
        const int equal = line.indexOf(QLatin1Char('='));
        if (inEntry && equal > 0) {
            entries.insert(line.left(equal).trimmed(), line.mid(equal + 1).trimmed());
        }
    }
    return entries;
}
}

class RayeSmokeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void privateLibraryHasRayeName();
    void versionControlLibraryIsTheSystemOne();
    void desktopFileIsFiles();
    void desktopFileHasOnlyEnglishAndFrench();
    void catalogTranslatesAboutStrings_data();
    void catalogTranslatesAboutStrings();
};

void RayeSmokeTest::initTestCase()
{
    // Before any lookup: the catalog is cached the first time it is used.
    KLocalizedString::addDomainLocaleDir("raye-files", QStringLiteral(RAYE_FILES_BINARY_DIR "/locale"));
}

void RayeSmokeTest::privateLibraryHasRayeName()
{
    // The library is really ours and really used.
    KFileItemModel model;
    QCOMPARE(model.count(), 0);

    const QString path = loadedLibraryMatching(QStringLiteral("libraye-files-private.so"));
    QVERIFY2(!path.isEmpty(), qPrintable(QStringLiteral("libraye-files-private is not loaded; loaded: ") + loadedLibraries().join(QLatin1Char(' '))));

    const QStringList libraries = loadedLibraries();
    for (const QString &library : libraries) {
        QVERIFY2(!QFileInfo(library).fileName().startsWith(QLatin1String("libdolphinprivate.so")), qPrintable(library));
    }
}

void RayeSmokeTest::versionControlLibraryIsTheSystemOne()
{
    // Touching the type makes sure the library is pulled in, not optimised out.
    QVERIFY(KVersionControlPlugin::staticMetaObject.className() != nullptr);

    const QString path = loadedLibraryMatching(QStringLiteral("libdolphinvcs.so"));
    QVERIFY2(!path.isEmpty(), qPrintable(QStringLiteral("libdolphinvcs is not loaded; loaded: ") + loadedLibraries().join(QLatin1Char(' '))));

    // It is the system library, not a copy of ours: a plugin built against
    // the system one has to end up in the same code.
    QVERIFY2(!QFileInfo(path).absoluteFilePath().startsWith(QStringLiteral(RAYE_FILES_BINARY_DIR)), qPrintable(path));
    QVERIFY2(!QFile::exists(QStringLiteral(RAYE_FILES_BINARY_DIR "/bin/libdolphinvcs.so.6")), "the build tree made its own copy of libdolphinvcs");
}

void RayeSmokeTest::desktopFileIsFiles()
{
    const auto entry = readDesktopEntry(QStringLiteral(RAYE_FILES_SOURCE_DIR "/src/org.kde.dolphin.desktop"));
    QVERIFY(!entry.isEmpty());

    QCOMPARE(entry.value(QStringLiteral("Type")), QStringLiteral("Application"));
    QCOMPARE(entry.value(QStringLiteral("Name")), QStringLiteral("Files"));
    QCOMPARE(entry.value(QStringLiteral("Name[fr]")), QStringLiteral("Fichiers"));
    QCOMPARE(entry.value(QStringLiteral("GenericName")), QStringLiteral("File Explorer"));
    QCOMPARE(entry.value(QStringLiteral("GenericName[fr]")), QStringLiteral("Explorateur de fichiers"));
    QCOMPARE(entry.value(QStringLiteral("Icon")), QStringLiteral("system-file-manager"));

    // What other parts of the desktop rely on stays as it was.
    QCOMPARE(entry.value(QStringLiteral("Exec")), QStringLiteral("dolphin %u"));
    QCOMPARE(entry.value(QStringLiteral("X-DBUS-ServiceName")), QStringLiteral("org.kde.dolphin"));
    QCOMPARE(entry.value(QStringLiteral("StartupWMClass")), QStringLiteral("dolphin"));
    QCOMPARE(entry.value(QStringLiteral("X-KDE-Shortcuts")), QStringLiteral("Meta+E"));
    QVERIFY(entry.value(QStringLiteral("MimeType")).split(QLatin1Char(';')).contains(QStringLiteral("inode/directory")));
    QVERIFY(entry.value(QStringLiteral("Categories")).split(QLatin1Char(';')).contains(QStringLiteral("FileManager")));
}

void RayeSmokeTest::desktopFileHasOnlyEnglishAndFrench()
{
    const auto entry = readDesktopEntry(QStringLiteral(RAYE_FILES_SOURCE_DIR "/src/org.kde.dolphin.desktop"));
    QVERIFY(!entry.isEmpty());

    for (auto it = entry.cbegin(); it != entry.cend(); ++it) {
        const QString &key = it.key();
        if (key.contains(QLatin1Char('['))) {
            QVERIFY2(key.endsWith(QLatin1String("[fr]")), qPrintable(key));
        }
        // The user never reads the old name; Exec and the D-Bus name are ids.
        if (key != QLatin1String("X-DBUS-ServiceName") && key != QLatin1String("Exec") && key != QLatin1String("StartupWMClass")) {
            QVERIFY2(!it.value().contains(QLatin1String("dolphin"), Qt::CaseInsensitive), qPrintable(key + QLatin1Char('=') + it.value()));
        }
    }
}

void RayeSmokeTest::catalogTranslatesAboutStrings_data()
{
    QTest::addColumn<QString>("language");
    QTest::addColumn<QString>("source");
    QTest::addColumn<QString>("expected");

    QTest::newRow("en name") << QStringLiteral("en_US") << QStringLiteral("Files") << QStringLiteral("Files");
    QTest::newRow("fr name") << QStringLiteral("fr") << QStringLiteral("Files") << QStringLiteral("Fichiers");
    QTest::newRow("en about") << QStringLiteral("en_US") << QStringLiteral("Files, based on Dolphin") << QStringLiteral("Files, based on Dolphin");
    QTest::newRow("fr about") << QStringLiteral("fr") << QStringLiteral("Files, based on Dolphin") << QStringLiteral("Fichiers, basé sur Dolphin");
}

void RayeSmokeTest::catalogTranslatesAboutStrings()
{
    QFETCH(QString, language);
    QFETCH(QString, source);
    QFETCH(QString, expected);

    QVERIFY2(QFile::exists(QStringLiteral(RAYE_FILES_BINARY_DIR "/locale/fr/LC_MESSAGES/raye-files.mo")), "raye-files.mo was not built");

    // gettext hands back the English string whenever the C library is in the
    // C locale, whatever the language list says. The locale has to be real.
    if (language == QLatin1String("fr")) {
        if (!std::setlocale(LC_MESSAGES, "fr_FR.UTF-8")) {
            if (qEnvironmentVariableIsSet("RAYE_TESTS_STRICT")) {
                QFAIL("fr_FR.UTF-8 is not generated on this machine");
            }
            QSKIP("fr_FR.UTF-8 is not generated on this machine");
        }
    } else {
        std::setlocale(LC_MESSAGES, "C.UTF-8");
    }
    KLocalizedString::setLanguages({language});

    QCOMPARE(ki18nd("raye-files", source.toUtf8().constData()).toString(), expected);
}

QTEST_MAIN(RayeSmokeTest)

#include "test_smoke.moc"
