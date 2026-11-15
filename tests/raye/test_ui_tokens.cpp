/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The token table of the Files widget kit: Night and Day, the accent rule,
// the fallback to a foreign palette, and (when org.raye.ui is installed)
// that the table still says what the theme says.

#include "ui/tokens.h"

#include <QApplication>
#include <QDir>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTest>

using Raye::Ui::TokenSet;
using Raye::Ui::Tokens;

class RayeUiTokensTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tableHasTheThemeValues();
    void everyRoleHasAName();
    void rayePaletteGivesTheTable_data();
    void rayePaletteGivesTheTable();
    void lastReleaseSchemeIsStillRaye();
    void foreignPaletteKeepsItsOwnColours();
    void unthemedPaletteIsRayeNight();
    void deriveAccent_data();
    void deriveAccent();
    void defaultAccentKeepsTheTunedShades();
    void selectedFillFollowsTheAccent();
    void contrastOfTheTable();
    void applicationTokensFollowThePalette();
    void qmlSeesTheSameTable();
    void matchesTheTheme();
};

void RayeUiTokensTest::tableHasTheThemeValues()
{
    const TokenSet night = TokenSet::table(true);
    const TokenSet day = TokenSet::table(false);
    QVERIFY(night.night());
    QVERIFY(!day.night());

    QCOMPARE(night.color(TokenSet::bgBase), QColor(QStringLiteral("#141318")));
    QCOMPARE(day.color(TokenSet::bgBase), QColor(QStringLiteral("#f5f4f7")));
    QCOMPARE(night.color(TokenSet::textPrimary), QColor(QStringLiteral("#f1f0f4")));
    QCOMPARE(night.color(TokenSet::textSecondary), QColor(QStringLiteral("#b5b3bc")));
    QCOMPARE(night.color(TokenSet::accent), QColor(QStringLiteral("#7b3cff")));
    QCOMPARE(night.color(TokenSet::captionClose), QColor(QStringLiteral("#e5334f")));
    // Alpha colours keep their alpha: white at 6 % over the surface.
    const QColor hover = night.color(TokenSet::fillSubtleHover);
    QCOMPARE(hover.rgb(), QColor(Qt::white).rgb());
    QCOMPARE(hover.alpha(), 0x0f);
}

void RayeUiTokensTest::everyRoleHasAName()
{
    QStringList names = TokenSet::roleNames();
    QCOMPARE(names.size(), int(TokenSet::RoleCount));
    QCOMPARE(names.removeDuplicates(), 0);
    const TokenSet night = TokenSet::table(true);
    for (const QString &name : names) {
        QVERIFY2(night.color(name).isValid(), qPrintable(name));
    }
    QVERIFY(!night.color(QStringLiteral("noSuchColour")).isValid());
    QCOMPARE(night.color(QStringLiteral("bgLayer")), night.color(TokenSet::bgLayer));
}

void RayeUiTokensTest::rayePaletteGivesTheTable_data()
{
    QTest::addColumn<bool>("night");
    QTest::newRow("night") << true;
    QTest::newRow("day") << false;
}

void RayeUiTokensTest::lastReleaseSchemeIsStillRaye()
{
    // Between the Files and theme updates the old Raye Night is in use.
    QPalette palette = TokenSet::table(true).palette();
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#130e1f")));
    QVERIFY(TokenSet::isRayePalette(palette));
    const TokenSet read = TokenSet::fromPalette(palette);
    QVERIFY(read.raye());
    QVERIFY(read.night());
    QCOMPARE(read.color(TokenSet::bgBase), QColor(QStringLiteral("#141318")));
}

void RayeUiTokensTest::rayePaletteGivesTheTable()
{
    QFETCH(bool, night);
    const TokenSet table = TokenSet::table(night);
    const QPalette palette = table.palette();
    QVERIFY(TokenSet::isRayePalette(palette));

    const TokenSet read = TokenSet::fromPalette(palette);
    QVERIFY(read.raye());
    QCOMPARE(read.night(), night);
    for (int r = 0; r < TokenSet::RoleCount; ++r) {
        const auto role = TokenSet::Role(r);
        QVERIFY2(read.color(role) == table.color(role), qPrintable(TokenSet::roleName(role)));
    }
}

void RayeUiTokensTest::foreignPaletteKeepsItsOwnColours()
{
    // A dark scheme that is not Raye's: its window, view and text colours
    // stay, the Raye overlays come on top.
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#202326")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#fcfcfc")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#141618")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#fcfcfc")));
    palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#a1a9b1")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#292c30")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3daee9")));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(QStringLiteral("#6e7173")));

    QVERIFY(!TokenSet::isRayePalette(palette));
    const TokenSet set = TokenSet::fromPalette(palette);
    QVERIFY(!set.raye());
    QVERIFY(set.night());
    QCOMPARE(set.color(TokenSet::bgBase), QColor(QStringLiteral("#202326")));
    QCOMPARE(set.color(TokenSet::bgLayer), QColor(QStringLiteral("#141618")));
    QCOMPARE(set.color(TokenSet::bgLayerSolid), QColor(QStringLiteral("#141618")));
    QCOMPARE(set.color(TokenSet::textPrimary), QColor(QStringLiteral("#fcfcfc")));
    QCOMPARE(set.color(TokenSet::textSecondary), QColor(QStringLiteral("#a1a9b1")));
    QCOMPARE(set.color(TokenSet::textDisabled), QColor(QStringLiteral("#6e7173")));
    QCOMPARE(set.color(TokenSet::bgTooltip), QColor(QStringLiteral("#292c30")));
    QCOMPARE(set.color(TokenSet::accent), QColor(QStringLiteral("#3daee9")));
    // Overlays are the Night ones: they work over any dark surface.
    QCOMPARE(set.color(TokenSet::fillSubtleHover), TokenSet::table(true).color(TokenSet::fillSubtleHover));

    // A light one is Day.
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#eff0f1")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#232629")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#e5334f")));
    QVERIFY(!TokenSet::fromPalette(palette).night());
}

void RayeUiTokensTest::unthemedPaletteIsRayeNight()
{
    // Qt's own palette: no scheme behind it, so Raye Night as org.raye.ui does.
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#efefef")));
    palette.setColor(QPalette::WindowText, Qt::black);
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#308cc6")));
    QVERIFY(TokenSet::isUnthemedPalette(palette));
    const TokenSet set = TokenSet::fromPalette(palette);
    QVERIFY(set.night());
    QVERIFY(set.raye());
    QCOMPARE(set.color(TokenSet::bgBase), TokenSet::table(true).color(TokenSet::bgBase));
    QCOMPARE(set.color(TokenSet::accent), QColor(QStringLiteral("#7b3cff")));

    // Text that cannot be read on its background is not a scheme either
    // (what the platform hands out before it has loaded one).
    QPalette black;
    black.setColor(QPalette::Window, Qt::black);
    black.setColor(QPalette::WindowText, Qt::black);
    QVERIFY(TokenSet::fromPalette(black).raye());
}

void RayeUiTokensTest::deriveAccent_data()
{
    // Expected values from org.raye.ui Tokens.deriveAccent().
    QTest::addColumn<QString>("accent");
    QTest::addColumn<bool>("night");
    QTest::addColumn<QString>("hover");
    QTest::addColumn<QString>("pressed");
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("disabled");
    QTest::addColumn<QString>("onAccent");

    QTest::newRow("blue night") << "#3daee9" << true << "#54b8ec" << "#3699cd" << "#3daee9" << "#2b4657" << "#17151c";
    QTest::newRow("blue day") << "#3daee9" << false << "#369acf" << "#2f87b5" << "#226182" << "#bed9e9" << "#17151c";
    QTest::newRow("red night") << "#e5334f" << true << "#d7304a" << "#ca2d46" << "#ef8192" << "#50242d" << "#ffffff";
    QTest::newRow("red day") << "#e5334f" << false << "#ca2d46" << "#ae273c" << "#ae273c" << "#e2b6be" << "#ffffff";
    QTest::newRow("gold night") << "#ffd700" << true << "#ffdc1f" << "#e0bd00" << "#ffd700" << "#615724" << "#17151c";
    QTest::newRow("gold day") << "#ffd700" << false << "#e0bd00" << "#c2a300" << "#6b5a00" << "#f3e9b6" << "#17151c";
    QTest::newRow("violet night") << "#7b3cff" << true << "#8950ff" << "#6c35e0" << "#b28eff" << "#362658" << "#ffffff";
    QTest::newRow("violet day") << "#7b3cff" << false << "#6c35e0" << "#5d2ec2" << "#6a34db" << "#c8b9ea" << "#ffffff";
    QTest::newRow("grey night") << "#202020" << true << "#3b3b3b" << "#1c1c1c" << "#a1a1a1" << "#18181a" << "#ffffff";
    QTest::newRow("grey day") << "#202020" << false << "#1c1c1c" << "#181818" << "#202020" << "#aaaaab" << "#ffffff";
}

void RayeUiTokensTest::deriveAccent()
{
    QFETCH(QString, accent);
    QFETCH(bool, night);
    QFETCH(QString, hover);
    QFETCH(QString, pressed);
    QFETCH(QString, text);
    QFETCH(QString, disabled);
    QFETCH(QString, onAccent);

    const auto shades = Raye::Ui::deriveAccent(QColor(accent), night);
    QCOMPARE(shades.accent.name(), accent);
    QCOMPARE(shades.hover.name(), hover);
    QCOMPARE(shades.pressed.name(), pressed);
    QCOMPARE(shades.text.name(), text);
    QCOMPARE(shades.disabled.name(), disabled);
    QCOMPARE(shades.onAccent.name(), onAccent);
}

void RayeUiTokensTest::defaultAccentKeepsTheTunedShades()
{
    // The table's hand-tuned shades win over the rule for the Raye violet.
    const TokenSet night = TokenSet::table(true);
    QCOMPARE(night.color(TokenSet::accentHover).name(), QStringLiteral("#8950ff"));
    QCOMPARE(night.color(TokenSet::accentPressed).name(), QStringLiteral("#6a2fe6"));
    QCOMPARE(night.color(TokenSet::accentText).name(), QStringLiteral("#b48cff"));
    const TokenSet day = TokenSet::table(false);
    QCOMPARE(day.color(TokenSet::accentHover).name(), QStringLiteral("#6c2ff0"));
    QCOMPARE(day.color(TokenSet::accentText).name(), QStringLiteral("#6a2fe6"));

    // Any other accent goes through the rule.
    const TokenSet blue = TokenSet::table(true, QColor(QStringLiteral("#3daee9")));
    QCOMPARE(blue.color(TokenSet::accentHover).name(), QStringLiteral("#54b8ec"));
    QCOMPARE(blue.color(TokenSet::textOnAccent).name(), QStringLiteral("#17151c"));
}

void RayeUiTokensTest::selectedFillFollowsTheAccent()
{
    const QColor selected = TokenSet::table(true, QColor(QStringLiteral("#3daee9"))).color(TokenSet::fillSubtleSelected);
    QCOMPARE(selected.rgb(), QColor(QStringLiteral("#3daee9")).rgb());
    QCOMPARE(selected.alpha(), qRound(0.09 * 255));
    QCOMPARE(TokenSet::table(false).color(TokenSet::fillSubtleSelected).alpha(), qRound(0.06 * 255));
}

void RayeUiTokensTest::contrastOfTheTable()
{
    using Raye::Ui::contrast;
    for (bool night : {true, false}) {
        const TokenSet t = TokenSet::table(night);
        QVERIFY(contrast(t.color(TokenSet::textPrimary), t.color(TokenSet::bgBase)) >= 4.5);
        QVERIFY(contrast(t.color(TokenSet::textSecondary), t.color(TokenSet::bgLayerSolid)) >= 4.5);
        QVERIFY(contrast(t.color(TokenSet::textOnAccent), t.color(TokenSet::accent)) >= 4.5);
        QVERIFY(contrast(t.color(TokenSet::focusOuter), t.color(TokenSet::bgBase)) >= 3.0);
    }
    QCOMPARE(qRound(contrast(Qt::black, Qt::white) * 10), 210);
}

void RayeUiTokensTest::applicationTokensFollowThePalette()
{
    Tokens *tokens = Tokens::instance();
    QSignalSpy spy(tokens, &Tokens::changed);

    QApplication::setPalette(TokenSet::table(false).palette());
    QTRY_VERIFY(!tokens->night());
    QVERIFY(spy.count() >= 1);
    QCOMPARE(tokens->colors().value(QStringLiteral("bgBase")).value<QColor>(), QColor(QStringLiteral("#f5f4f7")));

    QApplication::setPalette(TokenSet::table(true).palette());
    QTRY_VERIFY(tokens->night());
    QCOMPARE(Tokens::current().color(TokenSet::bgBase), QColor(QStringLiteral("#141318")));
    QCOMPARE(tokens->alpha(Qt::white, 0.5).alpha(), 128);
}

void RayeUiTokensTest::qmlSeesTheSameTable()
{
    Raye::Ui::registerQmlTypes();
    Raye::Ui::registerQmlTypes(); // twice is harmless
    QApplication::setPalette(TokenSet::table(true).palette());

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import org.raye.files.kit
        QtObject {
            property bool night: Kit.night
            property color base: Kit.colors.bgBase
            property int height: Kit.controlHeight
            property int fast: Kit.durationFast
        })",
                      QUrl());
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    QCOMPARE(object->property("night").toBool(), true);
    QCOMPARE(object->property("base").value<QColor>(), QColor(QStringLiteral("#141318")));
    QCOMPARE(object->property("height").toInt(), 32);

    // The binding follows a scheme change.
    QApplication::setPalette(TokenSet::table(false).palette());
    QTRY_COMPARE(object->property("base").value<QColor>(), QColor(QStringLiteral("#f5f4f7")));
    QApplication::setPalette(TokenSet::table(true).palette());
}

void RayeUiTokensTest::matchesTheTheme()
{
    // org.raye.ui is the theme's copy of the same table. Checked when it can
    // be loaded: installed (raye-style), or a tree named in RAYE_THEME_QML_DIR.
    QQmlEngine engine;
    const QString dir = qEnvironmentVariable("RAYE_THEME_QML_DIR");
    if (!dir.isEmpty()) {
        engine.addImportPath(dir);
    }
    for (const QString &mode : {QStringLiteral("night"), QStringLiteral("day")}) {
        QQmlComponent component(&engine);
        component.setData(QStringLiteral("import QtQuick\nimport org.raye.ui\nQtObject { property QtObject t: Tokens; Component.onCompleted: Tokens.forceMode = \"%1\" }").arg(mode).toUtf8(), QUrl());
        std::unique_ptr<QObject> object(component.create());
        if (!object) {
            if (!dir.isEmpty()) {
                QFAIL(qPrintable(QStringLiteral("org.raye.ui did not load from %1: %2").arg(dir, component.errorString())));
            }
            qInfo("org.raye.ui is not installed here: the table is not compared with the theme");
            return;
        }
        auto *theme = object->property("t").value<QObject *>();
        QVERIFY(theme);
        const TokenSet table = TokenSet::table(mode == QLatin1String("night"));
        int compared = 0;
        for (const QString &name : TokenSet::roleNames()) {
            const QVariant value = theme->property(name.toLatin1().constData());
            if (!value.isValid()) {
                continue;
            }
            QCOMPARE(QStringLiteral("%1 %2 %3").arg(mode, name, value.value<QColor>().name(QColor::HexArgb)),
                     QStringLiteral("%1 %2 %3").arg(mode, name, table.color(name).name(QColor::HexArgb)));
            ++compared;
        }
        QVERIFY(compared >= 70);
    }
}

QTEST_MAIN(RayeUiTokensTest)

#include "test_ui_tokens.moc"
