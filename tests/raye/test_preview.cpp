/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The Details and Preview panes (explorer spec 3.8): which loader each
// type gets, what the Details pane lists, and the Preview pane opening real
// fixtures (PDF, text, picture, font, audio) with its load time.

#include "preview/detailsmodel.h"
#include "preview/detailspane.h"
#include "preview/previewloader.h"
#include "preview/previewpane.h"
#include "preview/rightpane.h"

#include <KFileItem>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KParts/ReadWritePart>
#include <KSharedConfig>

#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Raye::Preview;

namespace
{
// A one-page PDF with a line of text, small enough to write by hand.
const char pdf[] =
    "%PDF-1.4\n"
    "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
    "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n"
    "3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >> endobj\n"
    "4 0 obj << /Length 44 >> stream\nBT /F1 24 Tf 40 100 Td (Raye preview) Tj ET\nendstream endobj\n"
    "5 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> endobj\n"
    "trailer << /Root 1 0 R >>\n%%EOF\n";

QString anyFont()
{
    QDirIterator it(QStringLiteral("/usr/share/fonts"), {QStringLiteral("*.ttf")}, QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext() ? it.next() : QString();
}

KFileItem itemFor(const QString &path)
{
    KFileItem item(QUrl::fromLocalFile(path));
    item.determineMimeType();
    return item;
}
}

class PreviewTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setLanguages({QStringLiteral("en")});
        QVERIFY(m_dir.isValid());
        auto write = [this](const QString &name, const QByteArray &bytes) {
            QFile f(m_dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(bytes);
        };
        write(QStringLiteral("notes.txt"), "Hello from Raye.\nSecond line.\n");
        write(QStringLiteral("doc.pdf"), QByteArray(pdf));
        // An MP3 frame header after an empty ID3 tag: enough for the type.
        write(QStringLiteral("song.mp3"), QByteArray("ID3\x03\x00\x00\x00\x00\x00\x00", 10) + QByteArray("\xff\xfb\x90\x64", 4) + QByteArray(400, '\0'));
        QImage image(320, 200, QImage::Format_RGB32);
        image.fill(QColor(123, 60, 255));
        QVERIFY(image.save(m_dir.filePath(QStringLiteral("photo.png"))));
        const QString font = anyFont();
        if (!font.isEmpty()) {
            QVERIFY(QFile::copy(font, m_dir.filePath(QStringLiteral("sample.ttf"))));
        }
    }

    void loaders_data()
    {
        QTest::addColumn<QString>("mime");
        QTest::addColumn<int>("kind");
        QTest::addColumn<QString>("part");
        QTest::newRow("pdf") << "application/pdf" << int(Kind::Document) << "okularpart";
        QTest::newRow("epub") << "application/epub+zip" << int(Kind::Document) << "okularpart";
        QTest::newRow("txt") << "text/plain" << int(Kind::Text) << "katepart";
        QTest::newRow("c++") << "text/x-c++src" << int(Kind::Text) << "katepart";
        QTest::newRow("json") << "application/json" << int(Kind::Text) << "katepart";
        QTest::newRow("png") << "image/png" << int(Kind::Image) << "";
        QTest::newRow("jpeg") << "image/jpeg" << int(Kind::Image) << "";
        QTest::newRow("ttf") << "font/ttf" << int(Kind::Font) << "";
        QTest::newRow("mp3") << "audio/mpeg" << int(Kind::Audio) << "";
        QTest::newRow("mkv") << "video/x-matroska" << int(Kind::Video) << "";
        QTest::newRow("docx") << "application/vnd.openxmlformats-officedocument.wordprocessingml.document" << int(Kind::Thumbnail) << "";
        QTest::newRow("zip") << "application/zip" << int(Kind::None) << "";
        QTest::newRow("exe") << "application/x-ms-dos-executable" << int(Kind::None) << "";
        QTest::newRow("folder") << "inode/directory" << int(Kind::None) << "";
        QTest::newRow("unknown") << "no/such-type" << int(Kind::None) << "";
    }

    void loaders()
    {
        QFETCH(QString, mime);
        QFETCH(int, kind);
        QFETCH(QString, part);
        const QStringList all = {QStringLiteral("okularpart"), QStringLiteral("katepart"), QStringLiteral("gvpart")};
        const Loader loader = loaderFor(mime, all);
        QCOMPARE(int(loader.kind), kind);
        QCOMPARE(loader.partId, part);
    }

    // Without the part: documents fall back to their thumbnail, text to the
    // plain view of Files.
    void missingParts()
    {
        QCOMPARE(int(loaderFor(QStringLiteral("application/pdf"), {}).kind), int(Kind::Thumbnail));
        const Loader text = loaderFor(QStringLiteral("text/plain"), {});
        QCOMPARE(int(text.kind), int(Kind::Text));
        QVERIFY(text.partId.isEmpty());
        // Parts of the same family are shared.
        QCOMPARE(loaderFor(QStringLiteral("text/plain"), {QStringLiteral("katepart")}).family,
                 loaderFor(QStringLiteral("text/x-python"), {QStringLiteral("katepart")}).family);
    }

    void locations()
    {
        const QString home = QStringLiteral("/home/ana");
        QCOMPARE(locationText(QUrl::fromLocalFile(home), home), QStringLiteral("ana"));
        QCOMPARE(locationText(QUrl::fromLocalFile(home + QStringLiteral("/Documents/Work")), home), QStringLiteral("Documents › Work"));
        QCOMPARE(locationText(QUrl::fromLocalFile(QStringLiteral("/usr/share")), home), QStringLiteral("Raye › usr › share"));
        QCOMPARE(locationText(QUrl(QStringLiteral("smb://nas/share/")), home), QStringLiteral("smb://nas/share"));
        // A password in an address is never shown.
        QVERIFY(!locationText(QUrl(QStringLiteral("sftp://ana:secret@host/x")), home).contains(QLatin1String("secret")));
        QVERIFY(!locationText(QUrl::fromLocalFile(home + QStringLiteral("/x")), home).contains(QLatin1String("/home")));
    }

    void detailsOne()
    {
        const KFileItem item = itemFor(m_dir.filePath(QStringLiteral("notes.txt")));
        const Details d = detailsFor({item}, KFileItem(), m_dir.path());
        QCOMPARE(d.title, QStringLiteral("notes"));
        QVERIFY(!d.multiple);
        QCOMPARE(d.rows.at(0).label, QStringLiteral("Type"));
        QCOMPARE(d.rows.at(0).value, QStringLiteral("Text document"));
        QCOMPARE(d.rows.at(1).label, QStringLiteral("Size"));
        QVERIFY2(d.rows.at(1).value.contains(QLatin1String("30 bytes")), qPrintable(d.rows.at(1).value));
        QCOMPARE(d.rows.at(2).label, QStringLiteral("Location"));
        QCOMPARE(d.rows.at(2).link.adjusted(QUrl::StripTrailingSlash), QUrl::fromLocalFile(m_dir.path()));
        QCOMPARE(d.rows.at(3).label, QStringLiteral("Date modified"));

        const Details picture = detailsFor({itemFor(m_dir.filePath(QStringLiteral("photo.png")))}, KFileItem(), m_dir.path());
        bool dimensions = false;
        for (const DetailRow &row : picture.rows) {
            dimensions = dimensions || (row.label == QLatin1String("Dimensions") && row.value == QStringLiteral("320 × 200"));
        }
        QVERIFY(dimensions);
    }

    void detailsMany()
    {
        const KFileItemList items = {itemFor(m_dir.filePath(QStringLiteral("notes.txt"))), itemFor(m_dir.filePath(QStringLiteral("doc.pdf"))),
                                     itemFor(m_dir.filePath(QStringLiteral("photo.png")))};
        const Details d = detailsFor(items, KFileItem(), m_dir.path());
        QVERIFY(d.multiple);
        QCOMPARE(d.title, QStringLiteral("3 items selected"));
        QCOMPARE(d.rows.at(0).label, QStringLiteral("Total size"));
        QCOMPARE(d.rows.at(1).label, QStringLiteral("Date modified"));
    }

    void detailsFolder()
    {
        const KFileItem folder = itemFor(m_dir.path());
        const Details d = detailsFor({}, folder, m_dir.path());
        QCOMPARE(d.rows.at(0).value, QStringLiteral("File folder"));
        for (const DetailRow &row : d.rows) {
            QVERIFY(row.label != QLatin1String("Size"));
        }
        QCOMPARE(dateRange(QDateTime(QDate(2026, 5, 12), QTime(9, 0)), QDateTime(QDate(2026, 5, 12), QTime(18, 0))),
                 QLocale().toString(QDate(2026, 5, 12), QLocale::ShortFormat));
    }

    void detailsPane()
    {
        DetailsPane pane;
        pane.resize(320, 600);
        pane.showItems({itemFor(m_dir.filePath(QStringLiteral("notes.txt")))}, KFileItem());
        QCOMPARE(pane.details().title, QStringLiteral("notes"));
        pane.showItems({}, itemFor(m_dir.path()));
        QCOMPARE(pane.details().rows.at(0).value, QStringLiteral("File folder"));
    }

    void previewImage()
    {
        PreviewPane pane;
        pane.resize(320, 480);
        pane.show();
        pane.showItem(itemFor(m_dir.filePath(QStringLiteral("photo.png"))));
        QCOMPARE(int(pane.currentKind()), int(Kind::Image));
        QVERIFY(pane.message().isEmpty());
        qInfo() << "picture shown in" << pane.lastLoadTime() << "ms";
        QVERIFY(pane.lastLoadTime() >= 0 && pane.lastLoadTime() < 200);
    }

    void previewText()
    {
        PreviewPane pane;
        pane.resize(320, 480);
        pane.show();
        pane.showItem(itemFor(m_dir.filePath(QStringLiteral("notes.txt"))));
        QCOMPARE(int(pane.currentKind()), int(Kind::Text));
        if (installedParts().contains(QLatin1String("katepart"))) {
            QVERIFY(pane.currentPart());
            QCOMPARE(pane.currentPart()->property("url").toUrl(), QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("notes.txt"))));
            // Read-only: nobody types into a preview by mistake.
            auto *document = qobject_cast<KParts::ReadWritePart *>(pane.currentPart());
            QVERIFY(document);
            QVERIFY(!document->isReadWrite());
        }
        qInfo() << "text shown in" << pane.lastLoadTime() << "ms";
        // The part is kept: a second file of the family is faster.
        QObject *part = pane.currentPart();
        QFile other(m_dir.filePath(QStringLiteral("other.txt")));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.write("more\n");
        other.close();
        pane.showItem(itemFor(m_dir.filePath(QStringLiteral("other.txt"))));
        QCOMPARE(pane.currentPart(), part);
        qInfo() << "second text shown in" << pane.lastLoadTime() << "ms";
        if (part) {
            QVERIFY(pane.lastLoadTime() < 200);
        }
    }

    void previewPdf()
    {
        if (!installedParts().contains(QLatin1String("okularpart"))) {
            QSKIP("Okular's part is not installed here (it is in Raye)");
        }
        PreviewPane pane;
        pane.resize(320, 480);
        pane.show();
        pane.showItem(itemFor(m_dir.filePath(QStringLiteral("doc.pdf"))));
        QCOMPARE(int(pane.currentKind()), int(Kind::Document));
        QVERIFY(pane.currentPart());
        qInfo() << "PDF shown in" << pane.lastLoadTime() << "ms";
    }

    void previewFont()
    {
        if (!QFile::exists(m_dir.filePath(QStringLiteral("sample.ttf")))) {
            QSKIP("no TrueType font on this system");
        }
        PreviewPane pane;
        pane.resize(320, 480);
        pane.show();
        pane.showItem(itemFor(m_dir.filePath(QStringLiteral("sample.ttf"))));
        QCOMPARE(int(pane.currentKind()), int(Kind::Font));
        QVERIFY(pane.message().isEmpty());
    }

    void previewAudioAndNothing()
    {
        PreviewPane pane;
        pane.resize(320, 480);
        pane.show();
        pane.showItem(itemFor(m_dir.filePath(QStringLiteral("song.mp3"))));
        QCOMPARE(int(pane.currentKind()), int(Kind::Audio));
        pane.showItem(itemFor(m_dir.path()));
        QCOMPARE(pane.message(), QStringLiteral("Select a file to preview."));
        QFile zip(m_dir.filePath(QStringLiteral("a.zip")));
        QVERIFY(zip.open(QIODevice::WriteOnly));
        zip.write("PK\x05\x06" + QByteArray(18, '\0'));
        zip.close();
        pane.showItem(itemFor(m_dir.filePath(QStringLiteral("a.zip"))));
        QCOMPARE(pane.message(), QStringLiteral("No preview available."));
    }

    // One pane at a time, and the choice is kept.
    void rightPane()
    {
        RightPane pane;
        pane.setMode(RightPane::Details);
        pane.setMode(RightPane::Preview);
        QCOMPARE(pane.mode(), RightPane::Preview);
        QCOMPARE(KSharedConfig::openConfig(QStringLiteral("dolphinrc"))->group(QStringLiteral("Raye")).readEntry("PreviewPane"), QStringLiteral("preview"));
        RightPane again;
        QCOMPARE(again.mode(), RightPane::Preview);
        QCOMPARE(RightPane::modeFromKey(QStringLiteral("none")), RightPane::Details);
        QCOMPARE(RightPane::modeKey(RightPane::Details), QStringLiteral("details"));
        pane.setMode(RightPane::Details);
    }

private:
    QTemporaryDir m_dir;
};

QTEST_MAIN(PreviewTest)

#include "test_preview.moc"
