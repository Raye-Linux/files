/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wheelstep.h"

#include "tokens.h"

#include <QAbstractScrollArea>
#include <QScrollBar>

namespace Raye::Ui
{

void setWheelStep(QAbstractScrollArea *area)
{
    if (!area) {
        return;
    }
    area->verticalScrollBar()->setSingleStep(Metrics::wheelLine);
    area->horizontalScrollBar()->setSingleStep(Metrics::wheelLine);
}

}
