/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Known extensions hidden like Windows, programs and scripts always showing
// theirs, and the part of the name inline rename selects (explorer spec 3.6).

#include "kitemviews/kfileitemmodel.h"
#include "typenames/filenames.h"
#include "typenames/typenames.h"

#include <KLocalizedString>

#include <QElapsedTimer>
#include <QFile>
#include <QLocale>
#include <QMimeDatabase>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Raye::FileNames;

class ExtensionsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void display_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("isDir");
        QTest::addColumn<QString>("hidden"); // ShowExtensions=false
        QTest::newRow("pdf") << "report.pdf" << false << "report";
        QTest::newRow("png upper") << "IMG_0001.PNG" << false << "IMG_0001";
        QTest::newRow("dots") << "my.notes.v2.txt" << false << "my.notes.v2";
        QTest::newRow("tar.gz") << "backup.tar.gz" << false << "backup";
        QTest::newRow("unknown") << "data.qwzx" << false << "data.qwzx";
        QTest::newRow("no dot") << "Makefile" << false << "Makefile";
        QTest::newRow("dot file") << ".bashrc" << false << ".bashrc";
        QTest::newRow("only ext") << ".zip" << false << ".zip";
        QTest::newRow("trailing dot") << "odd." << false << "odd.";
        QTest::newRow("folder") << "Photos.2024.jpg" << true << "Photos.2024.jpg";
        QTest::newRow("exe") << "setup.exe" << false << "setup.exe";
        QTest::newRow("EXE upper") << "SETUP.EXE" << false << "SETUP.EXE";
        QTest::newRow("double exe") << "photo.jpg.exe" << false << "photo.jpg.exe";
        QTest::newRow("msi") << "tool.msi" << false << "tool.msi";
        QTest::newRow("bat") << "run.bat" << false << "run.bat";
        QTest::newRow("cmd") << "run.cmd" << false << "run.cmd";
        QTest::newRow("sh") << "install.sh" << false << "install.sh";
        QTest::newRow("desktop") << "game.desktop" << false << "game.desktop";
        QTest::newRow("appimage") << "App-x86_64.AppImage" << false << "App-x86_64.AppImage";
        QTest::newRow("lnk") << "Word.lnk" << false << "Word.lnk";
        QTest::newRow("scr") << "screensaver.scr" << false << "screensaver.scr";
    }

    void display()
    {
        QFETCH(QString, name);
        QFETCH(bool, isDir);
        QFETCH(QString, hidden);
        QCOMPARE(displayName(name, isDir, false), hidden);
        // Shown: always the whole name.
        QCOMPARE(displayName(name, isDir, true), name);
    }

    // A file the system would run keeps its extension whatever it is called.
    void executableBit()
    {
        QCOMPARE(displayName(QStringLiteral("notes.txt"), false, false, true), QStringLiteral("notes.txt"));
    }

    void alwaysShown()
    {
        QVERIFY(alwaysShowsExtension(QStringLiteral("a.exe")));
        QVERIFY(alwaysShowsExtension(QStringLiteral("a.Exe")));
        QVERIFY(!alwaysShowsExtension(QStringLiteral("a.pdf")));
        QVERIFY(!alwaysShowsExtension(QStringLiteral("exe")));
        QVERIFY(!alwaysShowsExtension(QStringLiteral(".exe")));
    }

    void renameSelection_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("isDir");
        QTest::addColumn<int>("length");
        QTest::newRow("pdf") << "report.pdf" << false << 6;
        QTest::newRow("tar.gz") << "backup.tar.gz" << false << 6;
        QTest::newRow("unknown") << "data.qwzx" << false << 4;
        QTest::newRow("no dot") << "Makefile" << false << 8;
        QTest::newRow("dot file") << ".bashrc" << false << 7;
        QTest::newRow("trailing dot") << "odd." << false << 4;
        QTest::newRow("folder") << "my.folder" << true << 9;
        QTest::newRow("exe") << "setup.exe" << false << 5;
        QTest::newRow("unicode") << "été.txt" << false << 3;
    }

    void renameSelection()
    {
        QFETCH(QString, name);
        QFETCH(bool, isDir);
        QFETCH(int, length);
        QCOMPARE(baseNameLength(name, isDir), length);
        // What rename selects is exactly what the view shows when hidden.
        if (!isDir && !alwaysShowsExtension(name) && !knownExtension(name).isEmpty()) {
            QCOMPARE(name.left(length), displayName(name, false, false));
        }
    }

    // The spec's budget: listing 10 000 files may cost at most 10 % more
    // than stock. What Raye adds per item is the shown name and the type
    // name; both are timed against the time the model takes to list.
    //
    // raye_test_extensions was flagged as FLAKY -- pass / fail / pass on
    // the same binary under `ctest -j6`, 6/6 on its own -- and the assertion
    // below is the only thing in this file that can fail without the code
    // changing, so it is the obvious suspect. The measurement, 2026-11-07,
    // on a 28-core host with the machine loaded by a parallel build:
    //
    //     5 runs of the whole suite, ctest -L raye -j6   34/34 green, 5 times
    //     6 copies of this binary at once, shared $HOME  36/36 green, 6 times
    //     5 runs of performance() alone                  green, 5 times
    //
    //     listing 10000 files: 256 / 334 / 282 / 294 / 265 ms
    //     Raye names:            6 /   4 /   4 /   5 /   4 ms
    //
    // Eleven attempts, nothing red: the flake did NOT reproduce, and the
    // earlier guess that a shared $HOME / sycoca / MIME cache was to blame
    // is withdrawn -- it was never measured. The margin here is ~40x, so
    // this assertion is not what was seen.
    //
    // It remains the wrong SHAPE of measurement, and that is worth
    // knowing before the next report. rayeMs times a tight CPU loop, listMs
    // times an ASYNCHRONOUS directory listing waited on by a signal. The two
    // do not inflate together under load, and the budget divides by the
    // second: a listing that came back fast -- a warm page cache, a smaller
    // tree, /tmp on tmpfs -- shrinks the denominator while the numerator
    // stays, and `qMax(listMs, 1) + 10` then allows rayeMs only 1.5 ms where
    // 4 to 6 was measured. The numbers are left alone on purpose: changing a
    // passing performance guard on a failure nobody has reproduced trades a
    // real check for a feeling.
    void performance()
    {
        QTemporaryDir dir;
        const char *suffixes[] = {".txt", ".pdf", ".png", ".tar.gz", ".exe", ".docx", "", ".mp3", ".zip", ".qwzx"};
        QStringList names;
        for (int i = 0; i < 10000; ++i) {
            const QString name = QStringLiteral("file %1%2").arg(i).arg(QLatin1String(suffixes[i % 10]));
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            names.append(name);
        }

        KFileItemModel model;
        QSignalSpy done(&model, &KFileItemModel::directoryLoadingCompleted);
        QElapsedTimer listing;
        listing.start();
        model.loadDirectory(QUrl::fromLocalFile(dir.path()));
        QVERIFY(done.wait(60000));
        const qint64 listMs = listing.elapsed();
        QCOMPARE(model.count(), 10000);

        QMimeDatabase db;
        QElapsedTimer raye;
        raye.start();
        int hidden = 0;
        for (const QString &name : std::as_const(names)) {
            hidden += displayName(name, false, false).size() != name.size();
            // The type name is looked up once the type is known; the lookup
            // is what Raye adds on top of the database's answer.
            Raye::TypeNames::name(QStringLiteral("text/plain"), QStringLiteral("x"));
        }
        const qint64 rayeMs = raye.elapsed();
        qInfo() << "listing 10000 files:" << listMs << "ms, Raye names:" << rayeMs << "ms," << hidden << "extensions hidden";
        QVERIFY(hidden > 5000);
        QVERIFY2(rayeMs * 10 <= qMax<qint64>(listMs, 1) + 10, qPrintable(QStringLiteral("%1 ms against %2 ms").arg(rayeMs).arg(listMs)));
    }

    // Windows' Size column: whole kilobytes, rounded up.
    void sizeColumn()
    {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        QCOMPARE(sizeColumnText(0), QStringLiteral("0 KB"));
        QCOMPARE(sizeColumnText(1), QStringLiteral("1 KB"));
        QCOMPARE(sizeColumnText(1024), QStringLiteral("1 KB"));
        QCOMPARE(sizeColumnText(1025), QStringLiteral("2 KB"));
        QCOMPARE(sizeColumnText(3493888), QStringLiteral("3,412 KB"));
        QCOMPARE(sizeColumnText(-5), QStringLiteral("0 KB"));
        QLocale::setDefault(QLocale::c());
    }
};

QTEST_MAIN(ExtensionsTest)

#include "test_extensions.moc"
