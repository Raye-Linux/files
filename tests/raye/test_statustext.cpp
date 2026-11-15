/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The status bar text: item count, then the selection and its size.

#include "window/statustext.h"

#include <KLocalizedString>

#include <QTest>

#include <clocale>

using Raye::Window::StatusCounts;

class StatusTextTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setLanguages({QStringLiteral("en")});
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    void counts()
    {
        QCOMPARE(Raye::Window::itemsText(0), QStringLiteral("0 items"));
        QCOMPARE(Raye::Window::itemsText(1), QStringLiteral("1 item"));
        QCOMPARE(Raye::Window::itemsText(12), QStringLiteral("12 items"));
    }

    void nothingSelected()
    {
        StatusCounts c;
        c.items = 12;
        QCOMPARE(Raye::Window::selectionText(c), QString());
        QCOMPARE(Raye::Window::statusParts(c), QStringList{QStringLiteral("12 items")});
    }

    void foldersOnlyHaveNoSize()
    {
        StatusCounts c;
        c.items = 5;
        c.selected = 2;
        QCOMPARE(Raye::Window::selectionText(c), QStringLiteral("2 items selected"));
    }

    void filesShowTheirSize()
    {
        StatusCounts c;
        c.items = 12;
        c.selected = 1;
        c.selectedFiles = 1;
        c.selectedBytes = qint64(2.4 * 1024 * 1024);
        QCOMPARE(Raye::Window::statusParts(c), (QStringList{QStringLiteral("12 items"), QStringLiteral("1 item selected  2.4 MB")}));
    }

    void emptyFilesSayBytes()
    {
        StatusCounts c;
        c.items = 1;
        c.selected = 1;
        c.selectedFiles = 1;
        QCOMPARE(Raye::Window::selectionText(c), QStringLiteral("1 item selected  0 bytes"));
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
        StatusCounts c;
        c.items = 12;
        c.selected = 3;
        QCOMPARE(Raye::Window::itemsText(12), QStringLiteral("12 éléments"));
        QCOMPARE(Raye::Window::selectionText(c), QStringLiteral("3 éléments sélectionnés"));
        KLocalizedString::setLanguages({QStringLiteral("en")});
        std::setlocale(LC_MESSAGES, "C.UTF-8");
    }
};

QTEST_GUILESS_MAIN(StatusTextTest)

#include "test_statustext.moc"
