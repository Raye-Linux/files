/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "firstrun.h"

#include <KConfig>
#include <KConfigGroup>
#include <KFilePlacesModel>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

namespace Raye::FirstRun
{
namespace
{
const QString rayeGroup = QStringLiteral("Raye");

bool sameUrl(const QUrl &a, const QUrl &b)
{
    return a.matches(b, QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
}

int rowOf(const KFilePlacesModel &model, const QUrl &url, KFilePlacesModel::GroupType group)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        if (model.groupType(index) == group && sameUrl(model.url(index), url)) {
            return row;
        }
    }
    return -1;
}

int firstRowOf(const KFilePlacesModel &model, KFilePlacesModel::GroupType group)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.groupType(model.index(row, 0)) == group) {
            return row;
        }
    }
    return -1;
}

// Quick access in Windows 11, top to bottom.
QList<QUrl> windowsFolders()
{
    static const QStandardPaths::StandardLocation order[] = {
        QStandardPaths::DesktopLocation,
        QStandardPaths::DownloadLocation,
        QStandardPaths::DocumentsLocation,
        QStandardPaths::PicturesLocation,
        QStandardPaths::MusicLocation,
        QStandardPaths::MoviesLocation,
    };
    const QUrl home = QUrl::fromLocalFile(QDir::homePath());
    QList<QUrl> urls;
    for (const auto location : order) {
        const QUrl url = QUrl::fromLocalFile(QStandardPaths::writableLocation(location));
        // A user folder turned off in user-dirs.dirs points at the home.
        if (!sameUrl(url, home)) {
            urls << url;
        }
    }
    return urls;
}

bool isPlainName(const QString &name)
{
    return !name.isEmpty() && name != QLatin1String(".") && name != QLatin1String("..") && !name.contains(QLatin1Char('/'))
        && !name.contains(QLatin1Char('\n')) && !name.contains(QLatin1Char('\r')) && !name.contains(QChar(0));
}
}

bool migratePlaces(KFilePlacesModel *model)
{
    if (!model) {
        return false;
    }
    bool changed = false;

    // Whole groups Windows has no equivalent for in Quick access.
    for (const auto group : {KFilePlacesModel::RemoteType, KFilePlacesModel::RecentlySavedType, KFilePlacesModel::SearchForType}) {
        if (!model->isGroupHidden(group)) {
            model->setGroupHidden(group, true);
            changed = true;
        }
    }

    // Home, the Recycle Bin and the root folder: Files shows Home and the
    // Recycle Bin as its own fixed entries, and Windows never shows "/".
    const QList<QUrl> hide = {
        QUrl::fromLocalFile(QDir::homePath()),
        QUrl(QStringLiteral("trash:/")),
        QUrl(QStringLiteral("file:///")),
    };
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (model->isHidden(index)) {
            continue;
        }
        const QUrl url = model->url(index);
        for (const QUrl &unwanted : hide) {
            if (sameUrl(url, unwanted)) {
                model->setPlaceHidden(index, true);
                changed = true;
                break;
            }
        }
    }

    // The six folders first, in Windows' order. Rows are looked up again
    // after every move: each one shifts the rows below it.
    int target = firstRowOf(*model, KFilePlacesModel::PlacesType);
    if (target < 0) {
        return changed;
    }
    bool moved = false;
    for (const QUrl &url : windowsFolders()) {
        const int row = rowOf(*model, url, KFilePlacesModel::PlacesType);
        if (row < 0) {
            continue; // removed by the user: not brought back
        }
        if (row != target) {
            if (!model->movePlace(row, target)) {
                qWarning("Files: could not move %s in the places list", qPrintable(url.toDisplayString()));
                break;
            }
            moved = true;
        }
        ++target;
    }
    if (moved) {
        // movePlace() only reorders in memory; refresh() writes the file.
        model->refresh();
    }
    return changed || moved;
}

bool applyTrashDefaults(KConfig &ktrashrc, const QString &homeTrashPath)
{
    if (homeTrashPath.isEmpty()) {
        return false;
    }
    KConfigGroup group = ktrashrc.group(homeTrashPath);
    if (!group.keyList().isEmpty()) {
        return false; // set by the user (or by the Recycle Bin properties)
    }
    group.writeEntry("UseSizeLimit", true);
    group.writeEntry("Percent", 5.0);
    // 1 = delete the oldest items. KIO's default (0) refuses the delete and
    // leaves the file where it was, which Windows never does.
    group.writeEntry("LimitReachedAction", 1);
    ktrashrc.sync();
    return true;
}

QStringList hiddenUserDirs(const QString &home, const QString &userDirsText)
{
    const QString cleanHome = QDir::cleanPath(home);
    QStringList names;
    if (cleanHome.isEmpty()) {
        return names;
    }
    const QStringList lines = userDirsText.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0) {
            continue;
        }
        const QString key = line.left(equals).trimmed();
        if (key != QLatin1String("XDG_TEMPLATES_DIR") && key != QLatin1String("XDG_PUBLICSHARE_DIR")) {
            continue;
        }
        QString value = line.mid(equals + 1).trimmed();
        if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) {
            value = value.mid(1, value.size() - 2);
        }
        if (value.startsWith(QLatin1String("$HOME"))) {
            value = cleanHome + value.mid(5);
        }
        if (!value.startsWith(QLatin1Char('/'))) {
            continue; // the format only allows $HOME/... or an absolute path
        }
        const QFileInfo info(QDir::cleanPath(value));
        if (QDir::cleanPath(info.path()) != cleanHome) {
            continue; // the home itself, a deeper folder, or elsewhere
        }
        const QString name = info.fileName();
        if (isPlainName(name) && info.isDir() && !names.contains(name)) {
            names << name;
        }
    }
    return names;
}

bool addToHiddenFile(const QString &hiddenFile, const QStringList &names)
{
    QByteArray content;
    {
        QFile file(hiddenFile);
        if (file.exists() && !file.open(QIODevice::ReadOnly)) {
            return false;
        }
        if (file.isOpen()) {
            content = file.readAll();
        }
    }

    QStringList present;
    for (const QByteArray &line : content.split('\n')) {
        present << QString::fromUtf8(line).trimmed();
    }
    QStringList missing;
    for (const QString &name : names) {
        if (isPlainName(name) && !present.contains(name) && !missing.contains(name)) {
            missing << name;
        }
    }
    if (missing.isEmpty()) {
        return false;
    }

    if (!content.isEmpty() && !content.endsWith('\n')) {
        content += '\n';
    }
    for (const QString &name : std::as_const(missing)) {
        content += name.toUtf8() + '\n';
    }

    QDir().mkpath(QFileInfo(hiddenFile).absolutePath());
    QSaveFile out(hiddenFile);
    if (!out.open(QIODevice::WriteOnly) || out.write(content) != content.size() || !out.commit()) {
        qWarning("Files: could not write %s", qPrintable(hiddenFile));
        return false;
    }
    return true;
}

void runOnce(const KSharedConfig::Ptr &dolphinrc)
{
    if (!dolphinrc) {
        return;
    }
    KConfigGroup raye(dolphinrc, rayeGroup);
    bool dirty = false;

    if (!raye.readEntry("PlacesMigrated", false)) {
        KFilePlacesModel model;
        migratePlaces(&model);
        raye.writeEntry("PlacesMigrated", true);
        dirty = true;
    }

    if (raye.readEntry("DefaultsVersion", 0) < 1) {
        KConfig ktrashrc(QStringLiteral("ktrashrc"), KConfig::SimpleConfig);
        applyTrashDefaults(ktrashrc, QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash"));

        QFile userDirs(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/user-dirs.dirs"));
        if (userDirs.open(QIODevice::ReadOnly)) {
            const QString home = QDir::homePath();
            const QStringList names = hiddenUserDirs(home, QString::fromUtf8(userDirs.readAll()));
            if (!names.isEmpty()) {
                addToHiddenFile(home + QStringLiteral("/.hidden"), names);
            }
        }
    }
    if (raye.readEntry("DefaultsVersion", 0) < DefaultsVersion) {
        raye.writeEntry("DefaultsVersion", DefaultsVersion);
        dirty = true;
    }

    if (dirty) {
        dolphinrc->sync();
    }
}

void runOnce()
{
    runOnce(KSharedConfig::openConfig(QStringLiteral("dolphinrc")));
}

}
