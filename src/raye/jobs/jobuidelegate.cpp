/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "jobuidelegate.h"

#include "askuser.h"
#include "dialogs.h"
#include "jobtracker.h"

#include <KIO/JobTracker>

#include <QCoreApplication>
#include <QList>
#include <QTimer>

namespace Raye::Jobs
{

namespace
{
std::shared_ptr<Prompter> &sharedPrompter()
{
    static std::shared_ptr<Prompter> p;
    return p;
}

QList<std::function<QObject *()>> &makers()
{
    static QList<std::function<QObject *()>> list;
    return list;
}

bool s_installed = false;

QList<QObject *> interfacesFor(std::shared_ptr<Prompter> p)
{
    QList<QObject *> ifaces;
    ifaces.append(new AskUser(p ? std::move(p) : prompter()));
    for (const auto &make : std::as_const(makers())) {
        if (QObject *o = make()) {
            ifaces.append(o);
        }
    }
    return ifaces;
}
}

JobUiDelegate::JobUiDelegate(KJobUiDelegate::Flags flags, QWidget *window, std::shared_ptr<Prompter> p)
    : KIO::JobUiDelegate(flags, window, interfacesFor(std::move(p)))
{
}

KJobUiDelegate *JobUiDelegateFactory::createDelegate() const
{
    return new JobUiDelegate();
}

KJobUiDelegate *JobUiDelegateFactory::createDelegate(KJobUiDelegate::Flags flags, QWidget *window) const
{
    return new JobUiDelegate(flags, window);
}

std::shared_ptr<Prompter> prompter()
{
    if (!sharedPrompter()) {
        sharedPrompter() = std::make_shared<DialogPrompter>();
    }
    return sharedPrompter();
}

void setPrompter(std::shared_ptr<Prompter> p)
{
    sharedPrompter() = std::move(p);
}

void addDelegateInterface(const std::function<QObject *()> &maker)
{
    makers().append(maker);
}

void install()
{
    if (s_installed) {
        return;
    }
    s_installed = true;
    static JobUiDelegateFactory factory;
    KIO::setDefaultJobUiDelegateFactory(&factory);
    KIO::setJobTracker(JobTracker::instance());
}

bool installed()
{
    return s_installed;
}

}

// Files' own process takes the Raye dialogs; the KPart inside another
// application and the tests do not. After the first turn of the event
// loop, so KIO's widget library has registered its factory before ours
// replaces it.
static void rayeJobsInstallAtStartup()
{
    if (QCoreApplication::applicationName() != QLatin1String("dolphin") || qEnvironmentVariableIntValue("RAYE_FILES_KDE_DIALOGS") == 1) {
        return;
    }
    QTimer::singleShot(0, qApp, [] {
        Raye::Jobs::install();
    });
}
Q_COREAPP_STARTUP_FUNCTION(rayeJobsInstallAtStartup)
