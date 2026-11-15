/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archiveguard.h"

#include <limits>

namespace Raye::Archive
{

BombGuard::BombGuard(qint64 archiveSize, bool enabled)
    : m_archiveSize(qMax<qint64>(archiveSize, 1))
    , m_enabled(enabled)
{
}

bool BombGuard::addBytes(qint64 bytes)
{
    if (bytes > 0) {
        m_written = bytes > std::numeric_limits<qint64>::max() - m_written ? std::numeric_limits<qint64>::max() : m_written + bytes;
    }
    return check();
}

bool BombGuard::addItem()
{
    ++m_items;
    return check();
}

bool BombGuard::check()
{
    if (!m_enabled) {
        return true;
    }
    if (!m_tripped) {
        const bool tooBig = m_written > sizeLimit && m_written / ratioLimit > m_archiveSize;
        m_tripped = tooBig || m_items > itemLimit;
    }
    return !m_tripped;
}

}
