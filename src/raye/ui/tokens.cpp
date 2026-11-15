/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tokens.h"
#include "motion.h"

#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QHash>
#include <QQmlEngine>
#include <QtMath>

#include <cmath>

namespace Raye::Ui
{

namespace
{
struct TableRow {
    const char *name;
    QRgb night;
    QRgb day;
};

constexpr TableRow s_table[] = {
#define RAYE_TOKEN(name, night, day) {#name, night, day},
#include "tokentable.h"
#undef RAYE_TOKEN
};

constexpr int s_tableSize = sizeof(s_table) / sizeof(s_table[0]);

const char *const s_extraNames[] = {
    "accent",
    "accentHover",
    "accentPressed",
    "accentText",
    "accentDisabled",
    "textOnAccent",
    "fillSubtleSelected",
};

static_assert(s_tableSize + int(sizeof(s_extraNames) / sizeof(s_extraNames[0])) == TokenSet::RoleCount);

// The hand-tuned shades of the Raye violet (theme token table).
const AccentShades s_nightViolet = {QColor(0x7b3cff), QColor(0x8950ff), QColor(0x6a2fe6), QColor(0xb48cff), QColor(0x3a2a5e), QColor(0xffffff)};
const AccentShades s_dayViolet = {QColor(0x7b3cff), QColor(0x6c2ff0), QColor(0x5f27d6), QColor(0x6a2fe6), QColor(0xc9b8f2), QColor(0xffffff)};

// The surface the accent rule measures against: bg.base of each mode.
const QColor s_nightBase(0x141318);
const QColor s_dayBase(0xf5f4f7);
// The scheme files of Raye 0.2, until the theme package replaces them.
const QColor s_oldNightBase(0x130e1f);
const QColor s_oldDayBase(0xf6f3fb);

qreal channel(int v)
{
    const qreal c = v / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

QColor grey(const QColor &c)
{
    const int y = qRound((0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF()) * 255);
    return QColor(y, y, y);
}

QColor opposite(const QColor &c)
{
    if (c.rgb() == QColor(Qt::white).rgb()) {
        return QColor(Qt::black);
    }
    if (c.rgb() == QColor(Qt::black).rgb()) {
        return QColor(Qt::white);
    }
    return {};
}

bool sameRgb(const QColor &a, const QColor &b)
{
    return a.isValid() && b.isValid() && a.rgba() == b.rgba();
}
}

QFont textFont(TextStyle style)
{
    QFont f;
    // Inter first; the families after it only matter on a system without it.
    f.setFamilies({QStringLiteral("Inter"), QGuiApplication::font().family()});
    switch (style) {
    case TextStyle::Caption:
        f.setPointSizeF(9);
        f.setWeight(QFont::Normal);
        break;
    case TextStyle::Body:
        f.setPointSizeF(10);
        f.setWeight(QFont::Normal);
        break;
    case TextStyle::BodyStrong:
        f.setPointSizeF(10);
        f.setWeight(QFont::DemiBold);
        break;
    case TextStyle::BodyLarge:
        f.setPointSizeF(12.75);
        f.setWeight(QFont::Normal);
        break;
    case TextStyle::Subtitle:
        f.setPointSizeF(14.25);
        f.setWeight(QFont::DemiBold);
        break;
    case TextStyle::Title:
        f.setFamilies({QStringLiteral("Inter Display"), QStringLiteral("Inter"), QGuiApplication::font().family()});
        f.setPointSizeF(20.25);
        f.setWeight(QFont::DemiBold);
        break;
    }
    return f;
}

qreal luminance(const QColor &color)
{
    return 0.2126 * channel(color.red()) + 0.7152 * channel(color.green()) + 0.0722 * channel(color.blue());
}

qreal contrast(const QColor &a, const QColor &b)
{
    const qreal la = luminance(a);
    const qreal lb = luminance(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    auto ch = [t](int x, int y) {
        return qRound((x / 255.0 + (y / 255.0 - x / 255.0) * t) * 255);
    };
    return QColor(ch(a.red(), b.red()), ch(a.green(), b.green()), ch(a.blue(), b.blue()));
}

QColor withAlpha(const QColor &color, qreal alpha)
{
    QColor c = color;
    c.setAlphaF(qBound(0.0, alpha, 1.0) * color.alphaF());
    return c;
}

AccentShades deriveAccent(const QColor &accentColor, bool night)
{
    // Same steps as org.raye.ui Tokens.deriveAccent(), so a custom accent
    // looks the same in Files' widgets and in every Raye QML page.
    const QColor accent = QColor(accentColor.rgb());
    const QColor base = night ? s_nightBase : s_dayBase;
    const QColor light(Qt::white);
    const QColor dark(0x17151c);
    constexpr qreal want = 4.5;

    QColor onAccent;
    if (contrast(light, accent) >= want) {
        onAccent = light;
    } else if (contrast(dark, accent) >= want) {
        onAccent = dark;
    } else {
        onAccent = contrast(light, accent) >= contrast(dark, accent) ? light : dark;
    }
    const qreal floor = qMin(want, contrast(onAccent, accent));

    auto backOff = [&](const QColor &toward, qreal amount) {
        while (amount > 0 && contrast(onAccent, mix(accent, toward, amount)) < floor) {
            amount = qMax(0.0, std::round((amount - 0.005) * 1e6) / 1e6);
        }
        return amount;
    };

    const qreal hoverBy = 0.12;
    const qreal pressBy = night ? 0.12 : 0.24;
    QColor pressedTo(Qt::black);
    qreal pressedBy = backOff(pressedTo, pressBy);
    const QColor other = opposite(pressedTo);
    if (pressedBy < pressBy / 2 && other.isValid()) {
        const qreal flipped = backOff(other, pressBy);
        if (flipped >= pressBy / 2) {
            pressedTo = other;
            pressedBy = flipped;
        }
    }
    QColor hoveredTo = night ? QColor(Qt::white) : QColor(Qt::black);
    qreal hoveredBy = backOff(hoveredTo, hoverBy);
    if (hoveredBy < hoverBy / 2 && !sameRgb(pressedTo, hoveredTo) && pressedBy > 0) {
        hoveredTo = pressedTo;
        hoveredBy = pressedBy / 2;
    } else if (sameRgb(pressedTo, hoveredTo)) {
        hoveredBy = qMin(hoveredBy, pressedBy / 2);
    }

    const QColor toward = night ? QColor(Qt::white) : QColor(Qt::black);
    const qreal wantText = night ? 7 : 6;
    QColor text = accent;
    qreal t = 0;
    while (contrast(text, base) < wantText && t < 1) {
        t = qMin(1.0, t + 0.02);
        text = mix(accent, toward, t);
    }
    const QColor disabled = mix(base, accent, 0.35);

    return {accent, mix(accent, hoveredTo, hoveredBy), mix(accent, pressedTo, pressedBy), text, mix(disabled, grey(disabled), 0.3), onAccent};
}

QColor TokenSet::defaultAccent()
{
    return QColor(0x7b3cff);
}

TokenSet TokenSet::table(bool night, const QColor &accentColor)
{
    TokenSet set;
    set.m_night = night;
    set.m_raye = true;
    for (int i = 0; i < s_tableSize; ++i) {
        set.m_colors[i] = QColor::fromRgba(night ? s_table[i].night : s_table[i].day);
    }
    set.setAccent(accentColor.isValid() ? accentColor : defaultAccent());
    return set;
}

void TokenSet::setAccent(const QColor &accentColor)
{
    const QColor plain(accentColor.rgb());
    const AccentShades shades = sameRgb(plain, defaultAccent()) ? (m_night ? s_nightViolet : s_dayViolet) : deriveAccent(plain, m_night);
    m_colors[accent] = shades.accent;
    m_colors[accentHover] = shades.hover;
    m_colors[accentPressed] = shades.pressed;
    m_colors[accentText] = shades.text;
    m_colors[accentDisabled] = shades.disabled;
    m_colors[textOnAccent] = shades.onAccent;
    m_colors[fillSubtleSelected] = withAlpha(shades.accent, m_night ? 0.09 : 0.06);
}

bool TokenSet::isRayePalette(const QPalette &palette)
{
    const QRgb window = palette.color(QPalette::Active, QPalette::Window).rgba();
    return window == s_nightBase.rgba() || window == s_dayBase.rgba() || window == s_oldNightBase.rgba() || window == s_oldDayBase.rgba();
}

bool TokenSet::isUnthemedPalette(const QPalette &palette)
{
    const QColor window = palette.color(QPalette::Active, QPalette::Window);
    const QColor text = palette.color(QPalette::Active, QPalette::WindowText);
    const QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
    if (contrast(window, text) <= 1.5) {
        return true;
    }
    const bool qtWindow = sameRgb(window, QColor(0xefefef)) || sameRgb(window, QColor(0xeff0f1));
    const bool qtHighlight = sameRgb(highlight, QColor(0x308cc6)) || sameRgb(highlight, QColor(0x3daee9));
    return qtWindow && qtHighlight;
}

TokenSet TokenSet::fromPalette(const QPalette &palette)
{
    if (isUnthemedPalette(palette)) {
        return table(true);
    }
    const QColor window = palette.color(QPalette::Active, QPalette::Window);
    const QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
    if (isRayePalette(palette)) {
        return table(window.rgba() == s_nightBase.rgba() || window.rgba() == s_oldNightBase.rgba(), highlight);
    }

    // A scheme that is not Raye's: its surfaces and text, Raye's overlays.
    TokenSet set = table(luminance(window) < 0.5, highlight);
    set.m_raye = false;
    const QColor base = palette.color(QPalette::Active, QPalette::Base);
    set.m_colors[bgBase] = window;
    set.m_colors[bgLayer] = base;
    set.m_colors[bgLayerSolid] = base;
    set.m_colors[textPrimary] = palette.color(QPalette::Active, QPalette::WindowText);
    set.m_colors[focusOuter] = palette.color(QPalette::Active, QPalette::WindowText);
    const QColor placeholder = palette.color(QPalette::Active, QPalette::PlaceholderText);
    if (placeholder.isValid() && contrast(placeholder, window) >= 3.0) {
        set.m_colors[textSecondary] = placeholder;
    }
    const QColor disabled = palette.color(QPalette::Disabled, QPalette::WindowText);
    if (disabled.isValid() && !sameRgb(disabled, set.m_colors[textPrimary])) {
        set.m_colors[textDisabled] = disabled;
    }
    const QColor tooltip = palette.color(QPalette::Active, QPalette::ToolTipBase);
    if (tooltip.isValid()) {
        set.m_colors[bgTooltip] = tooltip;
    }
    return set;
}

QColor TokenSet::color(const QString &name) const
{
    static const QHash<QString, int> index = [] {
        QHash<QString, int> h;
        const QStringList names = roleNames();
        for (int i = 0; i < names.size(); ++i) {
            h.insert(names.at(i), i);
        }
        return h;
    }();
    const auto it = index.constFind(name);
    return it == index.constEnd() ? QColor() : m_colors[it.value()];
}

QString TokenSet::roleName(Role role)
{
    if (role < s_tableSize) {
        return QString::fromLatin1(s_table[role].name);
    }
    if (role < RoleCount) {
        return QString::fromLatin1(s_extraNames[role - s_tableSize]);
    }
    return {};
}

QStringList TokenSet::roleNames()
{
    QStringList names;
    names.reserve(RoleCount);
    for (int r = 0; r < RoleCount; ++r) {
        names.append(roleName(Role(r)));
    }
    return names;
}

QVariantMap TokenSet::toVariantMap() const
{
    QVariantMap map;
    for (int r = 0; r < RoleCount; ++r) {
        map.insert(roleName(Role(r)), m_colors[r]);
    }
    return map;
}

QPalette TokenSet::palette() const
{
    // What the Raye colour scheme gives an application (theme 3.2 mapping).
    QPalette p;
    const auto set = [&p](QPalette::ColorRole role, const QColor &c) {
        p.setColor(QPalette::Active, role, c);
        p.setColor(QPalette::Inactive, role, c);
        p.setColor(QPalette::Disabled, role, c);
    };
    set(QPalette::Window, m_colors[bgBase]);
    set(QPalette::WindowText, m_colors[textPrimary]);
    set(QPalette::Base, m_colors[bgLayerSolid]);
    set(QPalette::AlternateBase, m_colors[bgCardSolid]);
    set(QPalette::Text, m_colors[textPrimary]);
    set(QPalette::PlaceholderText, m_colors[textTertiary]);
    set(QPalette::Button, m_colors[fillControlSolid]);
    set(QPalette::ButtonText, m_colors[textPrimary]);
    set(QPalette::BrightText, m_colors[textPrimary]);
    set(QPalette::ToolTipBase, m_colors[bgTooltip]);
    set(QPalette::ToolTipText, m_colors[textPrimary]);
    set(QPalette::Highlight, m_colors[accent]);
    set(QPalette::HighlightedText, m_colors[textOnAccent]);
    set(QPalette::Accent, m_colors[accent]);
    set(QPalette::Link, m_colors[accentText]);
    set(QPalette::LinkVisited, m_colors[linkVisited]);
    set(QPalette::Light, m_colors[bgQuarternary]);
    set(QPalette::Midlight, m_colors[fillControlHoverSolid]);
    set(QPalette::Mid, m_colors[strokeControlSolid]);
    set(QPalette::Dark, m_colors[bgSecondary]);
    set(QPalette::Shadow, m_colors[shadow]);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        p.setColor(QPalette::Disabled, role, m_colors[textDisabled]);
    }
    p.setColor(QPalette::Disabled, QPalette::Highlight, m_colors[accentDisabled]);
    p.setColor(QPalette::Inactive, QPalette::WindowText, m_colors[textPrimary]);
    return p;
}

bool TokenSet::operator==(const TokenSet &other) const
{
    return m_night == other.m_night && m_raye == other.m_raye && m_colors == other.m_colors;
}

Tokens::Tokens(QObject *parent)
    : QObject(parent)
{
    if (qApp) {
        qApp->installEventFilter(this);
    }
    connect(Motion::notifier(), &Motion::Notifier::speedChanged, this, &Tokens::motionChanged);
    refresh();
}

Tokens *Tokens::instance()
{
    // Owned by the application: gone with it, never before a widget is.
    static Tokens *tokens = nullptr;
    if (!tokens) {
        tokens = new Tokens(qApp);
    }
    return tokens;
}

const TokenSet &Tokens::current()
{
    return instance()->set();
}

void Tokens::refresh()
{
    const TokenSet set = TokenSet::fromPalette(QGuiApplication::palette());
    if (set == m_set && !m_map.isEmpty()) {
        return;
    }
    m_set = set;
    m_map = m_set.toVariantMap();
    Q_EMIT changed();
}

bool Tokens::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange) {
        refresh();
    }
    return QObject::eventFilter(watched, event);
}

QColor Tokens::alpha(const QColor &color, qreal alpha) const
{
    return withAlpha(color, alpha);
}

int Tokens::durationFaster() const
{
    return Motion::duration(Motion::Faster);
}

int Tokens::durationFast() const
{
    return Motion::duration(Motion::Fast);
}

int Tokens::durationNormal() const
{
    return Motion::duration(Motion::Normal);
}

int Tokens::durationSlow() const
{
    return Motion::duration(Motion::Slow);
}

int Tokens::durationExit() const
{
    return Motion::duration(Motion::Exit);
}

void registerQmlTypes()
{
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    qmlRegisterSingletonType<Tokens>("org.raye.files.kit", 1, 0, "Kit", [](QQmlEngine *, QJSEngine *) -> QObject * {
        Tokens *tokens = Tokens::instance();
        // The application owns it, not the engine that asked first.
        QQmlEngine::setObjectOwnership(tokens, QQmlEngine::CppOwnership);
        return tokens;
    });
}

}

// Any QML engine in Files can import org.raye.files.kit without a call.
static void rayeUiRegisterAtStartup()
{
    Raye::Ui::registerQmlTypes();
}
Q_COREAPP_STARTUP_FUNCTION(rayeUiRegisterAtStartup)
