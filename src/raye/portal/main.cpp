/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// raye-files-portal: the Open, Save and "Open with" dialogs of every app.
//
// Started on demand over D-Bus by xdg-desktop-portal. It leaves on its own a
// little after its last dialog closes, so it holds no memory between uses.
//
// With --open-with it is the app the system starts for a file nothing can
// open: it asks which app to use and starts it.

#include "openwith/openwithdialog.h"
#include "portalservice.h"
#include "ui/carddialog.h"

#include <KAboutData>
#include <KIO/ApplicationLauncherJob>
#include <KIO/JobUiDelegateFactory>
#include <KLocalizedString>
#include <KService>

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QIcon>
#include <QMimeDatabase>
#include <QTimer>
#include <QUrl>

namespace
{
constexpr auto serviceName = "org.freedesktop.impl.portal.desktop.rayefiles";
constexpr auto objectPath = "/org/freedesktop/portal/desktop";
constexpr int idleExitMs = 60 * 1000;

int chooseAndOpen(const QList<QUrl> &urls)
{
    const QUrl first = urls.first();
    const QString mime = first.isLocalFile() ? QMimeDatabase().mimeTypeForFile(first.toLocalFile()).name() : QMimeDatabase().mimeTypeForUrl(first).name();
    auto *dialog = Raye::OpenWith::openWithDialog(nullptr, urls, mime, [urls](const KService::Ptr &service, bool) {
        if (!service) {
            QCoreApplication::quit();
            return;
        }
        auto *job = new KIO::ApplicationLauncherJob(service);
        job->setUrls(urls);
        // If the app cannot start, the person is told.
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
        QObject::connect(job, &KJob::result, qApp, [] {
            QCoreApplication::quit();
        });
        job->start();
    });
    dialog->show();
    return QApplication::exec();
}

int serve()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.interface()->registerService(QLatin1String(serviceName), QDBusConnectionInterface::DontQueueService)) {
        qWarning("another copy of the file dialogs is running");
        return 1;
    }
    QObject root;
    Raye::Portal::addPortalInterfaces(&root);
    if (!bus.registerObject(QLatin1String(objectPath), &root, QDBusConnection::ExportAdaptors)) {
        qWarning("could not export the portal object");
        return 1;
    }
    QTimer idle;
    idle.setInterval(idleExitMs);
    QObject::connect(&idle, &QTimer::timeout, qApp, [] {
        if (Raye::Portal::openDialogs() == 0) {
            QCoreApplication::quit();
        }
    });
    idle.start();
    return QApplication::exec();
}
}

int main(int argc, char *argv[])
{
    // This process is what the others call for their file dialogs: it must
    // never send its own back to the portal.
    qputenv("PLASMA_INTEGRATION_USE_PORTAL", "0");
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    KLocalizedString::setApplicationDomain("raye-files");
    KAboutData about(QStringLiteral("raye-files-portal"), i18nc("@title the part of the system that shows Open and Save dialogs", "File dialogs"), QStringLiteral(RAYE_FILES_VERSION));
    about.setDesktopFileName(QStringLiteral("org.raye.files.dialogs"));
    KAboutData::setApplicationData(about);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("system-file-manager")));

    QCommandLineParser parser;
    const QCommandLineOption openWith(QStringLiteral("open-with"), i18nc("@info:shell", "Ask which app opens these files, then open them."));
    parser.addOption(openWith);
    parser.addPositionalArgument(QStringLiteral("urls"), i18nc("@info:shell", "Files to open."), QStringLiteral("[urls...]"));
    about.setupCommandLine(&parser);
    parser.process(app);

    if (parser.isSet(openWith)) {
        QList<QUrl> urls;
        const auto arguments = parser.positionalArguments();
        for (const QString &argument : arguments) {
            urls.append(QUrl::fromUserInput(argument, QDir::currentPath(), QUrl::AssumeLocalFile));
        }
        if (urls.isEmpty()) {
            return 1;
        }
        return chooseAndOpen(urls);
    }
    return serve();
}
