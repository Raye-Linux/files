/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "defaultsets.h"
#include "themeicon_p.h"
#include "commandbar.h"
#include "commandids.h"
#include "netdrive/mapdrivedialog.h"

#include <KConfig>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KIO/ApplicationLauncherJob>
#include <KIO/CopyJob>
#include <KIO/DeleteOrTrashJob>
#include <KJobWidgets>
#include <KIO/ListJob>
#include <KIO/RestoreJob>
#include <KLocalizedString>
#include <KService>
#include <KServiceAction>

#include <Solid/Device>
#include <Solid/OpticalDrive>
#include <Solid/StorageAccess>

#include <QAction>
#include <QDir>
#include <QIcon>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace Raye::Commands
{
namespace
{
enum class Turn {
    None,
    Left,
    Right,
};

Turn turnOf(const QString &actionName, const KConfigGroup &group)
{
    const QString mark = group.readEntry("X-Raye-Rotate", QString()).trimmed().toLower();
    if (mark == QLatin1String("left")) {
        return Turn::Left;
    }
    if (mark == QLatin1String("right")) {
        return Turn::Right;
    }
    // "counterclockwise" holds "clockwise": left is tried first.
    static const QRegularExpression left(QStringLiteral("rotate[-_ ]?(left|ccw|counterclockwise|anticlockwise)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression right(QStringLiteral("rotate[-_ ]?(right|cw|clockwise)"), QRegularExpression::CaseInsensitiveOption);
    if (left.match(actionName).hasMatch()) {
        return Turn::Left;
    }
    if (right.match(actionName).hasMatch()) {
        return Turn::Right;
    }
    return Turn::None;
}

bool coversPictures(const QStringList &mimeTypes)
{
    for (const QString &m : mimeTypes) {
        const QString t = m.trimmed();
        if (t.startsWith(QLatin1String("image/")) || t == QLatin1String("all/all") || t == QLatin1String("all/allfiles")) {
            return true;
        }
    }
    return false;
}

void runServiceAction(const ServiceAction &found, const QList<QUrl> &urls)
{
    const KService::Ptr service(new KService(found.file));
    const QList<KServiceAction> actions = service->actions();
    for (const KServiceAction &action : actions) {
        if (action.name() == found.action) {
            auto *job = new KIO::ApplicationLauncherJob(action);
            job->setUrls(urls);
            job->start();
            return;
        }
    }
}

bool onePictureHere(const CommandContext &c)
{
    return c.selection.size() == 1 && c.selection.first().isLocalFile() && !c.selectionMimeTypes.isEmpty()
        && c.selectionMimeTypes.first().startsWith(QLatin1String("image/"));
}

void ejectDrive(const QString &udi)
{
    Solid::Device device(udi);
    if (!device.isValid()) {
        return;
    }
    // A disc: open the tray. Anything else: unmount, so it can be pulled out.
    for (Solid::Device d = device; d.isValid(); d = d.parent()) {
        if (auto *optical = d.as<Solid::OpticalDrive>()) {
            optical->eject();
            return;
        }
    }
    if (auto *access = device.as<Solid::StorageAccess>()) {
        if (access->isAccessible()) {
            access->teardown();
        }
    }
}
}

QStringList serviceMenuDirs()
{
    return QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("kio/servicemenus"), QStandardPaths::LocateDirectory);
}

RotateActions findRotateActions(const QStringList &dirs)
{
    RotateActions found;
    for (const QString &dirPath : dirs) {
        const QDir dir(dirPath);
        const QStringList files = dir.entryList({QStringLiteral("*.desktop")}, QDir::Files, QDir::Name);
        for (const QString &name : files) {
            const QString path = dir.filePath(name);
            // The rule Files applies to every service menu: system ones, or
            // the user's own when marked executable.
            if (!KDesktopFile::isAuthorizedDesktopFile(path)) {
                continue;
            }
            KConfig file(path, KConfig::SimpleConfig);
            const KConfigGroup entry = file.group(QStringLiteral("Desktop Entry"));
            if (!coversPictures(entry.readXdgListEntry("MimeType"))) {
                continue;
            }
            const QStringList actions = entry.readXdgListEntry("Actions");
            for (const QString &action : actions) {
                const Turn turn = turnOf(action, file.group(QStringLiteral("Desktop Action ") + action));
                ServiceAction *slot = turn == Turn::Left ? &found.left : turn == Turn::Right ? &found.right : nullptr;
                if (slot && !slot->isValid()) {
                    *slot = {path, action};
                }
            }
            if (found.left.isValid() && found.right.isValid()) {
                return found;
            }
        }
    }
    return found;
}

namespace
{
bool inRecycleBin(const CommandContext &c)
{
    return c.url.scheme() == QLatin1String("trash");
}

bool onNetworkPage(const CommandContext &c)
{
    return c.url == QUrl(QStringLiteral("raye:thispc")) || c.url.scheme() == QLatin1String("smb");
}

QUrl shareToMap(const CommandContext &c)
{
    // A share picked in the network view, or the share being browsed.
    for (const QUrl &u : c.selection) {
        if (u.scheme() == QLatin1String("smb") && !u.host().isEmpty()) {
            return u;
        }
    }
    return c.url.scheme() == QLatin1String("smb") && !c.url.host().isEmpty() ? c.url : QUrl();
}

void restoreAll(QWidget *window)
{
    auto *job = KIO::listDir(QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    auto *urls = new QList<QUrl>();
    QObject::connect(job, &KIO::ListJob::entries, job, [urls](KIO::Job *, const KIO::UDSEntryList &entries) {
        for (const KIO::UDSEntry &e : entries) {
            const QString name = e.stringValue(KIO::UDSEntry::UDS_NAME);
            if (name != QLatin1String(".") && name != QLatin1String("..")) {
                urls->append(QUrl(QStringLiteral("trash:/") + name));
            }
        }
    });
    QObject::connect(job, &KJob::result, job, [urls, window](KJob *) {
        if (!urls->isEmpty()) {
            auto *restore = KIO::restoreFromTrash(*urls);
            KJobWidgets::setWindow(restore, window);
            restore->start();
        }
        delete urls;
    });
}

// The Recycle Bin (Empty, Restore all, Restore the selected items) and
// the network places (Map network drive), as File Explorer has them.
void addPlaceSets(CommandBar *bar)
{
    auto *empty = new QAction(themeIcon(QStringLiteral("trash-empty")), i18nc("@action:button", "Empty Recycle Bin"), bar);
    empty->setObjectName(QStringLiteral("raye_empty_bin"));
    QObject::connect(empty, &QAction::triggered, bar, [bar]() {
        using Ask = KIO::AskUserActionInterface;
        auto *job = new KIO::DeleteOrTrashJob(QList<QUrl>{}, Ask::EmptyTrash, Ask::DefaultConfirmation, bar->window());
        job->start();
    });
    auto *all = new QAction(themeIcon(QStringLiteral("edit-reset")), i18nc("@action:button", "Restore all items"), bar);
    all->setObjectName(QStringLiteral("raye_restore_all"));
    QObject::connect(all, &QAction::triggered, bar, [bar]() {
        restoreAll(bar->window());
    });
    bar->registerSet(QStringLiteral("raye-recycle-bin"), {empty, all}, inRecycleBin);

    auto *selected = new QAction(themeIcon(QStringLiteral("edit-undo")), i18nc("@action:button", "Restore the selected items"), bar);
    selected->setObjectName(QStringLiteral("raye_restore_selected"));
    QObject::connect(selected, &QAction::triggered, bar, [bar]() {
        const QList<QUrl> urls = bar->context().selection;
        if (!urls.isEmpty()) {
            auto *restore = KIO::restoreFromTrash(urls);
            KJobWidgets::setWindow(restore, bar->window());
            restore->start();
        }
    });
    bar->registerSet(QStringLiteral("raye-recycle-bin-selection"), {selected}, [](const CommandContext &c) {
        return inRecycleBin(c) && !c.selection.isEmpty();
    });

    auto *map = new QAction(themeIcon(QStringLiteral("folder-network")), i18nc("@action:button", "Map network drive"), bar);
    map->setObjectName(QStringLiteral("raye_map_network_drive"));
    QObject::connect(map, &QAction::triggered, bar, [bar]() {
        QWidget *window = bar->window();
        QPointer<QWidget> guard(window);
        Raye::NetDrives::showMapDriveDialog(shareToMap(bar->context()), window, [guard](const QUrl &drive) {
            if (guard) {
                QMetaObject::invokeMethod(guard, "changeUrl", Q_ARG(QUrl, drive));
            }
        });
    });
    bar->registerSet(QStringLiteral("raye-network"), {map}, onNetworkPage);

    auto *settings = new QAction(themeIcon(QStringLiteral("configure")), i18nc("@action:button open the Settings app", "Open Settings"), bar);
    settings->setObjectName(QStringLiteral("raye_open_settings"));
    QObject::connect(settings, &QAction::triggered, bar, []() {
        QProcess::startDetached(QStringLiteral("raye-settings"), {QStringLiteral("--page"), QStringLiteral("system/storage")});
    });
    bar->registerSet(QStringLiteral("raye-this-pc"), {settings}, [](const CommandContext &c) {
        return c.url == QUrl(QStringLiteral("raye:thispc"));
    });
}
}

void addDefaultSets(CommandBar *bar)
{
    // Pictures
    auto *background = new QAction(themeIcon(QStringLiteral("preferences-desktop-wallpaper")),
                                   i18nc("@action:button use the selected picture as the desktop background", "Set as background"),
                                   bar);
    background->setObjectName(QString::fromLatin1(Ids::setAsBackground));
    QObject::connect(background, &QAction::triggered, bar, [bar]() {
        const CommandContext &c = bar->context();
        if (onePictureHere(c)) {
            // An absolute path: it can never be read as an option.
            QProcess::startDetached(QStringLiteral("plasma-apply-wallpaperimage"), {c.selection.first().toLocalFile()});
        }
    });
    QList<QAction *> pictureActions{background};

    const RotateActions rotate = findRotateActions(serviceMenuDirs());
    if (rotate.left.isValid()) {
        auto *left = new QAction(themeIcon(QStringLiteral("object-rotate-left")), i18nc("@action:button", "Rotate left"), bar);
        left->setObjectName(QString::fromLatin1(Ids::rotateLeft));
        QObject::connect(left, &QAction::triggered, bar, [bar, action = rotate.left]() {
            runServiceAction(action, bar->context().selection);
        });
        pictureActions << left;
    }
    if (rotate.right.isValid()) {
        auto *right = new QAction(themeIcon(QStringLiteral("object-rotate-right")), i18nc("@action:button", "Rotate right"), bar);
        right->setObjectName(QString::fromLatin1(Ids::rotateRight));
        QObject::connect(right, &QAction::triggered, bar, [bar, action = rotate.right]() {
            runServiceAction(action, bar->context().selection);
        });
        pictureActions << right;
    }
    bar->registerSet(QStringLiteral("raye-pictures"), pictureActions, onePictureHere);

    // Drives
    auto *eject = new QAction(themeIcon(QStringLiteral("media-eject")), i18nc("@action:button eject a drive or disc", "Eject"), bar);
    eject->setObjectName(QString::fromLatin1(Ids::eject));
    QObject::connect(eject, &QAction::triggered, bar, [bar]() {
        if (!bar->context().driveUdi.isEmpty()) {
            ejectDrive(bar->context().driveUdi);
        }
    });
    bar->registerSet(QStringLiteral("raye-drive-eject"), {eject}, [](const CommandContext &c) {
        return !c.driveUdi.isEmpty() && c.driveRemovable;
    });

    addPlaceSets(bar);

    const KService::Ptr diskManager = KService::serviceByDesktopName(QStringLiteral("org.kde.partitionmanager"));
    if (diskManager) {
        auto *format = new QAction(themeIcon(QStringLiteral("drive-harddisk")), i18nc("@action:button erase a drive and prepare it", "Format"), bar);
        format->setObjectName(QString::fromLatin1(Ids::format));
        QObject::connect(format, &QAction::triggered, bar, [diskManager]() {
            auto *job = new KIO::ApplicationLauncherJob(diskManager);
            job->start();
        });
        bar->registerSet(QStringLiteral("raye-drive-format"), {format}, [](const CommandContext &c) {
            return !c.driveUdi.isEmpty();
        });
    }
}

}
