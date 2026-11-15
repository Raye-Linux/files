/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include "locations/locationnames.h"

#include <QObject>
#include <QPointer>
#include <QUrl>

#include <optional>

class KJob;

namespace KIO
{
class ListJob;
}

namespace Raye::Address
{

// The folders a crumb's chevron offers (explorer spec 3.3): the folders in
// a folder, sorted the way the view sorts names ("a2" before "a10"),
// hidden ones left out; for This PC, its known folders and drives.
class DOLPHIN_EXPORT Subfolders : public QObject
{
    Q_OBJECT
public:
    // More than this and the list stops: a chevron is not a folder view.
    static constexpr int maximum = 500;

    explicit Subfolders(QObject *parent = nullptr);
    ~Subfolders() override;

    void setLocationNames(const Locations::LocationNames &names);

    // Starts listing; finished() comes when it is done (at once for pages).
    void list(const QUrl &url);
    void cancel();

    QUrl url() const
    {
        return m_url;
    }
    QList<Locations::Crumb> folders() const
    {
        return m_folders;
    }
    bool failed() const
    {
        return m_failed;
    }

Q_SIGNALS:
    void finished();

private:
    void sortAndFinish();

    std::optional<Locations::LocationNames> m_names;
    QUrl m_url;
    QPointer<KIO::ListJob> m_job;
    QList<Locations::Crumb> m_folders;
    bool m_failed = false;
};

}
