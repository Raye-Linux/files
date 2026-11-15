/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "menuactions.h"

#include "compactmenu.h"
#include "menuplan.h"

#include "commandbar/defaultsets.h"
#include "share/shareflyout.h"

#include <KApplicationTrader>
#include <KIO/ApplicationLauncherJob>
#include <KLocalizedString>
#include <KIO/CopyJob>
#include <KIO/SimpleJob>
#include <KJobWidgets>
#include <KProtocolInfo>
#include <KService>
#include <KServiceAction>

#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMenu>
#include <QMimeData>
#include <QProcess>
#include <QStandardPaths>

namespace Raye::Menu
{

void copyAsPath(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(copyAsPathText(urls));
}

QList<KJob *> createShortcuts(const QList<QUrl> &urls, QWidget *window)
{
    QList<KJob *> jobs;
    QHash<QString, QStringList> taken; // per folder, the names used so far
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QFileInfo info(url.toLocalFile());
        const QString dir = info.absolutePath();
        if (!taken.contains(dir)) {
            taken.insert(dir, QDir(dir).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot));
        }
        const QString name = shortcutName(info.fileName(), info.isDir(), taken.value(dir));
        taken[dir].append(name);
        KIO::SimpleJob *job = KIO::symlink(info.absoluteFilePath(), QUrl::fromLocalFile(dir + QLatin1Char('/') + name), KIO::HideProgressInfo);
        KJobWidgets::setWindow(job, window);
        jobs.append(job);
    }
    return jobs;
}

bool setAsDesktopBackground(const QUrl &picture)
{
    if (!picture.isLocalFile()) {
        return false;
    }
    const QString tool = QStandardPaths::findExecutable(QStringLiteral("plasma-apply-wallpaperimage"));
    if (tool.isEmpty()) {
        return false;
    }
    return QProcess::startDetached(tool, {picture.toLocalFile()});
}

bool canRotate(bool right)
{
    const Commands::RotateActions found = Commands::findRotateActions(Commands::serviceMenuDirs());
    return right ? found.right.isValid() : found.left.isValid();
}

bool rotate(const QUrl &picture, bool right)
{
    const Commands::RotateActions found = Commands::findRotateActions(Commands::serviceMenuDirs());
    const Commands::ServiceAction &which = right ? found.right : found.left;
    if (!which.isValid() || !picture.isLocalFile()) {
        return false;
    }
    const KService::Ptr service(new KService(which.file));
    const QList<KServiceAction> actions = service->actions();
    for (const KServiceAction &action : actions) {
        if (action.name() == which.action) {
            auto *job = new KIO::ApplicationLauncherJob(action);
            job->setUrls({picture});
            job->start();
            return true;
        }
    }
    return false;
}

void shareAt(const QPoint &globalPos, const QList<QUrl> &urls, const QStringList &mimeTypes)
{
    if (urls.isEmpty()) {
        return;
    }
    Share::ShareFlyout *flyout = Share::ShareFlyout::share(nullptr, urls, mimeTypes);
    flyout->showAt(QRect(globalPos, QSize(1, 1)), Qt::BottomEdge, Qt::AlignLeft);
}

bool adminAvailable()
{
    return KProtocolInfo::isKnownProtocol(QStringLiteral("admin"));
}

QUrl adminUrl(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return QUrl();
    }
    QUrl admin;
    admin.setScheme(QStringLiteral("admin"));
    admin.setPath(url.toLocalFile());
    return admin;
}

namespace
{
// The actions of a window action's own menu (a KSelectAction or a
// KActionMenu), as they are.
QList<QAction *> menuActionsOf(QAction *source)
{
    if (!source || !source->menu()) {
        return {};
    }
    return source->menu()->actions();
}
}

QMenu *viewMenu(QMenu *parent, const ActionLookup &lookup)
{
    const QList<QAction *> modes = menuActionsOf(lookup ? lookup(QStringLiteral("view_mode")) : nullptr);
    if (modes.isEmpty()) {
        return nullptr;
    }
    auto *menu = new QMenu(parent);
    menu->addActions(modes);
    return menu;
}

QMenu *sortMenu(QMenu *parent, const ActionLookup &lookup)
{
    const QList<QAction *> roles = menuActionsOf(lookup ? lookup(QStringLiteral("sort")) : nullptr);
    if (roles.isEmpty()) {
        return nullptr;
    }
    auto *menu = new QMenu(parent);
    menu->addActions(roles);
    return menu;
}

QMenu *groupMenu(QMenu *parent, const ActionLookup &lookup)
{
    if (!lookup) {
        return nullptr;
    }
    QAction *inGroups = lookup(QStringLiteral("show_in_groups"));
    if (!inGroups) {
        return nullptr;
    }
    auto *menu = new QMenu(parent);
    auto *group = new QActionGroup(menu);
    QAction *none = menu->addAction(i18nc("@action:inmenu group by", "(None)"));
    none->setObjectName(QStringLiteral("group_none"));
    none->setCheckable(true);
    none->setActionGroup(group);
    none->setChecked(!inGroups->isChecked());
    QObject::connect(none, &QAction::triggered, inGroups, [inGroups] {
        if (inGroups->isChecked()) {
            inGroups->trigger();
        }
    });
    menu->addSeparator();
    const QList<std::pair<QString, QString>> roles = {
        {QStringLiteral("sort_by_text"), i18nc("@action:inmenu group by", "Name")},
        {QStringLiteral("sort_by_modificationtime"), i18nc("@action:inmenu group by", "Date modified")},
        {QStringLiteral("sort_by_type"), i18nc("@action:inmenu group by", "Type")},
        {QStringLiteral("sort_by_size"), i18nc("@action:inmenu group by", "Size")},
    };
    for (const auto &[name, text] : roles) {
        QAction *sortBy = lookup(name);
        if (!sortBy) {
            continue;
        }
        QAction *a = menu->addAction(text);
        a->setObjectName(QStringLiteral("group_") + name.mid(8));
        a->setCheckable(true);
        a->setActionGroup(group);
        a->setChecked(inGroups->isChecked() && sortBy->isChecked());
        // Files groups by the column it sorts by, as Dolphin does.
        QObject::connect(a, &QAction::triggered, sortBy, [sortBy, inGroups] {
            if (!sortBy->isChecked()) {
                sortBy->trigger();
            }
            if (!inGroups->isChecked()) {
                inGroups->trigger();
            }
        });
    }
    return menu;
}

QMenu *openWithMenu(QMenu *parent, const QList<QUrl> &urls, const QString &mimeType, const std::function<void()> &chooseOther)
{
    auto *menu = new QMenu(parent);
    if (!mimeType.isEmpty()) {
        const KService::List apps = KApplicationTrader::queryByMimeType(mimeType);
        int shown = 0;
        for (const KService::Ptr &app : apps) {
            if (shown >= openWithApps) {
                break;
            }
            // Files is not an app to open a file with, and the app that asks
            // which app to use is this very list.
            if (app->desktopEntryName() == QLatin1String("org.kde.dolphin") || app->desktopEntryName() == QLatin1String("org.raye.files.openwith")) {
                continue;
            }
            QAction *a = menu->addAction(QIcon::fromTheme(app->icon()), app->name());
            a->setObjectName(QStringLiteral("open_with_app"));
            a->setData(app->storageId());
            QObject::connect(a, &QAction::triggered, menu, [app, urls] {
                auto *job = new KIO::ApplicationLauncherJob(app);
                job->setUrls(urls);
                job->start();
            });
            ++shown;
        }
        if (shown > 0) {
            menu->addSeparator();
        }
    }
    QAction *other = menu->addAction(QIcon::fromTheme(CompactMenu::iconName(QStringLiteral("open_with_choose"))),
                                     CompactMenu::defaultText(QStringLiteral("open_with_choose")));
    other->setObjectName(QStringLiteral("open_with_choose"));
    QObject::connect(other, &QAction::triggered, menu, [chooseOther] {
        if (chooseOther) {
            chooseOther();
        }
    });
    return menu;
}

QStringList excludedPlugins()
{
    return {QStringLiteral("compressfileitemaction"), QStringLiteral("extractfileitemaction"), QStringLiteral("kio-admin"), QStringLiteral("kio_admin")};
}

}
