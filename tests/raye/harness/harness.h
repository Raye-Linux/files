/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

// The render harness: shows any Raye widget (by its showcase name) or QML
// page of org.raye.files on a window painted in Raye Night or Day, in
// English or French, and can save what a Raye session would show:
//
//   raye_test_ui_harness --list
//   raye_test_ui_harness --widget Kit --theme day --lang fr --size 1920x1080 --shot kit.png
//   raye_test_ui_harness --page UsageBar --shot bar.png
//   raye_test_ui_harness --qml tests/raye/harness/KitGallery.qml
//
// Without --shot the window stays open (for a look under Xvfb). Motion is
// off while shooting, so every control is drawn in its final state.

#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>

class QWidget;

namespace RayeHarness
{

struct Options {
    QString widget; // a showcase name
    QString page; // a QML type of org.raye.files
    QString qmlFile; // a QML file
    bool night = true;
    QString lang = QStringLiteral("en");
    QSize size = QSize(1280, 800);
    QString shot; // where to save the PNG; empty: stay open
    bool list = false;
    bool help = false;
};

// Reads the command line; false with a message when it makes no sense.
bool parseArguments(const QStringList &arguments, Options *options, QString *error);
QString usage();

// The application palette of the Raye scheme, and the icon theme with it.
void applyTheme(bool night);
// The raye-files catalog in that language, from the build tree. False
// when the system has no locale for it (the text then stays English).
bool applyLanguage(const QString &lang);
QString language();

// True while a shot is taken, or when a compositor runs: showcases then
// open their flyouts and dialogs translucent, as in a Raye session.
bool compositing();

// Sample text of the harness itself (not Files' strings): English or
// French, following applyLanguage().
QString text(const char *english, const char *french);

// The window with the widget or page in it, sized and painted with the
// base colour; nullptr and a message when the name is unknown.
QWidget *buildWindow(const Options &options, QString *error);

// What a compositor would put on screen: the window, and on top of it
// every popup and dialog the application shows, with their shadows and
// the acrylic blur behind translucent ones.
QImage composite(QWidget *window);

// Everything the command line asks for; returns the exit code.
int run(const Options &options);

// The widget kit's own showcases (Kit, CommandBar, Flyout, CardDialog,
// KitQml); the other packages add theirs with RAYE_SHOWCASE.
void registerKitShowcases();

}
