/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "subfolders.h"

#include <KIO/ListJob>

#include <QCollator>

#include <algorithm>

namespace Raye::Address
{

Subfolders::Subfolders(QObject *parent)
    : QObject(parent)
{
}

Subfolders::~Subfolders()
{
    cancel();
}

void Subfolders::setLocationNames(const Locations::LocationNames &names)
{
    m_names = names;
}

void Subfolders::cancel()
{
    if (m_job) {
        m_job->disconnect(this);
        m_job->kill();
    }
    m_job.clear();
}

void Subfolders::list(const QUrl &url)
{
    cancel();
    m_url = url;
    m_folders.clear();
    m_failed = false;

    if (!m_names) {
        m_names = Locations::LocationNames();
    }
    const QList<Locations::Crumb> crumbs = m_names->crumbs(url);
    // The Network lists its computers like a folder; other pages know their
    // children without asking anyone.
    if (crumbs.isEmpty() || (!crumbs.last().isFolder && crumbs.last().kind != Locations::Kind::Network)) {
        m_folders = m_names->pageChildren(url);
        Q_EMIT finished();
        return;
    }

    // Hidden folders stay out, as in the view by default.
    m_job = KIO::listDir(url, KIO::HideProgressInfo, KIO::ListJob::ListFlags{});
    connect(m_job, &KIO::ListJob::entries, this, [this](KIO::Job *, const KIO::UDSEntryList &entries) {
        for (const KIO::UDSEntry &entry : entries) {
            if (m_folders.size() >= maximum) {
                cancel();
                sortAndFinish();
                return;
            }
            const QString name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
            if (!entry.isDir() || name.isEmpty() || name == QLatin1String(".") || name == QLatin1String("..") || name.startsWith(QLatin1Char('.'))) {
                continue;
            }
            // Never let a name climb out of the folder listed.
            if (name.contains(QLatin1Char('/'))) {
                continue;
            }
            QUrl child = m_url;
            const QString base = child.path().endsWith(QLatin1Char('/')) ? child.path() : QString(child.path() + QLatin1Char('/'));
            child.setPath(base + name);
            const QString display = entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
            m_folders.append({child, display.isEmpty() ? name : display, QStringLiteral("folder"), Locations::Kind::Folder, true});
        }
    });
    connect(m_job, &KJob::result, this, [this](KJob *job) {
        m_failed = job->error() != 0;
        m_job.clear();
        sortAndFinish();
    });
}

void Subfolders::sortAndFinish()
{
    // Numbers in names compare as numbers; the C locale cannot do that.
    const QLocale locale = QLocale().language() == QLocale::C ? QLocale(QLocale::English) : QLocale();
    QCollator collator(locale);
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(m_folders.begin(), m_folders.end(), [&collator](const Locations::Crumb &a, const Locations::Crumb &b) {
        return collator.compare(a.text, b.text) < 0;
    });
    Q_EMIT finished();
}

}
