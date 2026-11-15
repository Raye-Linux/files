/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "appchoices.h"

#include <KApplicationTrader>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KShell>
#include <KSycoca>

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>

namespace Raye::OpenWith
{

namespace
{
bool opensFiles(const KService::Ptr &s)
{
    const QString exec = s->exec();
    return exec.contains(QLatin1String("%f")) || exec.contains(QLatin1String("%F")) || exec.contains(QLatin1String("%u"))
        || exec.contains(QLatin1String("%U"));
}

bool shown(const KService::Ptr &s)
{
    return s && s->isApplication() && !s->noDisplay() && !s->exec().isEmpty();
}

// A program the user browsed to is kept hidden from menus but belongs in
// the list of its type.
bool browsedProgram(const KService::Ptr &s)
{
    return s && s->isApplication() && s->storageId().startsWith(QLatin1String("raye-openwith-"));
}
}

AppChoicesModel::AppChoicesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

KService::List AppChoicesModel::recommendedApps(const QString &mime)
{
    KService::List out;
    // queryByMimeType honours mimeapps.list: the default comes first, then
    // the apps for the type and for the types it inherits.
    const KService::List found = KApplicationTrader::queryByMimeType(mime);
    for (const KService::Ptr &s : found) {
        if (shown(s) || browsedProgram(s)) {
            out.append(s);
        }
    }
    // A program browsed to is hidden from menus, and the query leaves it
    // out even when it is the default: put it back at the top.
    const KService::Ptr preferred = KApplicationTrader::preferredService(mime);
    if (browsedProgram(preferred) && std::none_of(out.cbegin(), out.cend(), [&preferred](const KService::Ptr &s) {
            return s->storageId() == preferred->storageId();
        })) {
        out.prepend(preferred);
    }
    return out;
}

KService::List AppChoicesModel::otherApps(const KService::List &except)
{
    QSet<QString> skip;
    for (const KService::Ptr &s : except) {
        skip.insert(s->storageId());
    }
    KService::List out = KApplicationTrader::query([&skip](const KService::Ptr &s) {
        return shown(s) && opensFiles(s) && !skip.contains(s->storageId());
    });
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::sort(out.begin(), out.end(), [&collator](const KService::Ptr &a, const KService::Ptr &b) {
        return collator.compare(a->name(), b->name()) < 0;
    });
    return out;
}

void AppChoicesModel::setMimeType(const QString &mime)
{
    beginResetModel();
    m_mime = mime;
    const KService::List rec = recommendedApps(mime);
    m_services = rec;
    m_recommended = int(rec.size());
    m_services.append(otherApps(rec));
    const KService::Ptr preferred = KApplicationTrader::preferredService(mime);
    m_defaultId = preferred ? preferred->storageId() : QString();
    m_current = m_services.isEmpty() ? -1 : 0;
    endResetModel();
    Q_EMIT mimeTypeChanged();
    Q_EMIT currentIndexChanged();
}

void AppChoicesModel::prefer(const QStringList &desktopIds)
{
    KService::List wanted;
    for (const QString &id : desktopIds) {
        const QString storage = id.endsWith(QLatin1String(".desktop")) ? id : id + QLatin1String(".desktop");
        const KService::Ptr service = KService::serviceByStorageId(storage);
        if (service && service->isApplication()) {
            wanted.append(service);
        }
    }
    if (wanted.isEmpty()) {
        return;
    }
    beginResetModel();
    KService::List rest;
    int recommended = int(wanted.size());
    for (int i = 0; i < m_services.size(); ++i) {
        const KService::Ptr &s = m_services.at(i);
        const bool isWanted = std::any_of(wanted.cbegin(), wanted.cend(), [&s](const KService::Ptr &w) {
            return w->storageId() == s->storageId();
        });
        if (isWanted) {
            continue;
        }
        if (i < m_recommended) {
            ++recommended;
        }
        rest.append(s);
    }
    m_services = wanted + rest;
    m_recommended = recommended;
    m_current = 0;
    endResetModel();
    Q_EMIT mimeTypeChanged();
    Q_EMIT currentIndexChanged();
}

void AppChoicesModel::setCurrentIndex(int index)
{
    if (index < -1 || index >= int(m_services.size()) || index == m_current) {
        return;
    }
    m_current = index;
    Q_EMIT currentIndexChanged();
}

int AppChoicesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_services.size());
}

QHash<int, QByteArray> AppChoicesModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {IconRole, "iconName"},
        {StorageIdRole, "storageId"},
        {RecommendedRole, "recommended"},
        {DefaultRole, "isDefault"},
        {CommentRole, "comment"},
    };
}

QVariant AppChoicesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= int(m_services.size())) {
        return {};
    }
    const KService::Ptr &s = m_services.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return s->name();
    case IconRole:
        return s->icon().isEmpty() ? QStringLiteral("application-x-executable") : s->icon();
    case StorageIdRole:
        return s->storageId();
    case RecommendedRole:
        return index.row() < m_recommended;
    case DefaultRole:
        return !m_defaultId.isEmpty() && s->storageId() == m_defaultId;
    case CommentRole:
        return s->genericName().isEmpty() ? s->comment() : s->genericName();
    }
    return {};
}

KService::Ptr AppChoicesModel::service(int row) const
{
    return row >= 0 && row < int(m_services.size()) ? m_services.at(row) : KService::Ptr();
}

KService::Ptr AppChoicesModel::currentService() const
{
    return service(m_current);
}

void AppChoicesModel::addBrowsed(const KService::Ptr &service)
{
    if (!service) {
        return;
    }
    beginResetModel();
    for (int i = 0; i < int(m_services.size()); ++i) {
        if (m_services.at(i)->storageId() == service->storageId() && !service->storageId().isEmpty()) {
            if (i < m_recommended) {
                --m_recommended;
            }
            m_services.removeAt(i);
            break;
        }
    }
    m_services.prepend(service);
    ++m_recommended;
    m_current = 0;
    endResetModel();
    Q_EMIT mimeTypeChanged();
    Q_EMIT currentIndexChanged();
}

bool setAlways(const QString &mime, const KService::Ptr &service)
{
    if (mime.isEmpty() || !service || service->storageId().isEmpty()) {
        return false;
    }
    KApplicationTrader::setPreferredService(mime, service);
    return true;
}

KService::Ptr serviceForProgram(const QString &path, const QString &mime)
{
    const QFileInfo fi(path);
    if (!fi.isFile() || !fi.isExecutable()) {
        return {};
    }
    // raye-openwith-<name>.desktop in the user's applications folder; the
    // name keeps letters, digits, dots, dashes and underscores only.
    QString base = fi.completeBaseName();
    base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
    if (base.isEmpty() || base.startsWith(QLatin1Char('.'))) {
        base.prepend(QStringLiteral("app"));
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (!QDir().mkpath(dir)) {
        return {};
    }
    // The same name for another program gets a number, so an association
    // made before keeps its program.
    const QString exec = KShell::quoteArg(fi.absoluteFilePath()).replace(QLatin1Char('%'), QStringLiteral("%%")) + QStringLiteral(" %f");
    QString file = dir + QStringLiteral("/raye-openwith-%1.desktop").arg(base);
    for (int n = 2; QFile::exists(file) && KDesktopFile(file).desktopGroup().readEntry("Exec") != exec; ++n) {
        file = dir + QStringLiteral("/raye-openwith-%1-%2.desktop").arg(base).arg(n);
    }
    {
        KDesktopFile desktop(file);
        KConfigGroup g = desktop.desktopGroup();
        g.writeEntry("Type", "Application");
        g.writeEntry("Name", fi.completeBaseName());
        // Quoted for the desktop entry parser: spaces and shell characters
        // in the path stay part of the program name, and a % is doubled so
        // it is never read as a field code.
        g.writeEntry("Exec", exec);
        g.writeEntry("NoDisplay", true);
        if (!mime.isEmpty()) {
            g.writeXdgListEntry("MimeType", {mime});
        }
        desktop.sync();
    }
    KService::Ptr service(new KService(file));
    if (!service->isValid()) {
        return {};
    }
    // Known by its file name, like the apps the system lists, so "Always"
    // can name it in mimeapps.list before the app cache sees the file.
    service->setMenuId(QFileInfo(file).fileName());
    KSycoca::self()->ensureCacheValid();
    return service;
}

}
