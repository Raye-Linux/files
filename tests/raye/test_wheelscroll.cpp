/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The mouse wheel, as on Windows: one notch moves a view by the "lines per
// notch" setting, about 100 px at 3 lines, everywhere in Files: the file
// views, the navigation pane, the Details pane and the QML pages.

#include "kitemviews/kfileitemlistview.h"
#include "kitemviews/kfileitemmodel.h"
#include "kitemviews/kitemlistcontainer.h"
#include "kitemviews/kitemlistcontroller.h"
#include "navpane/navigationpane.h"
#include "preview/detailspane.h"
#include "ui/tokens.h"
#include "viewdefaults/viewdefaults.h"

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QWheelEvent>

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
#include <KLocalizedContext>
QT_WARNING_POP

using Raye::ViewDefaults::Mode;

namespace
{
int notchPixels()
{
    return QApplication::wheelScrollLines() * Raye::Ui::Metrics::wheelLine;
}

int count(const QRegularExpression &re, const QString &text)
{
    int n = 0;
    for (auto m = re.globalMatch(text); m.hasNext(); m.next()) {
        ++n;
    }
    return n;
}

void notch(QWidget *target, int angle)
{
    const QPointF at(target->width() / 2.0, target->height() / 2.0);
    QWheelEvent ev(at, target->mapToGlobal(at), QPoint(), QPoint(0, angle), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(target, &ev);
}
}

class WheelScrollTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_temp.isValid());
        for (int i = 0; i < 600; ++i) {
            QFile f(m_temp.filePath(QStringLiteral("file %1.txt").arg(i, 3, 10, QLatin1Char('0'))));
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
    }

    void notchIsAboutAHundredPixels()
    {
        QCOMPARE(QApplication::wheelScrollLines(), 3);
        QVERIFY(qAbs(notchPixels() - 100) <= 5);
    }

    void fileView_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("details") << int(Mode::Details);
        QTest::newRow("medium icons") << int(Mode::MediumIcons);
        QTest::newRow("large icons") << int(Mode::LargeIcons);
        QTest::newRow("content") << int(Mode::Content);
    }

    void fileView()
    {
        QFETCH(int, mode);
        const Raye::ViewDefaults::ModeSpec spec = Raye::ViewDefaults::spec(Mode(mode));

        auto *model = new KFileItemModel(this);
        auto *view = new KFileItemListView;
        auto *controller = new KItemListController(model, view, this);
        KItemListContainer container(controller);
#ifndef QT_NO_ACCESSIBILITY
        view->setAccessibleParentsObject(&container);
#endif
        using L = Raye::ViewDefaults::Layout;
        view->setItemLayout(spec.layout == L::Details ? KStandardItemListView::DetailsLayout
                                : spec.layout == L::Compact ? KStandardItemListView::CompactLayout
                                                            : KStandardItemListView::IconsLayout);
        view->setRayeMetrics(spec.rowHeight, spec.cellWidth, Raye::ViewDefaults::flowsInRows(Mode(mode)));
        model->loadDirectory(QUrl::fromLocalFile(m_temp.path()));
        QSignalSpy loaded(model, &KFileItemModel::directoryLoadingCompleted);
        QVERIFY(loaded.wait());
        container.resize(900, 500);
        container.show();
        QVERIFY(QTest::qWaitForWindowExposed(&container));
        QTRY_VERIFY(container.verticalScrollBar()->maximum() > 1000);
        QCOMPARE(view->scrollOffset(), 0.0);

        notch(container.viewport(), -120);
        QTest::qWait(600);
        const qreal moved = view->scrollOffset();
        if (spec.layout == Raye::ViewDefaults::Layout::Details) {
            // whole rows, as close to a notch as the rows allow
            const int rows = qMax(1, qRound(qreal(notchPixels()) / spec.rowHeight));
            QCOMPARE(moved, qreal(rows * spec.rowHeight));
        } else {
            QVERIFY2(qAbs(moved - notchPixels()) <= 2, qPrintable(QString::number(moved)));
        }

        // three quick notches go three times as far: none is lost while
        // the first one is still moving
        notch(container.viewport(), 120);
        QTRY_COMPARE(view->scrollOffset(), 0.0);
        notch(container.viewport(), -120);
        notch(container.viewport(), -120);
        notch(container.viewport(), -120);
        QTest::qWait(900);
        QVERIFY2(qAbs(view->scrollOffset() - 3 * moved) <= 3, qPrintable(QString::number(view->scrollOffset())));
    }

    void navigationPane()
    {
        Raye::NavPane::NavigationPane pane;
        pane.resize(260, 120);
        pane.show();
        QVERIFY(QTest::qWaitForWindowExposed(&pane));
        QTest::qWait(50);
        QCOMPARE(pane.verticalScrollBar()->singleStep() * QApplication::wheelScrollLines(), notchPixels());
        // and it stays after the rows change
        pane.resize(260, 300);
        QTest::qWait(50);
        QCOMPARE(pane.verticalScrollBar()->singleStep() * QApplication::wheelScrollLines(), notchPixels());
    }

    void detailsPane()
    {
        Raye::Preview::DetailsPane pane;
        pane.resize(300, 200);
        pane.show();
        QVERIFY(QTest::qWaitForWindowExposed(&pane));
        auto *scroll = pane.findChild<QScrollArea *>();
        QVERIFY(scroll);
        QCOMPARE(scroll->verticalScrollBar()->singleStep() * QApplication::wheelScrollLines(), notchPixels());
    }

    void qmlFlickable()
    {
        QQmlEngine engine;
        engine.addImportPath(qEnvironmentVariable("RAYE_FILES_BINARY_DIR", QStringLiteral(RAYE_FILES_BINARY_DIR)) + QStringLiteral("/bin"));
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
        QT_WARNING_POP
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import org.raye.files
            Window {
                width: 400; height: 300; visible: true
                Flickable {
                    id: flick
                    objectName: "flick"
                    anchors.fill: parent
                    contentHeight: 5000
                    WheelScroll { target: flick }
                }
            })",
                          QUrl());
        if (component.isError() && component.errorString().contains(QLatin1String("module \"org.raye.files\" is not installed"))
            && !qEnvironmentVariableIsSet("RAYE_TESTS_STRICT")) {
            QSKIP("the org.raye.files module is not built here");
        }
        std::unique_ptr<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.get());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *flick = window->findChild<QQuickItem *>(QStringLiteral("flick"));
        QVERIFY(flick);

        const QPointF at(200, 150);
        QWheelEvent down(at, window->mapToGlobal(at), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QGuiApplication::sendEvent(window, &down);
        QTRY_VERIFY(qAbs(flick->property("contentY").toReal() - notchPixels()) < 2);
        QTest::qWait(400);
        QVERIFY2(qAbs(flick->property("contentY").toReal() - notchPixels()) < 2, qPrintable(QString::number(flick->property("contentY").toReal())));
    }

    // Every scrolling list or page of the QML module has the Windows step.
    void everyQmlListHasIt()
    {
        const QString qmlDir = QStringLiteral(RAYE_FILES_SOURCE_DIR "/src/raye/qml");
        static const QRegularExpression scroller(QStringLiteral("\\b(Flickable|ListView|GridView)\\s*\\{"));
        static const QRegularExpression handler(QStringLiteral("\\bWheelScroll\\s*\\{"));
        QDirIterator it(qmlDir, {QStringLiteral("*.qml")}, QDir::Files, QDirIterator::Subdirectories);
        int checked = 0;
        while (it.hasNext()) {
            QFile f(it.next());
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QString text = QString::fromUtf8(f.readAll());
            const int lists = count(scroller, text);
            if (lists == 0) {
                continue;
            }
            const int handlers = count(handler, text);
            QVERIFY2(handlers >= lists, qPrintable(it.fileInfo().fileName()));
            ++checked;
        }
        QVERIFY(checked >= 6);
    }

private:
    QTemporaryDir m_temp;
};

QTEST_MAIN(WheelScrollTest)
#include "test_wheelscroll.moc"
