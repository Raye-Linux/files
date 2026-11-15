/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The render harness (tests/raye/harness). Run with arguments it shows or
// shoots a widget or a page (see harness/harness.h); run with none, as
// ctest does, it checks itself: every showcase renders in Night and Day,
// the composite carries the open flyout and dialog with their shadows,
// the QML controls load, and the kit speaks French.

#include "harness/harness.cpp"
#include "harness/kitdemos.cpp"

#include "ui/splitbutton.h"

#include <QTest>

using namespace RayeHarness;

class RayeUiHarnessTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void arguments_data();
    void arguments();
    void kitShowcasesAreListed();
    void everyShowcaseRenders_data();
    void everyShowcaseRenders();
    void compositeCarriesTheFlyout();
    void compositeCarriesTheDialog();
    void qmlControlsLoad_data();
    void qmlControlsLoad();
    void unknownNamesSayWhatExists();
    void kitSpeaksFrench();

private:
    QImage shoot(const Options &options, QString *error);
};

void RayeUiHarnessTest::initTestCase()
{
    // Before any lookup: a catalog is cached the first time it is used.
    applyLanguage(QStringLiteral("en"));
    registerKitShowcases();
    Raye::Ui::Motion::setSpeedFactor(0);
    s_shooting = true;
}

void RayeUiHarnessTest::cleanup()
{
    // Popups and dialogs a showcase opened go with its window.
    for (QWidget *w : QApplication::topLevelWidgets()) {
        w->close();
    }
    applyLanguage(QStringLiteral("en"));
}

QImage RayeUiHarnessTest::shoot(const Options &options, QString *error)
{
    applyTheme(options.night);
    applyLanguage(options.lang);
    std::unique_ptr<QWidget> window(buildWindow(options, error));
    if (!window) {
        return {};
    }
    window->move(0, 0);
    window->show();
    if (!QTest::qWaitForWindowExposed(window.get())) {
        *error = QStringLiteral("window never shown");
        return {};
    }
    settle(400);
    QImage image = composite(window.get());
    window.reset();
    return image;
}

void RayeUiHarnessTest::arguments_data()
{
    QTest::addColumn<QStringList>("args");
    QTest::addColumn<bool>("ok");
    QTest::addColumn<QString>("widget");
    QTest::addColumn<bool>("night");
    QTest::addColumn<QSize>("size");

    QTest::newRow("defaults") << QStringList{QStringLiteral("--widget"), QStringLiteral("Kit")} << true << QStringLiteral("Kit") << true << QSize(1280, 800);
    QTest::newRow("day, French, big") << QStringList{QStringLiteral("--widget"), QStringLiteral("Kit"), QStringLiteral("--theme"), QStringLiteral("day"),
                                                     QStringLiteral("--lang"), QStringLiteral("fr"), QStringLiteral("--size"), QStringLiteral("1920x1080"),
                                                     QStringLiteral("--shot"), QStringLiteral("out.png")}
                                      << true << QStringLiteral("Kit") << false << QSize(1920, 1080);
    QTest::newRow("list") << QStringList{QStringLiteral("--list")} << true << QString() << true << QSize(1280, 800);
    QTest::newRow("nothing") << QStringList{} << false << QString() << true << QSize();
    QTest::newRow("bad theme") << QStringList{QStringLiteral("--widget"), QStringLiteral("Kit"), QStringLiteral("--theme"), QStringLiteral("dusk")} << false << QString() << true << QSize();
    QTest::newRow("bad size") << QStringList{QStringLiteral("--widget"), QStringLiteral("Kit"), QStringLiteral("--size"), QStringLiteral("12x9")} << false << QString() << true << QSize();
    QTest::newRow("bad lang") << QStringList{QStringLiteral("--widget"), QStringLiteral("Kit"), QStringLiteral("--lang"), QStringLiteral("../x")} << false << QString() << true << QSize();
    QTest::newRow("two things") << QStringList{QStringLiteral("--widget"), QStringLiteral("Kit"), QStringLiteral("--page"), QStringLiteral("UsageBar")} << false << QString() << true << QSize();
    QTest::newRow("missing value") << QStringList{QStringLiteral("--widget"), QStringLiteral("--theme"), QStringLiteral("day")} << false << QString() << true << QSize();
    QTest::newRow("unknown") << QStringList{QStringLiteral("--colour"), QStringLiteral("red")} << false << QString() << true << QSize();
}

void RayeUiHarnessTest::arguments()
{
    QFETCH(QStringList, args);
    QFETCH(bool, ok);
    args.prepend(QStringLiteral("raye_test_ui_harness"));
    Options options;
    QString error;
    QCOMPARE(parseArguments(args, &options, &error), ok);
    if (!ok) {
        QVERIFY(!error.isEmpty());
        return;
    }
    QFETCH(QString, widget);
    QFETCH(bool, night);
    QFETCH(QSize, size);
    QCOMPARE(options.widget, widget);
    QCOMPARE(options.night, night);
    QCOMPARE(options.size, size);
}

void RayeUiHarnessTest::kitShowcasesAreListed()
{
    for (const char *name : {"Kit", "CommandBar", "Flyout", "CardDialog", "KitQml", "KitQmlDialog"}) {
        QVERIFY2(Raye::Ui::Showcase::find(QString::fromLatin1(name)), name);
    }
    QVERIFY(qmlTypes().contains(QStringLiteral("CommandButton")));
    QVERIFY(qmlTypes().contains(QStringLiteral("CardDialog")));
}

void RayeUiHarnessTest::everyShowcaseRenders_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("night");
    for (const auto &entry : Raye::Ui::Showcase::entries()) {
        if (entry.name.startsWith(QLatin1String("KitQml"))) {
            continue; // needs the QML modules: qmlControlsLoad
        }
        for (bool night : {true, false}) {
            QTest::newRow(qPrintable(QStringLiteral("%1 %2").arg(entry.name, night ? QStringLiteral("night") : QStringLiteral("day")))) << entry.name << night;
        }
    }
}

void RayeUiHarnessTest::everyShowcaseRenders()
{
    QFETCH(QString, name);
    QFETCH(bool, night);
    Options options;
    options.widget = name;
    options.night = night;
    options.size = QSize(1280, 800);
    QString error;
    const QImage image = shoot(options, &error);
    QVERIFY2(!image.isNull(), qPrintable(error));
    QCOMPARE(image.size(), QSize(1280, 800));
    // The window is the base colour of the mode...
    const QColor base = Raye::Ui::TokenSet::table(night).color(Raye::Ui::TokenSet::bgBase);
    QCOMPARE(QColor(image.pixel(2, image.height() - 3)).rgb(), base.rgb());
    // ...and something is drawn on it.
    QSet<QRgb> colours;
    for (int y = 0; y < image.height(); y += 4) {
        for (int x = 0; x < image.width(); x += 4) {
            colours.insert(image.pixel(x, y));
        }
    }
    QVERIFY2(colours.size() > 20, qPrintable(QString::number(colours.size())));
}

void RayeUiHarnessTest::compositeCarriesTheFlyout()
{
    Options options;
    options.widget = QStringLiteral("Flyout");
    QString error;
    applyTheme(true);
    std::unique_ptr<QWidget> window(buildWindow(options, &error));
    QVERIFY2(window, qPrintable(error));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
    settle(300);
    Raye::Ui::Flyout *flyout = window->findChild<Raye::Ui::Flyout *>();
    QVERIFY(flyout);
    QVERIFY(flyout->isVisible());
    QVERIFY(flyout->translucent());
    const QImage plain = window->grab().toImage();
    const QImage shot = composite(window.get());
    const QRect g(flyout->mapToGlobal(QPoint(0, 0)) - window->mapToGlobal(QPoint(0, 0)), flyout->size());
    QVERIFY(QRect(QPoint(), shot.size()).contains(g.center()));
    QVERIFY(shot.pixel(g.center()) != plain.pixel(g.center()));
    // The shadow darkens what lies under the flyout's bottom edge.
    const QPoint below(g.center().x(), g.bottom() + 8);
    QVERIFY(qGray(shot.pixel(below)) <= qGray(plain.pixel(below)));
}

void RayeUiHarnessTest::compositeCarriesTheDialog()
{
    Options options;
    options.widget = QStringLiteral("CardDialog");
    QString error;
    applyTheme(false);
    std::unique_ptr<QWidget> window(buildWindow(options, &error));
    QVERIFY2(window, qPrintable(error));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
    settle(300);
    auto *dialog = window->findChild<Raye::Ui::CardDialog *>();
    QVERIFY(dialog);
    QVERIFY(dialog->isVisible());
    const QImage shot = composite(window.get());
    const QRect g(dialog->mapToGlobal(QPoint(0, 0)) - window->mapToGlobal(QPoint(0, 0)), dialog->size());
    // The card body and its button row are drawn where the dialog is.
    const Raye::Ui::TokenSet day = Raye::Ui::TokenSet::table(false);
    QCOMPARE(QColor(shot.pixel(g.left() + 12, g.top() + 12)).rgb(), day.color(Raye::Ui::TokenSet::bgCardSolid).rgb());
    QCOMPARE(QColor(shot.pixel(g.left() + 12, g.bottom() - 12)).rgb(), day.color(Raye::Ui::TokenSet::bgBase).rgb());
}

void RayeUiHarnessTest::qmlControlsLoad_data()
{
    QTest::addColumn<QString>("showcase");
    QTest::newRow("controls") << QStringLiteral("KitQml");
    QTest::newRow("dialog") << QStringLiteral("KitQmlDialog");
}

void RayeUiHarnessTest::qmlControlsLoad()
{
    QFETCH(QString, showcase);
    applyTheme(true);
    const Raye::Ui::Showcase::Entry *entry = Raye::Ui::Showcase::find(showcase);
    QVERIFY(entry);
    QWidget window;
    window.resize(1280, 800);
    auto *view = qobject_cast<QQuickWidget *>(entry->build(&window));
    QVERIFY(view);
    QString error;
    if (viewFailed(view, &error)) {
        // The package build may run where the QML runtime modules are not
        // installed; the controls themselves are then checked in the image.
        if (error.contains(QLatin1String("is not installed"))) {
            QSKIP(qPrintable(QStringLiteral("QML modules missing here: ") + error));
        }
        QFAIL(qPrintable(error));
    }
    QCOMPARE(view->status(), QQuickWidget::Ready);
    QVERIFY(view->rootObject());
}

void RayeUiHarnessTest::unknownNamesSayWhatExists()
{
    Options options;
    options.widget = QStringLiteral("NoSuchWidget");
    QString error;
    QVERIFY(!buildWindow(options, &error));
    QVERIFY(error.contains(QLatin1String("Kit")));
    options = Options();
    options.page = QStringLiteral("NoSuchPage");
    QVERIFY(!buildWindow(options, &error));
    QVERIFY(error.contains(QLatin1String("CommandButton")));
}

void RayeUiHarnessTest::kitSpeaksFrench()
{
    if (!applyLanguage(QStringLiteral("fr"))) {
        if (qEnvironmentVariableIsSet("RAYE_TESTS_STRICT")) {
            QFAIL("fr_FR.UTF-8 is not generated on this machine");
        }
        QSKIP("fr_FR.UTF-8 is not generated on this machine");
    }
    QCOMPARE(text("Copy", "Copier"), QStringLiteral("Copier"));
    // Strings of the kit itself come from the raye-files catalog.
    Raye::Ui::SplitButton split;
    QCOMPARE(split.menuButton()->accessibleName(), QStringLiteral("Plus d'options"));
    QCOMPARE(Raye::Ui::CommandButton::toolTipFor(QStringLiteral("Copier"), QKeySequence(QStringLiteral("Ctrl+C"))), QStringLiteral("Copier (Ctrl+C)"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Harness options start with "--", QtTest's with one "-".
    if (argc > 1 && QByteArray(argv[1]).startsWith("--")) {
        registerKitShowcases();
        Options options;
        QString error;
        if (!parseArguments(app.arguments(), &options, &error)) {
            std::fputs(qPrintable(error + QStringLiteral("\n") + usage()), stderr);
            return 2;
        }
        return run(options);
    }
    RayeUiHarnessTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_ui_harness.moc"
