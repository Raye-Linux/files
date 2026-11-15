/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

// The kit is part of the private library; the tests and the render harness
// link against it, so what they use must be visible from outside.
#define RAYE_UI_EXPORT DOLPHIN_EXPORT
