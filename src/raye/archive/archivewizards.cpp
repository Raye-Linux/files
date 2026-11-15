/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archivewizards.h"

#include "archivejobs.h"
#include "archivewizardmodels.h"
#include "archivewizards_p.h"
#include "ui/carddialog.h"
#include "ui/showcase.h"
#include "ui/tokens.h"

#include <KIO/JobTracker>
#include <KJobTrackerInterface>
#include <KLocalizedString>

#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QPointer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QtMath>

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
#include <KLocalizedContext>
QT_WARNING_POP

namespace Raye::Archive::Wizards
{

using Raye::Ui::CardDialog;

namespace
{

// The width of the body of a 520 px card (24 px padding each side).
constexpr int bodyWidth = 472;

// A QML page of org.raye.files as the body of a Raye dialog. The view is
// as big as the page wants (an error line appearing grows the card).
QQuickWidget *pageView(CardDialog *dialog, const QString &type, const QVariantMap &properties, int width = bodyWidth)
{
    auto *view = new QQuickWidget;
    view->setResizeMode(QQuickWidget::SizeViewToRootObject);
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    auto *context = new KLocalizedContext(view->engine());
    QT_WARNING_POP
    context->setTranslationDomain(QStringLiteral("raye-files"));
    view->engine()->rootContext()->setContextObject(context);
    view->setClearColor(Raye::Ui::Tokens::current().color(Raye::Ui::TokenSet::bgCardSolid));
    view->loadFromModule("org.raye.files", type);

    QQuickItem *root = view->rootObject();
    if (!root) {
        for (const QQmlError &e : view->errors()) {
            qWarning("%s", qPrintable(e.toString()));
        }
        dialog->setContentWidget(view);
        return view;
    }
    root->setWidth(width);
    // Set after loading: the page starts with a model of its own, so its
    // bindings never see an empty one.
    for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
        if (!root->setProperty(it.key().toUtf8().constData(), it.value())) {
            qWarning("archive wizard: %s has no property %s", qPrintable(type), qPrintable(it.key()));
        }
        // A model goes with the view, never before it: the page's bindings
        // must not outlive what they read.
        if (QObject *object = it.value().value<QObject *>()) {
            object->setParent(view);
        }
    }
    // The page's height follows its content; the view and the card follow
    // the page.
    root->setHeight(root->implicitHeight());
    QObject::connect(root, &QQuickItem::implicitHeightChanged, root, [root] {
        root->setHeight(root->implicitHeight());
    });
    auto fit = [view, root, dialog, width] {
        const QRect before = dialog->geometry();
        view->setFixedSize(width, qMax(1, qCeil(root->height())));
        dialog->adjustSize();
        if (dialog->isVisible() && dialog->height() != before.height()) {
            // grows from its middle (Advanced options), and stays on screen
            QRect r(QPoint(before.x(), before.center().y() - dialog->height() / 2), dialog->size());
            if (const QScreen *screen = dialog->screen()) {
                const QRect room = screen->availableGeometry();
                r.moveBottom(qMin(r.bottom(), room.bottom()));
                r.moveTop(qMax(r.top(), room.top()));
            }
            dialog->move(r.topLeft());
        }
    };
    // Not from inside the scene's polish: the widget resize waits for the
    // event loop.
    QObject::connect(root, &QQuickItem::heightChanged, view, fit, Qt::QueuedConnection);
    fit();
    dialog->setContentWidget(view);
    return view;
}

CardDialog *newDialog(QWidget *parent, const QString &title, int width = bodyWidth)
{
    auto *dialog = new CardDialog(parent);
    dialog->setTitle(title);
    dialog->setFixedWidth(width + 48);
    return dialog;
}

void focusPage(QQuickWidget *view)
{
    QTimer::singleShot(0, view, [view] {
        view->setFocus(Qt::OtherFocusReason);
        if (QQuickItem *root = view->rootObject()) {
            QMetaObject::invokeMethod(root, "focusField");
        }
    });
}

void showError(QWidget *parent, const QString &title, const QString &message)
{
    CardDialog *dialog = newDialog(parent, title);
    dialog->setMessage(message);
    dialog->setButtonText(CardDialog::Primary, i18nc("@action:button", "OK"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

QString pickFolder(QWidget *parent, const QString &start)
{
    return QFileDialog::getExistingDirectory(parent, i18nc("@title:window", "Select a folder"), start);
}

void track(KJob *job)
{
    if (KJobTrackerInterface *tracker = KIO::getJobTracker()) {
        tracker->registerJob(job);
    }
}

struct ExtractRun {
    QString archive;
    QString destination;
    bool show = false;
    QString password;
    bool guard = true;
    bool collapse = false;
};

void runExtract(QPointer<QWidget> parent, const ExtractRun &run)
{
    const QString archive = run.archive;
    const QString destination = run.destination;
    const bool show = run.show;
    auto *job = new ArchiveExtractJob(archive, destination);
    job->setPassword(run.password);
    job->setGuardEnabled(run.guard);
    job->setConflicts(Conflicts::Ask);
    job->setCollapseSingleFolder(run.collapse);
    job->setWindow(parent ? parent->window() : nullptr);
    track(job);
    QObject::connect(job, &KJob::result, job, [=](KJob *) {
        const QString name = QFileInfo(archive).fileName();
        switch (job->archiveError()) {
        case Error::None:
            if (show) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(destination));
            }
            return;
        case Error::Cancelled:
            return;
        case Error::PasswordNeeded:
        case Error::WrongPassword: {
            const QString next = askPassword(parent, name, job->archiveError() == Error::WrongPassword);
            if (!next.isEmpty()) {
                ExtractRun again = run;
                again.password = next;
                runExtract(parent, again);
            }
            return;
        }
        case Error::TooLarge:
            if (confirmExpand(parent, name)) {
                ExtractRun again = run;
                again.guard = false;
                runExtract(parent, again);
            }
            return;
        default:
            if (job->error() == KJob::KilledJobError) {
                return;
            }
            showError(parent, i18nc("@title:window", "Can't extract the files"), job->errorText());
        }
    });
    job->start();
}

void runCreate(QPointer<QWidget> parent, const QStringList &sources, const QString &archive, const CreateOptions &options, const std::function<void(const QString &)> &created)
{
    auto *job = new ArchiveCreateJob(sources, archive, options);
    track(job);
    QObject::connect(job, &KJob::result, job, [=](KJob *) {
        if (job->error() == KJob::NoError) {
            if (created) {
                created(job->archivePath());
            }
            return;
        }
        if (job->error() == KJob::KilledJobError || job->archiveError() == Error::Cancelled) {
            return;
        }
        showError(parent, i18nc("@title:window", "Can't create the archive"), job->errorText());
    });
    job->start();
}

void place(CardDialog *dialog, QWidget *over)
{
    QTimer::singleShot(0, dialog, [dialog, over] {
        dialog->adjustSize();
        if (over) {
            const QPoint centre = over->window()->mapToGlobal(over->window()->rect().center());
            QRect r(centre - QPoint(dialog->width() / 2, dialog->height() / 2), dialog->size());
            if (const QScreen *screen = over->screen()) {
                const QRect room = screen->availableGeometry();
                r.moveBottom(qMin(r.bottom(), room.bottom()));
                r.moveTop(qMax(r.top(), room.top()));
            }
            dialog->move(r.topLeft());
        }
        dialog->show();
        // after show(): the dialog's own first focus would undo it
        if (auto *view = qobject_cast<QQuickWidget *>(dialog->contentWidget())) {
            focusPage(view);
        }
    });
}

// Wires the page's Browse… and Enter to the dialog.
PageSignals *wire(CardDialog *dialog, QQuickWidget *view, std::function<void()> browse, std::function<void()> accept)
{
    auto *signals_ = new PageSignals(dialog);
    signals_->onBrowse = std::move(browse);
    signals_->onAccept = std::move(accept);
    if (QQuickItem *root = view ? view->rootObject() : nullptr) {
        if (root->metaObject()->indexOfSignal("browseRequested()") >= 0) {
            QObject::connect(root, SIGNAL(browseRequested()), signals_, SLOT(browse()));
        }
        QObject::connect(root, SIGNAL(accepted()), signals_, SLOT(accept()));
    }
    return signals_;
}
}

namespace
{
CardDialog *makeExtractDialog(QWidget *parent, const QString &archivePath, ExtractAllModel **modelOut)
{
    CardDialog *dialog = newDialog(parent, i18nc("@title:window", "Extract compressed (zipped) folders"));
    auto *model = new ExtractAllModel(dialog);
    model->setArchivePath(QFileInfo(archivePath).absoluteFilePath());
    pageView(dialog, QStringLiteral("ExtractAllPage"), {{QStringLiteral("model"), QVariant::fromValue<QObject *>(model)}});
    dialog->setButtonText(CardDialog::Primary, i18nc("@action:button", "Extract"));
    dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
    dialog->setClosesOnButton(false);
    *modelOut = model;
    return dialog;
}

CardDialog *makeCompressDialog(QWidget *parent, const QStringList &sources, CompressOptionsModel **modelOut)
{
    // wide enough for the advanced options in two columns
    constexpr int width = 672;
    CardDialog *dialog = newDialog(parent, i18nc("@title:window", "Add to archive"), width);
    auto *model = new CompressOptionsModel(dialog);
    model->setSources(sources);
    pageView(dialog, QStringLiteral("CompressOptionsPage"), {{QStringLiteral("model"), QVariant::fromValue<QObject *>(model)}}, width);
    dialog->setButtonText(CardDialog::Primary, i18nc("@action:button", "Create"));
    dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
    dialog->setClosesOnButton(false);
    *modelOut = model;
    return dialog;
}

CardDialog *makePasswordDialog(QWidget *parent, const QString &archiveName, bool wrongPassword)
{
    CardDialog *dialog = newDialog(parent, i18nc("@title:window", "Enter password"));
    QQuickWidget *view = pageView(dialog,
                                  QStringLiteral("ArchivePasswordPage"),
                                  {{QStringLiteral("archiveName"), archiveName}, {QStringLiteral("wrongPassword"), wrongPassword}});
    dialog->setButtonText(CardDialog::Primary, i18nc("@action:button", "OK"));
    dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
    focusPage(view);
    return dialog;
}

}

void extractAll(QWidget *parent, const QString &archivePath)
{
    ExtractAllModel *model = nullptr;
    CardDialog *dialog = makeExtractDialog(parent, archivePath, &model);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QPointer<QWidget> owner = parent;
    const QString archive = QFileInfo(archivePath).absoluteFilePath();

    auto accept = [dialog, model, owner, archive] {
        if (!model->validate()) {
            return;
        }
        ExtractRun run;
        run.archive = archive;
        run.destination = model->resolvedDestination();
        run.show = model->showWhenDone();
        run.collapse = run.destination == ExtractAllModel::defaultDestination(archive);
        dialog->close();
        runExtract(owner, run);
    };
    auto browse = [dialog, model] {
        const QString chosen = pickFolder(dialog, QFileInfo(model->resolvedDestination()).absolutePath());
        if (!chosen.isEmpty()) {
            model->setDestination(chosen);
        }
    };
    wire(dialog, qobject_cast<QQuickWidget *>(dialog->contentWidget()), browse, accept);
    QObject::connect(dialog, &CardDialog::buttonClicked, dialog, [dialog, accept](CardDialog::ButtonRole role) {
        if (role == CardDialog::Primary) {
            accept();
        } else {
            dialog->close();
        }
    });
    place(dialog, parent);
}

void extractHere(QWidget *parent, const QString &archivePath)
{
    ExtractRun run;
    run.archive = QFileInfo(archivePath).absoluteFilePath();
    run.destination = QFileInfo(run.archive).absolutePath();
    runExtract(parent, run);
}

void extractToFolder(QWidget *parent, const QString &archivePath)
{
    ExtractRun run;
    run.archive = QFileInfo(archivePath).absoluteFilePath();
    run.destination = ExtractAllModel::defaultDestination(run.archive);
    run.collapse = true;
    runExtract(parent, run);
}

void compressWithOptions(QWidget *parent, const QStringList &sources)
{
    if (sources.isEmpty()) {
        return;
    }
    CompressOptionsModel *model = nullptr;
    CardDialog *dialog = makeCompressDialog(parent, sources, &model);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QPointer<QWidget> owner = parent;

    auto accept = [dialog, model, owner] {
        if (!model->validate()) {
            return;
        }
        const QStringList items = model->sources();
        const QString path = model->archivePath();
        const CreateOptions options = model->options();
        dialog->close();
        runCreate(owner, items, path, options, {});
    };
    auto browse = [dialog, model] {
        const QString chosen = pickFolder(dialog, model->folder());
        if (!chosen.isEmpty()) {
            model->setFolder(chosen);
        }
    };
    wire(dialog, qobject_cast<QQuickWidget *>(dialog->contentWidget()), browse, accept);
    QObject::connect(dialog, &CardDialog::buttonClicked, dialog, [dialog, accept](CardDialog::ButtonRole role) {
        if (role == CardDialog::Primary) {
            accept();
        } else {
            dialog->close();
        }
    });
    place(dialog, parent);
}

void compressNow(QWidget *parent, const QStringList &sources, Format format, const std::function<void(const QString &)> &created)
{
    if (sources.isEmpty()) {
        return;
    }
    CreateOptions options;
    options.format = format;
    // quick by default: the Add to archive window is there for more
    options.level = Level::Fast;
    if (format == Format::Tar) {
        options.tarFilter = TarFilter::Zstd;
    }
    const QFileInfo first(sources.first());
    const QString folder = first.absolutePath();
    const QString name = uniqueFileName(folder, archiveBaseNameFor(first.fileName(), first.isDir()) + options.extension());
    runCreate(parent, sources, folder + QLatin1Char('/') + name, options, created);
}

QString askPassword(QWidget *parent, const QString &archiveName, bool wrongPassword)
{
    CardDialog *dialog = makePasswordDialog(parent, archiveName, wrongPassword);
    auto *view = qobject_cast<QQuickWidget *>(dialog->contentWidget());
    wire(dialog, view, {}, [dialog] {
        dialog->accept();
    });
    QString password;
    if (dialog->exec() == CardDialog::PrimaryChosen && view && view->rootObject()) {
        password = view->rootObject()->property("password").toString();
    }
    delete dialog;
    return password;
}

bool confirmExpand(QWidget *parent, const QString &archiveName)
{
    CardDialog *dialog = newDialog(parent, i18nc("@title:window", "Extract anyway?"));
    dialog->setMessage(errorMessage(Error::TooLarge) + QStringLiteral("\n\n") + archiveName);
    dialog->setButtonText(CardDialog::Primary, i18nc("@action:button", "Extract anyway"));
    dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
    // the safe answer is the one Enter gives
    dialog->setDefaultButton(CardDialog::Close);
    const bool yes = dialog->exec() == CardDialog::PrimaryChosen;
    delete dialog;
    return yes;
}

// ---- Render harness ---------------------------------------------------------
// Sample data only; nothing here runs unless tests/raye/harness asks.

namespace
{
QWidget *backdrop(QWidget *parent)
{
    auto *page = new QWidget(parent);
    page->setAutoFillBackground(true);
    return page;
}

void showOver(CardDialog *dialog, QWidget *page)
{
    dialog->setTranslucent(true);
    place(dialog, page);
}
}

RAYE_SHOWCASE(extractAllDialog, "ExtractAllDialog", "Extract all, over a folder")
{
    QWidget *page = backdrop(parent);
    ExtractAllModel *model = nullptr;
    showOver(makeExtractDialog(page, QStringLiteral("/home/ana/Downloads/Holiday photos 2026.zip"), &model), page);
    return page;
}

RAYE_SHOWCASE(extractAllError, "ExtractAllDialogError", "Extract all with a folder that cannot be used")
{
    QWidget *page = backdrop(parent);
    ExtractAllModel *model = nullptr;
    CardDialog *dialog = makeExtractDialog(page, QStringLiteral("/home/ana/Downloads/Holiday photos 2026.zip"), &model);
    model->setDestination(QStringLiteral("/etc/hostname"));
    model->validate();
    showOver(dialog, page);
    return page;
}

RAYE_SHOWCASE(compressOptionsDialog, "CompressOptionsDialog", "Compress to, Additional options")
{
    QWidget *page = backdrop(parent);
    CompressOptionsModel *model = nullptr;
    showOver(makeCompressDialog(page, {QStringLiteral("/home/ana/Documents/Report 2026.docx")}, &model), page);
    return page;
}

RAYE_SHOWCASE(compressOptionsAdvanced, "CompressOptionsDialogAdvanced", "Add to archive with the advanced options open")
{
    QWidget *page = backdrop(parent);
    CompressOptionsModel *model = nullptr;
    CardDialog *dialog = makeCompressDialog(page, {QStringLiteral("/home/ana/Projects/website")}, &model);
    model->setFormat(int(Format::SevenZip));
    model->setLevelIndex(4);
    model->setVolumeIndex(2);
    model->setPassword(QStringLiteral("hunter22"));
    model->setPasswordAgain(QStringLiteral("hunter22"));
    model->setEncryptNames(true);
    if (auto *view = qobject_cast<QQuickWidget *>(dialog->contentWidget()); view && view->rootObject()) {
        view->rootObject()->setProperty("advanced", true);
    }
    showOver(dialog, page);
    return page;
}

RAYE_SHOWCASE(compressOptionsTar, "CompressOptionsDialogTar", "Additional options with TAR and gzip")
{
    QWidget *page = backdrop(parent);
    CompressOptionsModel *model = nullptr;
    CardDialog *dialog = makeCompressDialog(page, {QStringLiteral("/home/ana/Projects/website")}, &model);
    model->setFormat(int(Format::Tar));
    model->setTarFilter(int(TarFilter::Gzip));
    model->setKeepSymlinks(true);
    showOver(dialog, page);
    return page;
}

RAYE_SHOWCASE(archivePasswordDialog, "ArchivePasswordDialog", "Password of a protected archive")
{
    QWidget *page = backdrop(parent);
    showOver(makePasswordDialog(page, QStringLiteral("Tax return 2025.zip"), true), page);
    return page;
}

RAYE_SHOWCASE(archiveExpandDialog, "ArchiveExpandDialog", "An archive that expands too much")
{
    QWidget *page = backdrop(parent);
    CardDialog *dialog = newDialog(page, i18nc("@title:window", "Extract anyway?"));
    dialog->setMessage(errorMessage(Error::TooLarge) + QStringLiteral("\n\n") + QStringLiteral("free-movies.zip"));
    dialog->setButtonText(CardDialog::Primary, i18nc("@action:button", "Extract anyway"));
    dialog->setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
    dialog->setDefaultButton(CardDialog::Close);
    showOver(dialog, page);
    return page;
}

}
