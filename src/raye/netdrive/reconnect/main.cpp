/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// raye-files-netdrives: connects again, at sign-in, the network drives set
// to "Reconnect at sign-in". Started by the session (autostart).

#include "netdrive/networkdrives.h"

#include <KLocalizedString>
#include <KNotification>

#include <QApplication>
#include <QDir>
#include <QTextStream>
#include <QTimer>

using namespace Raye::NetDrives;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    KLocalizedString::setApplicationDomain("raye-files");
    app.setApplicationName(QStringLiteral("raye-files-netdrives"));
    app.setDesktopFileName(QStringLiteral("org.kde.dolphin"));

    const QList<Mount> mounted = networkMounts();
    QStringList failed;
    for (const Mapping &m : loadMappings()) {
        if (!m.reconnect || !mountAt(m.mountPoint, mounted).type.isEmpty()) {
            continue;
        }
        QString password;
        if (!m.user.isEmpty() && (!m.remember || !readPassword(m, &password))) {
            failed << m.name;
            continue;
        }
        QDir().mkpath(m.mountPoint);
        QString error;
        const int code = mountNow(m, password, &error);
        password.fill(QLatin1Char(' '));
        if (code != 0) {
            QTextStream(stderr) << m.name << ": " << failureText(failureOf(code, error), m.address) << '\n';
            failed << m.name;
        }
    }
    if (failed.isEmpty()) {
        return 0;
    }
    KNotification *n = KNotification::event(KNotification::Notification,
                                             i18nc("@title notification", "Couldn't reconnect all network drives"),
                                             i18ncp("@info notification",
                                                    "%2 isn't connected. Open Files to connect it again.",
                                                    "%2 aren't connected. Open Files to connect them again.",
                                                    failed.size(),
                                                    failed.join(QStringLiteral(", "))),
                                             QStringLiteral("network-disconnect"),
                                             KNotification::Persistent);
    QObject::connect(n, &KNotification::closed, &app, &QCoreApplication::quit);
    QTimer::singleShot(20000, &app, &QCoreApplication::quit);
    app.exec();
    return 1;
}
