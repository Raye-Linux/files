/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

#include <functional>

namespace Raye::Jobs
{

// Takes the signals of a QML page (connected by name) to C++ callbacks.
class PageRelay : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    std::function<void(int, bool)> onChosen;
    std::function<void()> onComparing;

public Q_SLOTS:
    void chosen(int choice, bool applyToAll)
    {
        if (onChosen) {
            onChosen(choice, applyToAll);
        }
    }
    void comparingChanged()
    {
        if (onComparing) {
            onComparing();
        }
    }
};

}
