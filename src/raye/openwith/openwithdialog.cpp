/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "openwithdialog.h"

#include "appchoices.h"
#include "jobs/jobuidelegate.h"
#include "jobs/quickhost.h"
#include "portal/filedialog.h"
#include "ui/carddialog.h"
#include "ui/commandbutton.h"
#include "ui/showcase.h"

#include <KJob>
#include <KJobWidgets>
#include <KLocalizedString>

#include <QCoreApplication>
#include <QLayout>
#include <QMimeDatabase>
#include <QPointer>
#include <QProcess>
#include <QQuickItem>
#include <QQuickWidget>
#include <QStandardPaths>
#include <QTimer>

#include <memory>

namespace Raye::OpenWith
{

using Raye::Ui::CardDialog;

namespace
{
constexpr int bodyWidth = 472;

// Relays the page's signals (connected by name) to the dialog.
class PageSignals : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    std::function<void()> browse;
    std::function<void()> store;
    std::function<void()> activated;
public Q_SLOTS:
    void onBrowse()
    {
        if (browse) {
            browse();
        }
    }
    void onStore()
    {
        if (store) {
            store();
        }
    }
    void onActivated()
    {
        if (activated) {
            activated();
        }
    }
};

// A Qt Quick layout works out its size when it is next polished, normally
// just before the first frame. Do it now, children first, so the page already
// has its real height while the dialog is being built.
void settleNow(QQuickItem *item)
{
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        settleNow(child);
    }
    item->ensurePolished();
}

QString titleFor(const QString &mime, const QList<QUrl> &urls)
{
    QString suffix;
    if (!urls.isEmpty()) {
        suffix = QMimeDatabase().suffixForFileName(urls.first().fileName());
        if (suffix.isEmpty()) {
            const QString name = urls.first().fileName();
            const int dot = name.lastIndexOf(QLatin1Char('.'));
            suffix = dot > 0 ? name.mid(dot + 1) : QString();
        }
    }
    if (suffix.isEmpty()) {
        const QMimeType m = QMimeDatabase().mimeTypeForName(mime);
        suffix = m.preferredSuffix();
    }
    return suffix.isEmpty() ? i18nc("@title:window", "Select an app to open this file") : i18nc("@title:window", "Select an app to open this .%1 file", suffix);
}
}

CardDialog *openWithDialog(QWidget *parent, const QList<QUrl> &urls, const QString &mime, const Chosen &done, const QStringList &preferred)
{
    auto *dialog = new CardDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFixedWidth(bodyWidth + 48);
    dialog->setTitle(titleFor(mime, urls));

    auto *model = new AppChoicesModel(dialog);
    model->setMimeType(mime);
    model->prefer(preferred);
    const QMimeType type = QMimeDatabase().mimeTypeForName(mime);
    const QString fileName = urls.isEmpty() ? type.comment() : urls.first().fileName();
    QQuickWidget *view = Raye::Jobs::quickPage(QStringLiteral("OpenWithPage"),
                                               {{QStringLiteral("model"), QVariant::fromValue<QObject *>(model)},
                                                {QStringLiteral("fileName"), fileName},
                                                {QStringLiteral("fileIcon"), type.isValid() ? type.iconName() : QStringLiteral("unknown")},
                                                {QStringLiteral("forFile"), !urls.isEmpty()},
                                                {QStringLiteral("storeAvailable"), !QStandardPaths::findExecutable(QStringLiteral("raye-store")).isEmpty()}},
                                               nullptr,
                                               "bgCardSolid");
    dialog->setContentWidget(view);
    if (QQuickItem *root = view->rootObject()) {
        root->setWidth(bodyWidth);
        settleNow(root);
    }
    Raye::Jobs::fitToPage(view, bodyWidth, dialog);

    dialog->setButtonText(CardDialog::Primary, i18nc("@action:button", "Always"));
    if (!urls.isEmpty()) {
        dialog->setButtonText(CardDialog::Secondary, i18nc("@action:button", "Just once"));
        dialog->setDefaultButton(CardDialog::Secondary);
    }
    dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
    // A long title takes two lines at this width: the height has to be asked
    // for this width, the size hint counts it on one line.
    dialog->resize(dialog->width(), dialog->layout()->totalHeightForWidth(dialog->width()));

    // done is called once, with null when the dialog goes another way.
    auto pending = std::make_shared<Chosen>(done);
    auto finish = [pending](const KService::Ptr &s, bool always) {
        if (*pending) {
            Chosen f = std::move(*pending);
            *pending = nullptr;
            f(s, always);
        }
    };
    QObject::connect(dialog, &QObject::destroyed, [finish] {
        finish({}, false);
    });

    QPointer<AppChoicesModel> modelGuard(model);
    QPointer<CardDialog> guard(dialog);
    auto pick = [modelGuard, mime, finish](bool always) {
        if (!modelGuard) {
            return;
        }
        const KService::Ptr s = modelGuard->currentService();
        if (s && always) {
            setAlways(mime, s);
        }
        finish(s, always);
    };
    QObject::connect(dialog, &CardDialog::buttonClicked, dialog, [pick](CardDialog::ButtonRole role) {
        if (role == CardDialog::Primary) {
            pick(true);
        } else if (role == CardDialog::Secondary) {
            pick(false);
        }
    });
    auto updateButtons = [guard, modelGuard] {
        if (guard && modelGuard) {
            const bool any = modelGuard->currentService();
            if (auto *b = guard->button(CardDialog::Primary)) {
                b->setEnabled(any);
            }
            if (auto *b = guard->button(CardDialog::Secondary)) {
                b->setEnabled(any);
            }
        }
    };
    QObject::connect(model, &AppChoicesModel::currentIndexChanged, dialog, updateButtons);
    updateButtons();

    if (QQuickItem *root = view->rootObject()) {
        auto *signals_ = new PageSignals(dialog);
        QObject::connect(root, SIGNAL(browseRequested()), signals_, SLOT(onBrowse()));
        QObject::connect(root, SIGNAL(storeRequested()), signals_, SLOT(onStore()));
        QObject::connect(root, SIGNAL(activated()), signals_, SLOT(onActivated()));
        signals_->browse = [guard, modelGuard, mime] {
            if (!guard) {
                return;
            }
            const QUrl picked = Raye::Portal::pickFile(guard, i18nc("@title:window", "Look for an app on this PC"), QUrl::fromLocalFile(QStringLiteral("/usr/bin")));
            if (picked.isEmpty() || !modelGuard) {
                return;
            }
            const KService::Ptr s = serviceForProgram(picked.toLocalFile(), mime);
            if (s) {
                modelGuard->addBrowsed(s);
            }
        };
        signals_->store = [] {
            // The Store takes no search from the command line yet (it
            // refuses unknown options): it opens on its home page.
            QProcess::startDetached(QStringLiteral("raye-store"), {});
        };
        // A double-click opens it just this once (or sets it, with no file).
        const bool forFile = !urls.isEmpty();
        signals_->activated = [pick, guard, forFile] {
            pick(!forFile);
            if (guard) {
                guard->accept();
            }
        };
    }
    return dialog;
}

bool chooseDefaultApp(const QString &mime, QWidget *parent)
{
    const QMimeType type = QMimeDatabase().mimeTypeForName(mime);
    if (!type.isValid()) {
        return false;
    }
    CardDialog *dialog = openWithDialog(parent, {}, type.name(), [](const KService::Ptr &, bool) { });
    dialog->exec();
    return true;
}

OpenWithHandler::OpenWithHandler(QObject *parent)
    : KIO::OpenWithHandlerInterface(parent)
{
}

void OpenWithHandler::promptUserForApplication(KJob *job, const QList<QUrl> &urls, const QString &mimeType)
{
    QWidget *parent = job ? KJobWidgets::window(job) : nullptr;
    QPointer<OpenWithHandler> self(this);
    CardDialog *dialog = openWithDialog(parent, urls, mimeType, [self](const KService::Ptr &service, bool) {
        if (!self) {
            return;
        }
        if (service) {
            Q_EMIT self->serviceSelected(service);
        } else {
            Q_EMIT self->canceled();
        }
    });
    dialog->show();
}

}

// Every Raye job delegate carries the Raye "Open with".
static void rayeOpenWithRegister()
{
    Raye::Jobs::addDelegateInterface([] {
        return new Raye::OpenWith::OpenWithHandler;
    });
}
Q_COREAPP_STARTUP_FUNCTION(rayeOpenWithRegister)

RAYE_SHOWCASE(openWithDialog, "OpenWithDialog", "How do you want to open this file?")
{
    auto *host = new QWidget(parent);
    host->setAutoFillBackground(true);
    Raye::Ui::CardDialog *dialog = Raye::OpenWith::openWithDialog(host,
                                                        {QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents/notes.txt"))},
                                                        QStringLiteral("text/plain"),
                                                        [](const KService::Ptr &, bool) { });
    QTimer::singleShot(0, dialog, [dialog] {
        dialog->show();
    });
    return host;
}

#include "openwithdialog.moc"
