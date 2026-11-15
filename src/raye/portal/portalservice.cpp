/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "portalservice.h"

#include "filedialog.h"
#include "openwith/openwithdialog.h"
#include "request.h"
#include "ui/carddialog.h"

#include <KService>
#include <KWindowSystem>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QMimeDatabase>
#include <QPointer>
#include <QWindow>

// The wire format of the options, as xdg-desktop-portal documents it.
namespace
{
struct WireGlob {
    uint type = 0; // 0 a glob, 1 a MIME type
    QString pattern;
};
using WireGlobs = QList<WireGlob>;
struct WireFilter {
    QString label;
    WireGlobs globs;
};
using WireFilters = QList<WireFilter>;
struct WireOption {
    QString id;
    QString label;
};
using WireOptions = QList<WireOption>;
struct WireChoice {
    QString id;
    QString label;
    WireOptions options;
    QString initial;
};
using WireChoices = QList<WireChoice>;
struct WireChosen {
    QString id;
    QString value;
};
using WireChosenList = QList<WireChosen>;
}

Q_DECLARE_METATYPE(WireGlob)
Q_DECLARE_METATYPE(WireFilter)
Q_DECLARE_METATYPE(WireOption)
Q_DECLARE_METATYPE(WireChoice)
Q_DECLARE_METATYPE(WireChosen)

namespace
{
QDBusArgument &operator<<(QDBusArgument &arg, const WireGlob &v)
{
    arg.beginStructure();
    arg << v.type << v.pattern;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, WireGlob &v)
{
    arg.beginStructure();
    arg >> v.type >> v.pattern;
    arg.endStructure();
    return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const WireFilter &v)
{
    arg.beginStructure();
    arg << v.label << v.globs;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, WireFilter &v)
{
    arg.beginStructure();
    arg >> v.label >> v.globs;
    arg.endStructure();
    return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const WireOption &v)
{
    arg.beginStructure();
    arg << v.id << v.label;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, WireOption &v)
{
    arg.beginStructure();
    arg >> v.id >> v.label;
    arg.endStructure();
    return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const WireChoice &v)
{
    arg.beginStructure();
    arg << v.id << v.label << v.options << v.initial;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, WireChoice &v)
{
    arg.beginStructure();
    arg >> v.id >> v.label >> v.options >> v.initial;
    arg.endStructure();
    return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const WireChosen &v)
{
    arg.beginStructure();
    arg << v.id << v.value;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, WireChosen &v)
{
    arg.beginStructure();
    arg >> v.id >> v.value;
    arg.endStructure();
    return arg;
}

int s_openDialogs = 0;
}

namespace Raye::Portal
{

namespace
{
void registerTypes()
{
    static const bool done = [] {
        qDBusRegisterMetaType<WireGlob>();
        qDBusRegisterMetaType<WireGlobs>();
        qDBusRegisterMetaType<WireFilter>();
        qDBusRegisterMetaType<WireFilters>();
        qDBusRegisterMetaType<WireOption>();
        qDBusRegisterMetaType<WireOptions>();
        qDBusRegisterMetaType<WireChoice>();
        qDBusRegisterMetaType<WireChoices>();
        qDBusRegisterMetaType<WireChosen>();
        qDBusRegisterMetaType<WireChosenList>();
        return true;
    }();
    Q_UNUSED(done)
}

FileType toFileType(const WireFilter &filter)
{
    FileType type;
    type.label = filter.label;
    for (const WireGlob &glob : filter.globs) {
        (glob.type == 1 ? type.mimeTypes : type.globs).append(glob.pattern);
    }
    return type;
}

WireFilter toWire(const FileType &type)
{
    WireFilter filter;
    filter.label = type.label;
    for (const QString &glob : type.globs) {
        filter.globs.append({0, glob});
    }
    for (const QString &mime : type.mimeTypes) {
        filter.globs.append({1, mime});
    }
    return filter;
}

QByteArray pathOption(const QVariantMap &options, const char *key)
{
    return options.value(QLatin1String(key)).toByteArray();
}

FileRequest readRequest(Mode mode, const QString &title, const QVariantMap &options)
{
    FileRequest request;
    request.mode = mode;
    request.title = title;
    request.acceptLabel = options.value(QStringLiteral("accept_label")).toString();
    request.multiple = options.value(QStringLiteral("multiple")).toBool();
    if (mode == Mode::OpenFile && options.value(QStringLiteral("directory")).toBool()) {
        request.mode = Mode::OpenFolder;
    }

    QString folder = decodePath(pathOption(options, "current_folder"));
    const QString currentFile = decodePath(pathOption(options, "current_file"));
    request.name = options.value(QStringLiteral("current_name")).toString();
    if (!currentFile.isEmpty()) {
        const int slash = currentFile.lastIndexOf(QLatin1Char('/'));
        if (folder.isEmpty() && slash > 0) {
            folder = currentFile.left(slash);
        }
        if (request.name.isEmpty()) {
            request.name = currentFile.mid(slash + 1);
        }
    }
    // A relative path means nothing for another process.
    if (folder.startsWith(QLatin1Char('/'))) {
        request.folder = QUrl::fromLocalFile(folder);
    }

    if (options.contains(QStringLiteral("filters"))) {
        const auto filters = qdbus_cast<WireFilters>(options.value(QStringLiteral("filters")));
        for (const WireFilter &filter : filters) {
            request.types.append(toFileType(filter));
        }
    }
    if (options.contains(QStringLiteral("current_filter"))) {
        const FileType current = toFileType(qdbus_cast<WireFilter>(options.value(QStringLiteral("current_filter"))));
        int at = request.types.indexOf(current);
        if (at < 0) {
            // The spec lets an app name a filter outside its list.
            request.types.prepend(current);
            at = 0;
        }
        request.currentType = at;
    }
    if (options.contains(QStringLiteral("choices"))) {
        const auto choices = qdbus_cast<WireChoices>(options.value(QStringLiteral("choices")));
        for (const WireChoice &wire : choices) {
            Choice choice;
            choice.id = wire.id;
            choice.label = wire.label;
            choice.initial = wire.initial;
            for (const WireOption &option : wire.options) {
                choice.options.append({option.id, option.label});
            }
            request.choices.append(choice);
        }
    }
    if (mode == Mode::SaveFiles) {
        const auto files = qdbus_cast<QList<QByteArray>>(options.value(QStringLiteral("files")));
        for (const QByteArray &file : files) {
            request.names.append(decodePath(file));
        }
    }
    return request;
}

// Closes the dialog when the app gives up on the request.
class CloseAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Request")
public:
    CloseAdaptor(QObject *owner, QDialog *dialog)
        : QDBusAbstractAdaptor(owner)
        , m_dialog(dialog)
    {
    }

public Q_SLOTS:
    void Close()
    {
        if (m_dialog) {
            m_dialog->reject();
        }
    }

private:
    QPointer<QDialog> m_dialog;
};

void exportRequest(const QDBusObjectPath &handle, QDialog *dialog)
{
    auto *owner = new QObject(dialog);
    new CloseAdaptor(owner, dialog);
    QDBusConnection::sessionBus().registerObject(handle.path(), owner, QDBusConnection::ExportAdaptors);
}

void parentOn(QDialog *dialog, const QString &parentWindow, bool modal)
{
    dialog->winId();
    QWindow *window = dialog->windowHandle();
    if (!window) {
        return;
    }
    if (parentWindow.startsWith(QLatin1String("x11:"))) {
        KWindowSystem::setMainWindow(window, QStringView(parentWindow).mid(4).toULongLong(nullptr, 16));
    } else if (parentWindow.startsWith(QLatin1String("wayland:"))) {
        KWindowSystem::setMainWindow(window, parentWindow.mid(int(qstrlen("wayland:"))));
    }
    dialog->setWindowModality(modal ? Qt::WindowModal : Qt::NonModal);
}

void countDialog(QDialog *dialog)
{
    ++s_openDialogs;
    QObject::connect(dialog, &QObject::destroyed, [] {
        --s_openDialogs;
    });
}

void showFileDialog(Mode mode,
                    const QDBusObjectPath &handle,
                    const QString &parentWindow,
                    const QString &title,
                    const QVariantMap &options,
                    const QDBusMessage &message)
{
    message.setDelayedReply(true);
    auto *dialog = new FileDialog(readRequest(mode, title, options));
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);
    countDialog(dialog);
    exportRequest(handle, dialog);
    parentOn(dialog, parentWindow, options.value(QStringLiteral("modal"), true).toBool());

    const QDBusMessage pending = message;
    QObject::connect(dialog, &QDialog::finished, dialog, [dialog, pending](int result) {
        QVariantMap results;
        uint response = responseCancelled;
        if (result == QDialog::Accepted) {
            response = responseSuccess;
            QStringList uris;
            const auto urls = dialog->selectedUrls();
            for (const QUrl &url : urls) {
                uris.append(QString::fromUtf8(url.toEncoded()));
            }
            results.insert(QStringLiteral("uris"), uris);
            WireChosenList chosen;
            const auto values = dialog->choiceValues();
            for (auto it = values.cbegin(); it != values.cend(); ++it) {
                chosen.append({it.key(), it.value()});
            }
            if (!chosen.isEmpty()) {
                results.insert(QStringLiteral("choices"), QVariant::fromValue(chosen));
            }
            const auto types = dialog->types();
            if (dialog->currentType() >= 0 && dialog->currentType() < types.size()) {
                results.insert(QStringLiteral("current_filter"), QVariant::fromValue(toWire(types.at(dialog->currentType()))));
            }
            results.insert(QStringLiteral("writable"), true);
        }
        QDBusMessage reply = pending.createReply();
        reply << response << results;
        QDBusConnection::sessionBus().send(reply);
        dialog->deleteLater();
    });
    dialog->show();
}
}

FileChooserAdaptor::FileChooserAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    registerTypes();
}

void FileChooserAdaptor::OpenFile(const QDBusObjectPath &handle,
                                  const QString &,
                                  const QString &parent_window,
                                  const QString &title,
                                  const QVariantMap &options,
                                  const QDBusMessage &message,
                                  uint &,
                                  QVariantMap &)
{
    showFileDialog(Mode::OpenFile, handle, parent_window, title, options, message);
}

void FileChooserAdaptor::SaveFile(const QDBusObjectPath &handle,
                                  const QString &,
                                  const QString &parent_window,
                                  const QString &title,
                                  const QVariantMap &options,
                                  const QDBusMessage &message,
                                  uint &,
                                  QVariantMap &)
{
    showFileDialog(Mode::SaveFile, handle, parent_window, title, options, message);
}

void FileChooserAdaptor::SaveFiles(const QDBusObjectPath &handle,
                                   const QString &,
                                   const QString &parent_window,
                                   const QString &title,
                                   const QVariantMap &options,
                                   const QDBusMessage &message,
                                   uint &,
                                   QVariantMap &)
{
    showFileDialog(Mode::SaveFiles, handle, parent_window, title, options, message);
}

AppChooserAdaptor::AppChooserAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

void AppChooserAdaptor::ChooseApplication(const QDBusObjectPath &handle,
                                          const QString &,
                                          const QString &parent_window,
                                          const QStringList &choices,
                                          const QVariantMap &options,
                                          const QDBusMessage &message,
                                          uint &,
                                          QVariantMap &)
{
    message.setDelayedReply(true);

    QList<QUrl> urls;
    const QString uri = options.value(QStringLiteral("uri")).toString();
    const QString filename = options.value(QStringLiteral("filename")).toString();
    if (!uri.isEmpty()) {
        urls.append(QUrl(uri));
    } else if (!filename.isEmpty()) {
        urls.append(QUrl::fromLocalFile(filename));
    }
    QString mime = options.value(QStringLiteral("content_type")).toString();
    if (mime.isEmpty() && !urls.isEmpty()) {
        mime = QMimeDatabase().mimeTypeForFile(urls.first().toLocalFile()).name();
    }
    QStringList preferred = choices;
    const QString last = options.value(QStringLiteral("last_choice")).toString();
    if (!last.isEmpty()) {
        preferred.removeAll(last);
        preferred.prepend(last);
    }

    const QDBusMessage pending = message;
    auto answered = std::make_shared<bool>(false);
    Raye::Ui::CardDialog *dialog = Raye::OpenWith::openWithDialog(
        nullptr,
        urls,
        mime,
        [pending, answered](const KService::Ptr &service, bool) {
            if (*answered) {
                return;
            }
            *answered = true;
            QVariantMap results;
            uint response = responseCancelled;
            if (service) {
                response = responseSuccess;
                QString id = service->storageId();
                if (id.endsWith(QLatin1String(".desktop"))) {
                    id.chop(int(qstrlen(".desktop")));
                }
                results.insert(QStringLiteral("choice"), id);
            }
            QDBusMessage reply = pending.createReply();
            reply << response << results;
            QDBusConnection::sessionBus().send(reply);
        },
        preferred);
    countDialog(dialog);
    exportRequest(handle, dialog);
    parentOn(dialog, parent_window, options.value(QStringLiteral("modal"), true).toBool());
    dialog->show();
}

void AppChooserAdaptor::UpdateChoices(const QDBusObjectPath &, const QStringList &)
{
}

void addPortalInterfaces(QObject *object)
{
    new FileChooserAdaptor(object);
    new AppChooserAdaptor(object);
}

int openDialogs()
{
    return s_openDialogs;
}

}

#include "portalservice.moc"
