/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "archiveexport.h"

#include <QtGlobal>

namespace Raye::Archive
{

// Stops an archive that unpacks to far more than it weighs (a "zip bomb")
// before it fills the disk. An extraction is stopped once it has written
// more than 2 GB and more than 100 times the size of the archive, or more
// than a million items; the user can say go on, and it runs again with the
// guard off. A big archive that is not abnormally compressed (a 6 GB disk
// image in a 5 GB TAR) never stops.
class RAYE_ARCHIVE_EXPORT BombGuard
{
public:
    static constexpr qint64 sizeLimit = qint64(2) * 1024 * 1024 * 1024;
    static constexpr qint64 ratioLimit = 100;
    static constexpr qint64 itemLimit = 1000000;

    explicit BombGuard(qint64 archiveSize, bool enabled = true);

    // Bytes written so far (sparse holes count: they fill the disk the same
    // way once copied). False once the caps are passed.
    bool addBytes(qint64 bytes);
    // One more item written. False once the caps are passed.
    bool addItem();

    bool tripped() const
    {
        return m_tripped;
    }
    qint64 written() const
    {
        return m_written;
    }

private:
    bool check();

    qint64 m_archiveSize;
    qint64 m_written = 0;
    qint64 m_items = 0;
    bool m_enabled;
    bool m_tripped = false;
};

}
