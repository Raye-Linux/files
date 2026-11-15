/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "uiexport.h"

#include <QList>
#include <QString>

#include <functional>

class QWidget;

namespace Raye::Ui
{

// The list of widgets the render harness (tests/raye/harness) can show by
// name. A package registers a builder that makes its widget with fake data;
// nothing here runs unless the harness asks for it:
//
//   RAYE_SHOWCASE(titleBar, "TitleBar", "Tabs and caption buttons")
//   {
//       auto *bar = new TitleBar(parent);
//       ... fill it with sample tabs ...
//       return bar;
//   }
namespace Showcase
{
using Builder = std::function<QWidget *(QWidget *parent)>;

struct Entry {
    QString name;
    QString description;
    Builder build;
};

// A second entry with the same name replaces the first.
RAYE_UI_EXPORT bool add(const QString &name, const QString &description, const Builder &build);
// Sorted by name.
RAYE_UI_EXPORT QList<Entry> entries();
// Case-insensitive; nullptr when nothing has that name.
RAYE_UI_EXPORT const Entry *find(const QString &name);
}

}

#define RAYE_SHOWCASE(id, name, description)                                                                                                           \
    static QWidget *rayeShowcase_##id(QWidget *parent);                                                                                                \
    [[maybe_unused]] static const bool rayeShowcaseAdded_##id = Raye::Ui::Showcase::add(QStringLiteral(name), QStringLiteral(description), rayeShowcase_##id); \
    static QWidget *rayeShowcase_##id(QWidget *parent)
