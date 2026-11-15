/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "uiexport.h"

#include <QColor>
#include <QFont>
#include <QObject>
#include <QPalette>
#include <QStringList>
#include <QVariantMap>

#include <array>

namespace Raye::Ui
{

// Sizes of the Raye controls in Files, in logical pixels at 100 %. The
// colours are in TokenSet; these never change with the mode.
namespace Metrics
{
constexpr int controlHeight = 32;
constexpr int compactHeight = 24;
constexpr int iconButtonWidth = 40; // a command bar button with no label
constexpr int labelPadding = 12; // left and right of a labelled button
constexpr int iconSize = 16;
constexpr int iconTextGap = 8;
constexpr int chevronSize = 12;
constexpr int splitChevronWidth = 28; // the menu half of a split button
constexpr int radiusControl = 4;
constexpr int radiusOverlay = 8;
constexpr int radiusSmall = 2;
constexpr int focusMargin = 3; // focus ring outside the control
constexpr int focusOuter = 2;
constexpr int focusInner = 1;
constexpr int focusGlow = 2;
constexpr int separatorLength = 24;
constexpr int flyoutPadding = 4;
constexpr int flyoutGap = 4; // between the anchor and the flyout
constexpr int flyoutSlide = 6; // the popup comes in from this far
constexpr int menuItemHeight = 32;
constexpr int pivotHeight = 40;
constexpr int pivotPadding = 12;
constexpr int pillWidth = 16; // selected pivot item underline
constexpr int pillHeight = 3;
constexpr int usageBarHeight = 4;
constexpr int dialogPadding = 24;
constexpr int dialogButtonRow = 80;
constexpr int dialogButtonGap = 8;
constexpr qreal usageCautionAbove = 0.9; // a drive this full turns to the caution colour
}

// Type ramp (theming spec 3.4): Inter, sizes in points at 96 dpi.
enum class TextStyle {
    Caption, // 12 px
    Body, // 13.33 px
    BodyStrong, // 13.33 px, 600
    BodyLarge, // 17 px
    Subtitle, // 19 px, 600
    Title, // 27 px, 600
};

RAYE_UI_EXPORT QFont textFont(TextStyle style);

// WCAG 2 relative luminance and contrast ratio.
RAYE_UI_EXPORT qreal luminance(const QColor &color);
RAYE_UI_EXPORT qreal contrast(const QColor &a, const QColor &b);
// a moved toward b by t (0..1), on 8 bits like the stored colours
RAYE_UI_EXPORT QColor mix(const QColor &a, const QColor &b, qreal t);
// colour at alpha, the way the table writes "drawn over the surface"
RAYE_UI_EXPORT QColor withAlpha(const QColor &color, qreal alpha);

// The shades of an accent, the rule org.raye.ui and the Qt style use.
struct AccentShades {
    QColor accent;
    QColor hover;
    QColor pressed;
    QColor text;
    QColor disabled;
    QColor onAccent;
};
RAYE_UI_EXPORT AccentShades deriveAccent(const QColor &accent, bool night);

// Every Raye colour for one mode. Built from the palette the application
// paints with: the Raye colour schemes give exactly the theme's table; any
// other palette (a scheme the user picked, no scheme at all) keeps its own
// window, view and text colours and the Raye overlays on top of them.
class RAYE_UI_EXPORT TokenSet
{
public:
    enum Role {
#define RAYE_TOKEN(name, night, day) name,
#include "tokentable.h"
#undef RAYE_TOKEN
        accent,
        accentHover,
        accentPressed,
        accentText,
        accentDisabled,
        textOnAccent,
        fillSubtleSelected,
        RoleCount,
    };

    // The theme's own values, with the given accent.
    static TokenSet table(bool night, const QColor &accentColor = defaultAccent());
    static TokenSet fromPalette(const QPalette &palette);
    static QColor defaultAccent();

    // True when the palette is one of the Raye schemes.
    static bool isRayePalette(const QPalette &palette);
    // Qt's own palette and Plasma's built-in one, with no colour scheme
    // behind them: Raye paints Raye Night then, like org.raye.ui does.
    static bool isUnthemedPalette(const QPalette &palette);

    bool night() const
    {
        return m_night;
    }
    bool raye() const
    {
        return m_raye;
    }
    QColor color(Role role) const
    {
        return m_colors[role];
    }
    // By QML name ("bgBase"); invalid for an unknown name.
    QColor color(const QString &name) const;

    static QString roleName(Role role);
    static QStringList roleNames();

    QVariantMap toVariantMap() const;
    // A QPalette painting this set, for a program that has no Raye scheme
    // installed (the render harness).
    QPalette palette() const;

    bool operator==(const TokenSet &other) const;

private:
    void setAccent(const QColor &accentColor);

    bool m_night = true;
    bool m_raye = true;
    std::array<QColor, RoleCount> m_colors;
};

// The tokens of the running application, for widgets and QML alike. It
// follows the application palette, so a scheme or accent change repaints
// every Raye control without code. In QML: import org.raye.files.kit,
// then Kit.colors.bgBase, Kit.night, Kit.durationFast...
class RAYE_UI_EXPORT Tokens : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool night READ night NOTIFY changed)
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY changed)
    Q_PROPERTY(int controlHeight READ controlHeight CONSTANT)
    Q_PROPERTY(int compactHeight READ compactHeight CONSTANT)
    Q_PROPERTY(int iconButtonWidth READ iconButtonWidth CONSTANT)
    Q_PROPERTY(int iconSize READ iconSize CONSTANT)
    Q_PROPERTY(int radiusControl READ radiusControl CONSTANT)
    Q_PROPERTY(int radiusOverlay READ radiusOverlay CONSTANT)
    Q_PROPERTY(int durationFaster READ durationFaster NOTIFY motionChanged)
    Q_PROPERTY(int durationFast READ durationFast NOTIFY motionChanged)
    Q_PROPERTY(int durationNormal READ durationNormal NOTIFY motionChanged)
    Q_PROPERTY(int durationSlow READ durationSlow NOTIFY motionChanged)
    Q_PROPERTY(int durationExit READ durationExit NOTIFY motionChanged)
    Q_PROPERTY(QFont caption READ caption CONSTANT)
    Q_PROPERTY(QFont body READ body CONSTANT)
    Q_PROPERTY(QFont bodyStrong READ bodyStrong CONSTANT)
    Q_PROPERTY(QFont subtitle READ subtitle CONSTANT)

public:
    static Tokens *instance();

    const TokenSet &set() const
    {
        return m_set;
    }
    // Shorthand for widgets: Tokens::current().color(TokenSet::bgBase).
    static const TokenSet &current();

    bool night() const
    {
        return m_set.night();
    }
    QVariantMap colors() const
    {
        return m_map;
    }
    int controlHeight() const
    {
        return Metrics::controlHeight;
    }
    int compactHeight() const
    {
        return Metrics::compactHeight;
    }
    int iconButtonWidth() const
    {
        return Metrics::iconButtonWidth;
    }
    int iconSize() const
    {
        return Metrics::iconSize;
    }
    int radiusControl() const
    {
        return Metrics::radiusControl;
    }
    int radiusOverlay() const
    {
        return Metrics::radiusOverlay;
    }
    int durationFaster() const;
    int durationFast() const;
    int durationNormal() const;
    int durationSlow() const;
    int durationExit() const;
    QFont caption() const
    {
        return textFont(TextStyle::Caption);
    }
    QFont body() const
    {
        return textFont(TextStyle::Body);
    }
    QFont bodyStrong() const
    {
        return textFont(TextStyle::BodyStrong);
    }
    QFont subtitle() const
    {
        return textFont(TextStyle::Subtitle);
    }

    // The colour at alpha, for QML: Kit.alpha(Kit.colors.accent, 0.35).
    Q_INVOKABLE QColor alpha(const QColor &color, qreal alpha) const;

    // Read the palette again (done on every palette change).
    void refresh();

Q_SIGNALS:
    void changed();
    void motionChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit Tokens(QObject *parent = nullptr);

    TokenSet m_set;
    QVariantMap m_map;
};

// Registers org.raye.files.kit (Kit, the Tokens above) for QML. Done once
// when the application starts; calling it again does nothing.
RAYE_UI_EXPORT void registerQmlTypes();

}
