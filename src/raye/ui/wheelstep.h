/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "uiexport.h"

class QAbstractScrollArea;

namespace Raye::Ui
{

// The Windows wheel step on a widget scroll area: one notch moves it by the
// "lines per notch" setting times Metrics::wheelLine, about 100 px. Item
// views set their own step on every layout: call it again from
// updateGeometries() there.
RAYE_UI_EXPORT void setWheelStep(QAbstractScrollArea *area);

}
