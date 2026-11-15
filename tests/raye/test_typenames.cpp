/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Windows type names (explorer spec 3.6): the table, its aliases, the
// shared-mime-info fallback, and French from the raye-files catalog.

#include "typenames/typenames.h"

#include <KLocalizedString>

#include <QMimeDatabase>
#include <QSet>
#include <QTest>

#include <clocale>

class TypeNamesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setLanguages({QStringLiteral("en")});
    }

    void names_data()
    {
        QTest::addColumn<QString>("mime");
        QTest::addColumn<QString>("expected");
        QTest::newRow("folder") << "inode/directory" << "File folder";
        QTest::newRow("text") << "text/plain" << "Text document";
        QTest::newRow("png") << "image/png" << "PNG file";
        QTest::newRow("jpeg") << "image/jpeg" << "JPG file";
        QTest::newRow("zip") << "application/zip" << "Compressed (zipped) folder";
        QTest::newRow("exe") << "application/x-ms-dos-executable" << "Application";
        QTest::newRow("elf") << "application/x-executable" << "Application";
        QTest::newRow("msi") << "application/x-msi" << "Windows Installer package";
        QTest::newRow("pdf") << "application/pdf" << "PDF document";
        QTest::newRow("docx") << "application/vnd.openxmlformats-officedocument.wordprocessingml.document" << "Word document";
        QTest::newRow("mp3") << "audio/mpeg" << "MP3 file";
        QTest::newRow("mkv") << "video/x-matroska" << "MKV file";
        QTest::newRow("ttf") << "font/ttf" << "TrueType font file";
        QTest::newRow("tar.gz") << "application/x-compressed-tar" << "TAR.GZ archive";
        QTest::newRow("shortcut") << "application/x-desktop" << "Shortcut";
    }

    void names()
    {
        QFETCH(QString, mime);
        QFETCH(QString, expected);
        QCOMPARE(Raye::TypeNames::name(mime), expected);
        QVERIFY(Raye::TypeNames::hasName(mime));
    }

    // An old name of a type finds the table row of its real name.
    void aliases()
    {
        QMimeDatabase db;
        if (db.mimeTypeForName(QStringLiteral("image/x-png")).name() != QLatin1String("image/png")) {
            QSKIP("this mime database has no image/x-png alias");
        }
        QCOMPARE(Raye::TypeNames::name(QStringLiteral("image/x-png")), QStringLiteral("PNG file"));
        QCOMPARE(Raye::TypeNames::name(QStringLiteral("application/x-zip-compressed")), QStringLiteral("Compressed (zipped) folder"));
    }

    void fallback()
    {
        QVERIFY(!Raye::TypeNames::hasName(QStringLiteral("application/x-gnumeric")));
        QCOMPARE(Raye::TypeNames::name(QStringLiteral("application/x-gnumeric"), QStringLiteral("Given")), QStringLiteral("Given"));
        const QMimeType type = QMimeDatabase().mimeTypeForName(QStringLiteral("application/x-gnumeric"));
        QCOMPARE(Raye::TypeNames::name(QStringLiteral("application/x-gnumeric")), type.comment());
        QVERIFY(Raye::TypeNames::name(QString()).isEmpty());
        QVERIFY(Raye::TypeNames::name(QStringLiteral("no/such-type")).isEmpty());
    }

    // The spec asks for the 60 commonest types; every row is a type the
    // database knows, and no type is written twice.
    void table()
    {
        const QStringList types = Raye::TypeNames::mimeTypes();
        QVERIFY2(types.size() >= 60, qPrintable(QString::number(types.size())));
        QCOMPARE(QSet<QString>(types.begin(), types.end()).size(), types.size());
        QMimeDatabase db;
        for (const QString &t : types) {
            QVERIFY2(db.mimeTypeForName(t).isValid(), qPrintable(t));
            QVERIFY2(!Raye::TypeNames::name(t).isEmpty(), qPrintable(t));
        }
    }

    void french()
    {
        if (!std::setlocale(LC_MESSAGES, "fr_FR.UTF-8")) {
            if (qEnvironmentVariableIsSet("RAYE_TESTS_STRICT")) {
                QFAIL("no fr_FR locale: the French strings are not checked");
            }
            QSKIP("no fr_FR locale here");
        }
        KLocalizedString::addDomainLocaleDir("raye-files", QStringLiteral(RAYE_FILES_BINARY_DIR "/locale"));
        KLocalizedString::setLanguages({QStringLiteral("fr")});
        const QString text = Raye::TypeNames::name(QStringLiteral("text/plain"));
        KLocalizedString::setLanguages({QStringLiteral("en")});
        std::setlocale(LC_MESSAGES, "C.UTF-8");
        QCOMPARE(text, QStringLiteral("Document texte"));
        KLocalizedString::setLanguages({QStringLiteral("fr")});
        std::setlocale(LC_MESSAGES, "fr_FR.UTF-8");
        QCOMPARE(Raye::TypeNames::name(QStringLiteral("application/zip")), QStringLiteral("Dossier compressé"));
        QCOMPARE(Raye::TypeNames::name(QStringLiteral("inode/directory")), QStringLiteral("Dossier de fichiers"));
        KLocalizedString::setLanguages({QStringLiteral("en")});
        std::setlocale(LC_MESSAGES, "C.UTF-8");
    }
};

QTEST_GUILESS_MAIN(TypeNamesTest)

#include "test_typenames.moc"
