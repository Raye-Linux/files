/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The "Merge folders" and "Replace or skip files" dialogs fit what they
// show: the last line of the page (the "Do this for all current items"
// box) stays above the row of buttons, in English and French, before and
// after the compare view opens.

#include "jobs/dialogs.h"
#include "ui/carddialog.h"
#include "ui/commandbutton.h"

#include <KLocalizedString>

#include <QApplication>
#include <QQuickItem>
#include <QQuickWidget>
#include <QStandardPaths>
#include <QTest>
#include <QtMath>

#include <clocale>

using namespace Raye::Jobs;

namespace
{
QQuickItem *itemWithText(QQuickItem *root, const QString &text)
{
    const auto items = root->findChildren<QQuickItem *>();
    for (QQuickItem *item : items) {
        if (item->property("text").toString() == text) {
            return item;
        }
    }
    return nullptr;
}

ConflictInfo info(bool folders)
{
    ConflictInfo i;
    i.source = QUrl::fromLocalFile(QStringLiteral("/home/ana/Pictures/Holidays in the mountains, summer"));
    i.destination = QUrl::fromLocalFile(QStringLiteral("/media/ana/USB DRIVE/Backups/Holidays in the mountains, summer"));
    i.sourceModified = QDateTime(QDate(2026, 7, 3), QTime(10, 0));
    i.destinationModified = QDateTime(QDate(2026, 6, 1), QTime(9, 0));
    i.sourceSize = 2048;
    i.destinationSize = 1024;
    i.jobTitle = QStringLiteral("Copying 14 items from Pictures to Backups");
    i.options = KIO::RenameDialog_MultipleItems | KIO::RenameDialog_Overwrite | KIO::RenameDialog_Skip;
    if (folders) {
        i.options |= KIO::RenameDialog_SourceIsDirectory | KIO::RenameDialog_DestIsDirectory;
    }
    return i;
}
}

class ConflictDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        const QByteArray imports = qEnvironmentVariable("RAYE_FILES_BINARY_DIR", QStringLiteral(RAYE_FILES_BINARY_DIR)).toUtf8() + QByteArray("/bin");
        qputenv("QML_IMPORT_PATH", imports);
        KLocalizedString::addDomainLocaleDir("raye-files", QStringLiteral(RAYE_FILES_BINARY_DIR "/locale"));
    }

    void cleanup()
    {
        KLocalizedString::setLanguages({QStringLiteral("en")});
        std::setlocale(LC_MESSAGES, "C.UTF-8");
    }

    void fits_data()
    {
        QTest::addColumn<bool>("folders");
        QTest::addColumn<QString>("language");
        QTest::addColumn<bool>("compare");
        QTest::addColumn<bool>("growsLate");
        QTest::addColumn<bool>("longLabel");
        QTest::newRow("merge, en") << true << QStringLiteral("en") << false << false << false;
        QTest::newRow("merge, fr") << true << QStringLiteral("fr") << false << false << false;
        QTest::newRow("files, en") << false << QStringLiteral("en") << false << false << false;
        QTest::newRow("files, fr") << false << QStringLiteral("fr") << false << false << false;
        QTest::newRow("compare, en") << false << QStringLiteral("en") << true << false << false;
        QTest::newRow("compare, fr") << false << QStringLiteral("fr") << true << false << false;
        // the page gets taller once the window is up (text laid out again
        // at the real scale, a line that wraps): the window follows
        QTest::newRow("merge grows, en") << true << QStringLiteral("en") << false << true << false;
        QTest::newRow("merge grows, fr") << true << QStringLiteral("fr") << false << true << false;
        // a label that wraps (large text, a long translation): the box is
        // as tall as its lines, and they stay above the buttons
        QTest::newRow("long label, en") << true << QStringLiteral("en") << false << false << true;
        QTest::newRow("long label, fr") << true << QStringLiteral("fr") << false << false << true;
    }

    void fits()
    {
        QFETCH(bool, folders);
        QFETCH(QString, language);
        QFETCH(bool, compare);
        QFETCH(bool, growsLate);
        QFETCH(bool, longLabel);
        if (language == QLatin1String("fr")) {
            if (!std::setlocale(LC_MESSAGES, "fr_FR.UTF-8") && !std::setlocale(LC_MESSAGES, "C.UTF-8")) {
                QSKIP("no UTF-8 locale here");
            }
            KLocalizedString::setLanguages({QStringLiteral("fr")});
        }

        Raye::Ui::CardDialog *dialog = conflictDialog(nullptr, info(folders), [](const ConflictAnswer &) {});
        QVERIFY(dialog);
        auto *view = dialog->findChild<QQuickWidget *>();
        QVERIFY(view);
        QQuickItem *root = view->rootObject();
        if (!root) {
            delete dialog;
            if (!qEnvironmentVariableIsSet("RAYE_TESTS_STRICT")) {
                QSKIP("the org.raye.files module is not built here");
            }
            QFAIL("the conflict page did not load");
        }
        if (growsLate) {
            root->setProperty("multiple", false);
        }
        dialog->show();
        QVERIFY(QTest::qWaitForWindowExposed(dialog));
        if (growsLate) {
            QTest::qWait(100);
            root->setProperty("multiple", true);
            root->setProperty("headline", QStringLiteral("Copying 14 items from Pictures to Backups, a long line that has to wrap onto a second and even a third line in this card"));
        }
        if (compare) {
            root->setProperty("comparing", true);
        }
        QTest::qWait(300);

        const QString forAllText = i18nc("@option:check", "Do this for all current items");
        if (language == QLatin1String("fr")) {
            QVERIFY2(forAllText != QLatin1String("Do this for all current items"), "no French catalog");
        }
        QQuickItem *forAll = itemWithText(root, forAllText);
        QVERIFY(forAll);
        QVERIFY(forAll->isVisible());
        if (longLabel) {
            forAll->setProperty("text", QString(forAllText + QLatin1Char(' ') + forAllText + QLatin1Char(' ') + forAllText));
            QTest::qWait(300);
        }
        // the label's lines are inside the box
        QQuickItem *label = forAll->property("contentItem").value<QQuickItem *>();
        QVERIFY(label);
        QVERIFY2(label->mapToItem(forAll, QPointF(0, label->implicitHeight())).y() <= forAll->height() + 1,
                 qPrintable(QStringLiteral("label %1 high, box %2").arg(label->implicitHeight()).arg(forAll->height())));

        // the whole page is in the view...
        QVERIFY2(view->height() + 1 >= qCeil(root->implicitHeight()),
                 qPrintable(QStringLiteral("view %1, page %2").arg(view->height()).arg(root->implicitHeight())));
        const qreal boxBottom = forAll->mapToScene(QPointF(0, forAll->height())).y();
        QVERIFY2(boxBottom <= view->height() + 1, qPrintable(QStringLiteral("box ends at %1, view %2").arg(boxBottom).arg(view->height())));

        // ...and the view is above the buttons, inside the dialog
        Raye::Ui::CommandButton *cancel = dialog->button(Raye::Ui::CardDialog::Close);
        QVERIFY(cancel);
        QVERIFY(cancel->isVisible());
        const int viewBottom = view->mapTo(dialog, QPoint(0, view->height())).y();
        const int cancelTop = cancel->mapTo(dialog, QPoint(0, 0)).y();
        QVERIFY2(viewBottom <= cancelTop, qPrintable(QStringLiteral("page ends at %1, Cancel starts at %2").arg(viewBottom).arg(cancelTop)));
        QVERIFY(cancel->mapTo(dialog, QPoint(0, cancel->height())).y() <= dialog->height());
        delete dialog;
    }
};

QTEST_MAIN(ConflictDialogTest)
#include "test_conflictdialog.moc"
