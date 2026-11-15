/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "menuplan.h"

#include <KLocalizedString>

#include <QMimeDatabase>
#include <QMimeType>

#include <algorithm>

namespace Raye::Menu
{
namespace
{
// Adds a group of ids, with a separator before it when something came first.
void addGroup(QStringList &items, const QStringList &group)
{
    if (group.isEmpty()) {
        return;
    }
    if (!items.isEmpty() && items.last() != QLatin1String("-")) {
        items.append(QStringLiteral("-"));
    }
    items.append(group);
}

bool all(const QList<ItemKind> &items, std::initializer_list<ItemKind> kinds)
{
    return !items.isEmpty() && std::all_of(items.begin(), items.end(), [&](ItemKind k) {
        return std::find(kinds.begin(), kinds.end(), k) != kinds.end();
    });
}

bool any(const QList<ItemKind> &items, ItemKind kind)
{
    return items.contains(kind);
}

Plan backgroundPlan(const Selection &s)
{
    Plan p;
    if (s.place == Place::RecycleBin) {
        p.items = {QStringLiteral("view"), QStringLiteral("sort"), QStringLiteral("group"), QStringLiteral("refresh")};
        addGroup(p.items, {QStringLiteral("empty_bin")});
        addGroup(p.items, {QStringLiteral("properties")});
        return p;
    }
    if (s.clipboardHasFiles && s.writable && s.place == Place::Normal) {
        p.row = {QStringLiteral("paste")};
    }
    p.items = {QStringLiteral("view"), QStringLiteral("sort"), QStringLiteral("group"), QStringLiteral("refresh")};
    QStringList second{QStringLiteral("undo")};
    if (s.writable && s.place == Place::Normal) {
        second.append(QStringLiteral("new"));
    }
    addGroup(p.items, second);
    QStringList third;
    if (s.local && s.place == Place::Normal) {
        third.append(QStringLiteral("open_terminal"));
    }
    third.append(QStringLiteral("properties"));
    addGroup(p.items, third);
    addGroup(p.items, {QStringLiteral("more_options")});
    return p;
}

Plan recycleBinItemPlan()
{
    Plan p;
    p.row = {QStringLiteral("cut"), QStringLiteral("delete")};
    p.items = {QStringLiteral("restore")};
    addGroup(p.items, {QStringLiteral("properties")});
    return p;
}

Plan drivePlan(const Selection &s)
{
    Plan p;
    p.items = {QStringLiteral("open"), QStringLiteral("open_new_tab"), QStringLiteral("open_new_window")};
    if (s.items.size() == 1) {
        addGroup(p.items, {QStringLiteral("pin_quick_access")});
    }
    QStringList drive;
    if (s.driveRemovable) {
        drive.append(QStringLiteral("eject"));
    }
    drive.append(QStringLiteral("format"));
    addGroup(p.items, drive);
    addGroup(p.items, {QStringLiteral("properties")});
    addGroup(p.items, {QStringLiteral("more_options")});
    return p;
}
}

Plan plan(const Selection &s)
{
    if (s.background || s.items.isEmpty()) {
        return backgroundPlan(s);
    }
    if (s.place == Place::RecycleBin) {
        return recycleBinItemPlan();
    }
    if (all(s.items, {ItemKind::Drive})) {
        return drivePlan(s);
    }

    const bool single = s.items.size() == 1;
    const ItemKind first = s.items.first();
    const bool folders = all(s.items, {ItemKind::Folder});
    const bool files = !any(s.items, ItemKind::Folder) && !any(s.items, ItemKind::Drive);
    const bool search = s.place == Place::Search;

    Plan p;
    // The row: Cut, Copy, (Paste into a folder), Rename, Share, Delete.
    p.row = {QStringLiteral("cut"), QStringLiteral("copy")};
    if (single && first == ItemKind::Folder && s.clipboardHasFiles && s.writable) {
        p.row.append(QStringLiteral("paste"));
    }
    p.row.append({QStringLiteral("rename"), QStringLiteral("share"), QStringLiteral("delete")});

    // Open
    QStringList open{QStringLiteral("open")};
    if (files && !any(s.items, ItemKind::Executable)) {
        open.append(QStringLiteral("open_with"));
    }
    if (folders) {
        open.append({QStringLiteral("open_new_tab"), QStringLiteral("open_new_window")});
    }
    if (search && single) {
        open.append(QStringLiteral("open_file_location"));
    }
    addGroup(p.items, open);

    // Network drives
    if (single && first == ItemKind::Folder && s.networkShare) {
        addGroup(p.items, {QStringLiteral("map_drive")});
    } else if (single && first == ItemKind::Folder && s.networkDrive) {
        addGroup(p.items, {QStringLiteral("disconnect_drive")});
    }

    // Quick access and Favorites
    QStringList keep;
    if (single && first == ItemKind::Folder) {
        keep.append(QStringLiteral("pin_quick_access"));
    }
    if (files && s.local) {
        keep.append(QStringLiteral("favorite"));
    }
    addGroup(p.items, keep);

    // Archives
    QStringList archive;
    if (s.local && s.writable && !search) {
        archive.append(QStringLiteral("compress"));
    }
    if (s.local && single && first == ItemKind::Archive) {
        archive.append(QStringLiteral("extract_all"));
        if (s.writable && !search) {
            archive.append({QStringLiteral("extract_here"), QStringLiteral("extract_to")});
        }
    }
    addGroup(p.items, archive);

    // Pictures
    if (s.local && single && first == ItemKind::Picture) {
        addGroup(p.items, {QStringLiteral("set_background"), QStringLiteral("rotate_right"), QStringLiteral("rotate_left")});
    }

    // Paths, shortcuts, terminal
    QStringList path{QStringLiteral("copy_path")};
    if (s.local && s.writable && !search) {
        path.append(QStringLiteral("create_shortcut"));
    }
    if (s.local && single && first == ItemKind::Folder) {
        path.append(QStringLiteral("open_terminal"));
    }
    addGroup(p.items, path);

    addGroup(p.items, {QStringLiteral("properties")});
    addGroup(p.items, {QStringLiteral("more_options")});
    return p;
}

ItemKind kindOf(const QString &mimeType, bool isDir, const QString &fileName, bool executableBit)
{
    if (isDir) {
        return ItemKind::Folder;
    }
    const QString lower = fileName.toLower();
    if (lower.endsWith(QLatin1String(".exe")) || lower.endsWith(QLatin1String(".msi"))) {
        return ItemKind::Executable;
    }
    const QMimeType mime = QMimeDatabase().mimeTypeForName(mimeType);
    if (mimeType.startsWith(QLatin1String("image/"))) {
        return ItemKind::Picture;
    }
    for (const char *archive : {"application/zip", "application/x-7z-compressed", "application/x-tar", "application/x-compressed-tar",
                                "application/x-bzip-compressed-tar", "application/x-xz-compressed-tar", "application/x-zstd-compressed-tar",
                                "application/vnd.rar", "application/x-rar", "application/x-lzma-compressed-tar", "application/x-lzip-compressed-tar",
                                "application/x-tarz", "application/x-lz4-compressed-tar", "application/x-cd-image", "application/vnd.ms-cab-compressed",
                                "application/x-cpio", "application/gzip", "application/x-bzip2", "application/x-xz", "application/zstd",
                                "application/x-lzma", "application/x-lzip", "application/x-lz4", "application/x-compress", "application/x-arj",
                                "application/x-lha"}) {
        if (mime.isValid() && mime.inherits(QString::fromLatin1(archive))) {
            // Office files and the like are ZIP files inside, not folders.
            const QString name = mime.name();
            if (name.startsWith(QLatin1String("application/vnd.")) && name != QLatin1String("application/vnd.rar")) {
                break;
            }
            if (name == QLatin1String("application/epub+zip") || name.startsWith(QLatin1String("application/x-java"))) {
                break;
            }
            return ItemKind::Archive;
        }
    }
    if (executableBit && mime.isValid()
        && (mime.inherits(QStringLiteral("application/x-executable")) || mime.inherits(QStringLiteral("application/x-sharedlib"))
            || mime.inherits(QStringLiteral("application/x-shellscript")))) {
        return ItemKind::Executable;
    }
    return ItemKind::File;
}

QString copyAsPathText(const QList<QUrl> &urls)
{
    QStringList lines;
    lines.reserve(urls.size());
    for (const QUrl &url : urls) {
        const QString text = url.isLocalFile() ? url.toLocalFile() : url.toDisplayString(QUrl::RemovePassword);
        lines.append(QLatin1Char('"') + text + QLatin1Char('"'));
    }
    return lines.join(QLatin1Char('\n'));
}

QString shortcutName(const QString &fileName, bool isDir, const QStringList &taken)
{
    QString base = fileName;
    QString suffix;
    if (!isDir) {
        const QString known = QMimeDatabase().suffixForFileName(fileName);
        if (!known.isEmpty() && fileName.size() > known.size() + 1) {
            base = fileName.left(fileName.size() - known.size() - 1);
            suffix = QLatin1Char('.') + fileName.right(known.size());
        } else {
            const int dot = fileName.lastIndexOf(QLatin1Char('.'));
            if (dot > 0) {
                base = fileName.left(dot);
                suffix = fileName.mid(dot);
            }
        }
    }
    const QString stem = i18nc("@item the name of a new shortcut, %1 is the item's name", "%1 - Shortcut", base);
    QString name = stem + suffix;
    for (int n = 2; taken.contains(name); ++n) {
        name = stem + QStringLiteral(" (%1)").arg(n) + suffix;
    }
    return name;
}

}
