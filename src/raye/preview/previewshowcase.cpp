/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The render harness: the Details and Preview panes on sample files, 320
// wide on the right of the window as in Files. Nothing here runs outside
// the harness.

#include "preview/detailspane.h"
#include "preview/previewpane.h"

#include "ui/showcase.h"

#include <KLocalizedString>

#include <QDirIterator>
#include <QFile>
#include <QHBoxLayout>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QTimer>

namespace
{
const char samplePdf[] =
    "%PDF-1.4\n"
    "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
    "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n"
    "3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >> endobj\n"
    "4 0 obj << /Length 87 >> stream\nBT /F1 22 Tf 30 340 Td (Quarterly report) Tj 0 -40 Td /F1 12 Tf (Raye Files preview) Tj ET\nendstream endobj\n"
    "5 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> endobj\n"
    "trailer << /Root 1 0 R >>\n%%EOF\n";

QString samples()
{
    static QTemporaryDir dir;
    static bool made = false;
    if (made) {
        return dir.path();
    }
    made = true;
    auto write = [&](const char *name, const QByteArray &bytes) {
        QFile f(dir.filePath(QLatin1String(name)));
        if (f.open(QIODevice::WriteOnly)) {
            f.write(bytes);
        }
    };
    write("Meeting notes.txt",
          "Weekly meeting\n\n- Ship the new navigation pane\n- Review the preview pane\n- Plan the gallery\n\nNext meeting: Thursday 10:00\n");
    write("Report.pdf", QByteArray(samplePdf));
    write("song.mp3", QByteArray("ID3\x03\x00\x00\x00\x00\x00\x00", 10) + QByteArray("\xff\xfb\x90\x64", 4) + QByteArray(400, '\0'));
    write("Archive.zip", QByteArray("PK\x05\x06", 4) + QByteArray(18, '\0'));
    QImage image(1200, 800, QImage::Format_RGB32);
    {
        QPainter p(&image);
        QLinearGradient sky(0, 0, 0, 800);
        sky.setColorAt(0, QColor(26, 16, 40));
        sky.setColorAt(0.6, QColor(123, 60, 255));
        sky.setColorAt(1, QColor(192, 64, 255));
        p.fillRect(image.rect(), sky);
        p.setBrush(QColor(243, 234, 255));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(850, 260), 90, 90);
        p.setBrush(QColor(14, 10, 23));
        QPolygonF hills;
        hills << QPointF(0, 800) << QPointF(0, 560) << QPointF(300, 470) << QPointF(620, 600) << QPointF(900, 500) << QPointF(1200, 620) << QPointF(1200, 800);
        p.drawPolygon(hills);
    }
    image.save(dir.filePath(QStringLiteral("Sunset.png")));
    QDirIterator it(QStringLiteral("/usr/share/fonts"), {QStringLiteral("NotoSans-Regular.ttf"), QStringLiteral("*.ttf")}, QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) {
        QFile::copy(it.next(), dir.filePath(QStringLiteral("Sample font.ttf")));
    }
    return dir.path();
}

KFileItem sample(const char *name)
{
    KFileItem item(QUrl::fromLocalFile(samples() + QLatin1Char('/') + QLatin1String(name)));
    item.determineMimeType();
    return item;
}

// The pane on the right, the rest of the window left empty like a view.
QWidget *frame(QWidget *parent, QWidget *pane)
{
    auto *box = new QWidget(parent);
    auto *layout = new QHBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch(1);
    pane->setParent(box);
    pane->setFixedWidth(320);
    layout->addWidget(pane);
    return box;
}

QWidget *preview(QWidget *parent, const char *name)
{
    auto *pane = new Raye::Preview::PreviewPane;
    QWidget *box = frame(parent, pane);
    const KFileItem item = name ? sample(name) : KFileItem();
    // After the first layout, so the pane knows its size.
    QTimer::singleShot(0, pane, [pane, item] {
        if (item.isNull()) {
            pane->clear();
        } else {
            pane->showItem(item);
        }
    });
    return box;
}

const bool registered = [] {
    using Raye::Ui::Showcase::add;
    add(QStringLiteral("DetailsPane"), QStringLiteral("Details pane, one picture"), [](QWidget *parent) {
        auto *pane = new Raye::Preview::DetailsPane;
        QWidget *box = frame(parent, pane);
        pane->showItems({sample("Sunset.png")}, KFileItem());
        return box;
    });
    add(QStringLiteral("DetailsPaneMany"), QStringLiteral("Details pane, three items"), [](QWidget *parent) {
        auto *pane = new Raye::Preview::DetailsPane;
        QWidget *box = frame(parent, pane);
        pane->showItems({sample("Sunset.png"), sample("Report.pdf"), sample("Meeting notes.txt")}, KFileItem());
        return box;
    });
    add(QStringLiteral("PreviewPane-pdf"), QStringLiteral("Preview pane, a PDF"), [](QWidget *parent) {
        return preview(parent, "Report.pdf");
    });
    add(QStringLiteral("PreviewPane-text"), QStringLiteral("Preview pane, a text file"), [](QWidget *parent) {
        return preview(parent, "Meeting notes.txt");
    });
    add(QStringLiteral("PreviewPane-image"), QStringLiteral("Preview pane, a picture"), [](QWidget *parent) {
        return preview(parent, "Sunset.png");
    });
    add(QStringLiteral("PreviewPane-font"), QStringLiteral("Preview pane, a font"), [](QWidget *parent) {
        return preview(parent, "Sample font.ttf");
    });
    add(QStringLiteral("PreviewPane-audio"), QStringLiteral("Preview pane, a song"), [](QWidget *parent) {
        return preview(parent, "song.mp3");
    });
    add(QStringLiteral("PreviewPane-none"), QStringLiteral("Preview pane, nothing to show"), [](QWidget *parent) {
        return preview(parent, "Archive.zip");
    });
    return true;
}();
}
