/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

#include <functional>

namespace Raye::Archive
{

// A QML page's signals have no C++ type to connect a lambda to; their
// old-style connections land here instead.
class PageSignals : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    std::function<void()> onBrowse;
    std::function<void()> onAccept;

public Q_SLOTS:
    void browse()
    {
        if (onBrowse) {
            onBrowse();
        }
    }
    void accept()
    {
        if (onAccept) {
            onAccept();
        }
    }
};

}
