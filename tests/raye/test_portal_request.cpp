/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// What the Open and Save dialogs make of what the portal sends and of what a
// person types, without a window or a bus.

#include "raye/portal/request.h"

#include <QDir>
#include <QTest>

using namespace Raye::Portal;

class TestPortalRequest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void pathsLoseTheirNul();
    void labelsLoseTheirMnemonic_data();
    void labelsLoseTheirMnemonic();
    void globsAreSimplified_data();
    void globsAreSimplified();
    void fieldRoundTrip_data();
    void fieldRoundTrip();
    void typedNames_data();
    void typedNames();
    void namesResolve();
    void extensionAdded_data();
    void extensionAdded();
    void showsEverything();
    void displayName();
};

void TestPortalRequest::pathsLoseTheirNul()
{
    QCOMPARE(decodePath(QByteArray("/home/ann", 10)), QStringLiteral("/home/ann"));
    QCOMPARE(decodePath(QByteArray("/home/ann")), QStringLiteral("/home/ann"));
    QCOMPARE(decodePath(QByteArray("/home/\xc3\xa9t\xc3\xa9")), QString::fromUtf8("/home/\xc3\xa9t\xc3\xa9"));
    QCOMPARE(decodePath(QByteArray()), QString());
}

void TestPortalRequest::labelsLoseTheirMnemonic_data()
{
    QTest::addColumn<QString>("in");
    QTest::addColumn<QString>("out");
    QTest::newRow("gtk") << "_Open" << "Open";
    QTest::newRow("middle") << "Sa_ve" << "Save";
    QTest::newRow("plain") << "Attach" << "Attach";
    QTest::newRow("real underscore") << "a__b" << "a_b";
    QTest::newRow("empty") << "" << "";
}

void TestPortalRequest::labelsLoseTheirMnemonic()
{
    QFETCH(QString, in);
    QFETCH(QString, out);
    QCOMPARE(cleanLabel(in), out);
}

void TestPortalRequest::globsAreSimplified_data()
{
    QTest::addColumn<QString>("in");
    QTest::addColumn<QString>("out");
    QTest::newRow("both cases") << "*.[pP][nN][gG]" << "*.png";
    QTest::newRow("plain") << "*.png" << "*.png";
    QTest::newRow("two letters that differ stay") << "*.[ab]" << "*.[ab]";
    QTest::newRow("all") << "*" << "*";
}

void TestPortalRequest::globsAreSimplified()
{
    QFETCH(QString, in);
    QFETCH(QString, out);
    QCOMPARE(simplifyGlob(in), out);
}

void TestPortalRequest::fieldRoundTrip_data()
{
    QTest::addColumn<QStringList>("names");
    QTest::addColumn<QString>("field");
    QTest::newRow("one") << QStringList{"a b.txt"} << "a b.txt";
    QTest::newRow("two") << QStringList{"a.txt", "b c.txt"} << "\"a.txt\" \"b c.txt\"";
    // A quote in a name must not turn it into another name.
    QTest::newRow("starts with a quote") << QStringList{"\"report\".txt"} << "\"report\".txt";
    QTest::newRow("all in quotes") << QStringList{"\"a\""} << "\"\\\"a\\\"\"";
    QTest::newRow("one quote") << QStringList{"\"a.txt"} << "\"\\\"a.txt\"";
    QTest::newRow("quotes in several") << QStringList{"a\"b.txt", "c"} << "\"a\\\"b.txt\" \"c\"";
    QTest::newRow("backslash in several") << QStringList{"a\\b", "c\\"} << "\"a\\\\b\" \"c\\\\\"";
    QTest::newRow("space around") << QStringList{" a "} << "\" a \"";
}

void TestPortalRequest::fieldRoundTrip()
{
    QFETCH(QStringList, names);
    QFETCH(QString, field);
    QCOMPARE(fieldFromNames(names), field);
    QCOMPARE(namesFromField(field), names);
    QCOMPARE(namesFromField(QStringLiteral("  ")), QStringList());
    QCOMPARE(namesFromField(QStringLiteral("\"a.txt")), QStringList{QStringLiteral("a.txt")});
}

void TestPortalRequest::typedNames_data()
{
    QTest::addColumn<QString>("field");
    QTest::addColumn<QStringList>("names");
    QTest::newRow("quote then more") << "\"report\".txt" << QStringList{"\"report\".txt"};
    QTest::newRow("text between") << "\"a\" and \"b\"" << QStringList{"\"a\" and \"b\""};
    QTest::newRow("plain backslash") << "\"a\\b\" \"c\"" << QStringList{"a\\b", "c"};
    QTest::newRow("spaces between") << "  \"a\"   \"b\"  " << QStringList{"a", "b"};
}

void TestPortalRequest::typedNames()
{
    QFETCH(QString, field);
    QFETCH(QStringList, names);
    QCOMPARE(namesFromField(field), names);
}

void TestPortalRequest::namesResolve()
{
    const QUrl here = QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents"));
    QCOMPARE(resolveName(QStringLiteral("a.txt"), here), QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents/a.txt")));
    QCOMPARE(resolveName(QStringLiteral("../b.txt"), here), QUrl::fromLocalFile(QStringLiteral("/home/ann/b.txt")));
    QCOMPARE(resolveName(QStringLiteral("/etc/fstab"), here), QUrl::fromLocalFile(QStringLiteral("/etc/fstab")));
    QCOMPARE(resolveName(QStringLiteral("~/c"), here), QUrl::fromLocalFile(QDir::homePath() + QStringLiteral("/c")));
}

void TestPortalRequest::extensionAdded_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QStringList>("globs");
    QTest::addColumn<QString>("out");
    QTest::newRow("added") << "note" << QStringList{"*.txt"} << "note.txt";
    QTest::newRow("first of several") << "pic" << QStringList{"*.png", "*.jpg"} << "pic.png";
    QTest::newRow("kept") << "note.md" << QStringList{"*.txt"} << "note.md";
    QTest::newRow("all files") << "note" << QStringList{"*"} << "note";
    QTest::newRow("no type") << "note" << QStringList{} << "note";
    QTest::newRow("dot in the folder") << "folder.v1/report" << QStringList{"*.pdf"} << "folder.v1/report.pdf";
    QTest::newRow("dot in the name, folder too") << "folder.v1/report.old" << QStringList{"*.pdf"} << "folder.v1/report.old";
    QTest::newRow("hidden name") << "folder/.notes" << QStringList{"*.txt"} << "folder/.notes";
    QTest::newRow("mixed case glob") << "pic" << QStringList{"*.[pP][nN][gG]"} << "pic.png";
}

void TestPortalRequest::extensionAdded()
{
    QFETCH(QString, name);
    QFETCH(QStringList, globs);
    QFETCH(QString, out);
    FileType type;
    type.globs = globs;
    QCOMPARE(withTypeExtension(name, type), out);
}

void TestPortalRequest::showsEverything()
{
    QVERIFY(FileType{}.showsEverything());
    QVERIFY((FileType{QStringLiteral("All"), {QStringLiteral("*")}, {}}).showsEverything());
    QVERIFY(!(FileType{QStringLiteral("Text"), {QStringLiteral("*.txt")}, {}}).showsEverything());
    QVERIFY(!(FileType{QStringLiteral("Pics"), {}, {QStringLiteral("image/png")}}).showsEverything());
}

void TestPortalRequest::displayName()
{
    QCOMPARE((FileType{QStringLiteral("Pictures"), {QStringLiteral("*.png"), QStringLiteral("*.jpg")}, {}}).displayName(), QStringLiteral("Pictures (*.png, *.jpg)"));
    QCOMPARE((FileType{QStringLiteral("Pictures (*.png)"), {QStringLiteral("*.png")}, {}}).displayName(), QStringLiteral("Pictures (*.png)"));
    QCOMPARE((FileType{QStringLiteral("Pictures"), {}, {QStringLiteral("image/png")}}).displayName(), QStringLiteral("Pictures"));
}

QTEST_GUILESS_MAIN(TestPortalRequest)

#include "test_portal_request.moc"
