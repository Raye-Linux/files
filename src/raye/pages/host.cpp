/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "host.h"

#include "gallerymodel.h"
#include "homemodel.h"
#include "thispcmodel.h"
#include "netdrive/mapdrivedialog.h"
#include "jobs/quickhost.h"
#include "properties/propertiesdialog.h"
#include "ui/showcase.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QPointer>
#include <QStandardPaths>
#include <QEventLoop>
#include <QHash>
#include <QQuickItem>
#include <QQuickWidget>
#include <QVBoxLayout>

namespace Raye::Pages
{

namespace
{
QHash<QString, PageHost::Page> &pages()
{
    static QHash<QString, PageHost::Page> table;
    return table;
}

void registerHome()
{
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    PageHost::registerPage({QStringLiteral("home"),
                            QStringLiteral("HomePage"),
                            i18nc("@title the Home page of Files", "Home"),
                            QStringLiteral("user-home"),
                            [](QObject *parent) -> QObject * {
                                return new HomeModel(nullptr, nullptr, parent);
                            }});
    PageHost::registerPage({QStringLiteral("gallery"),
                            QStringLiteral("GalleryPage"),
                            i18nc("@title the Gallery page of Files", "Gallery"),
                            QStringLiteral("folder-pictures"),
                            [](QObject *parent) -> QObject * {
                                return new GalleryModel({}, parent);
                            }});
    PageHost::registerPage({QStringLiteral("thispc"),
                            QStringLiteral("ThisPcPage"),
                            i18nc("@title the This PC page of Files", "This PC"),
                            QStringLiteral("computer"),
                            [](QObject *parent) -> QObject * {
                                return new ThisPcModel({}, {}, parent);
                            }});
}
}

void PageHost::registerPage(const Page &page)
{
    pages().insert(page.name, page);
}

QString PageHost::pageName(const QUrl &url)
{
    if (url.scheme() != QLatin1String("raye")) {
        return QString();
    }
    QString name = url.path();
    while (name.startsWith(QLatin1Char('/'))) {
        name.remove(0, 1);
    }
    if (name.endsWith(QLatin1Char('/'))) {
        name.chop(1);
    }
    return name;
}

bool PageHost::isPageUrl(const QUrl &url)
{
    registerHome();
    return pages().contains(pageName(url));
}

QString PageHost::title(const QUrl &url)
{
    registerHome();
    return pages().value(pageName(url)).title;
}

QString PageHost::iconName(const QUrl &url)
{
    registerHome();
    return pages().value(pageName(url)).iconName;
}

QUrl PageHost::homeUrl()
{
    return QUrl(QStringLiteral("raye:home"));
}

PageHost::PageHost(QWidget *parent)
    : QWidget(parent)
{
    registerHome();
    setObjectName(QStringLiteral("RayePageHost"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    setFocusPolicy(Qt::StrongFocus);
}

bool PageHost::setUrl(const QUrl &url)
{
    const QString name = pageName(url);
    if (!isPageUrl(url)) {
        return false;
    }
    if (name == pageName(m_url) && m_view) {
        m_url = url;
        return true;
    }
    const Page page = pages().value(name);
    QObject *oldModel = m_model;
    QQuickWidget *oldView = m_view;
    m_model = page.model ? page.model(this) : nullptr;
    QVariantMap properties;
    if (m_model) {
        properties.insert(QStringLiteral("model"), QVariant::fromValue<QObject *>(m_model.data()));
        // The page asks to open things through its model's signals.
        if (m_model->metaObject()->indexOfSignal("openRequested(QUrl)") >= 0) {
            connect(m_model, SIGNAL(openRequested(QUrl)), this, SLOT(relayOpen(QUrl)));
        }
        const QMetaObject *meta = m_model->metaObject();
        if (meta->indexOfSignal("openInNewTabRequested(QUrl)") >= 0) {
            connect(m_model, SIGNAL(openInNewTabRequested(QUrl)), this, SLOT(relayNewTab(QUrl)));
        }
        if (meta->indexOfSignal("openInNewWindowRequested(QUrl)") >= 0) {
            connect(m_model, SIGNAL(openInNewWindowRequested(QUrl)), this, SLOT(relayNewWindow(QUrl)));
        }
        if (meta->indexOfSignal("propertiesRequested(QUrl)") >= 0) {
            connect(m_model, SIGNAL(propertiesRequested(QUrl)), this, SLOT(relayProperties(QUrl)));
        }
        if (auto *gallery = qobject_cast<GalleryModel *>(m_model.data())) {
            connect(gallery, &GalleryModel::addFolderRequested, this, [this, gallery] {
                QPointer<GalleryModel> model(gallery);
                const QString dir = QFileDialog::getExistingDirectory(window(),
                                                                      i18nc("@title:window", "Add a folder to Gallery"),
                                                                      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
                if (!dir.isEmpty() && model) {
                    model->setFolderIncluded(QDir::cleanPath(dir), true);
                }
            });
        }
        if (auto *thisPc = qobject_cast<ThisPcModel *>(m_model.data())) {
            connect(thisPc, &ThisPcModel::mapNetworkDriveRequested, this, [this, thisPc](const QUrl &share) {
                QPointer<PageHost> self(this);
                QPointer<ThisPcModel> model(thisPc);
                Raye::NetDrives::showMapDriveDialog(share, window(), [self, model](const QUrl &drive) {
                    if (model) {
                        model->reload();
                    }
                    if (self) {
                        self->relayOpen(drive);
                    }
                });
            });
            connect(thisPc, &ThisPcModel::reconnectRequested, this, [this, thisPc](const QString &mountPoint) {
                QPointer<PageHost> self(this);
                QPointer<ThisPcModel> model(thisPc);
                Raye::NetDrives::showMapDriveDialog(Raye::NetDrives::mappingAt(mountPoint), window(), [self, model](const QUrl &drive) {
                    if (model) {
                        model->reload();
                    }
                    if (self) {
                        self->relayOpen(drive);
                    }
                });
            });
            connect(thisPc, &ThisPcModel::errorOccurred, this, [this](const QString &text) {
                KMessageBox::error(window(), text);
            });
        }
        if (m_model->metaObject()->indexOfSignal("openLocationRequested(QUrl,QUrl)") >= 0) {
            connect(m_model, SIGNAL(openLocationRequested(QUrl, QUrl)), this, SLOT(relayOpenLocation(QUrl, QUrl)));
        }
    }
    m_view = Raye::Jobs::quickPage(page.qmlType, properties, this, "bgLayerSolid");
    m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_view->setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(m_view);
    layout()->addWidget(m_view);
    m_url = url;
    if (oldView) {
        oldView->deleteLater();
    }
    if (oldModel) {
        oldModel->deleteLater();
    }
    return true;
}

void PageHost::relayOpen(const QUrl &url)
{
    Q_EMIT openUrlRequested(url);
}

// The main window owns tabs and windows: ask it by name, so a page does
// not depend on it.
void PageHost::relayNewTab(const QUrl &url)
{
    QMetaObject::invokeMethod(window(), "openNewTabAndActivate", Q_ARG(QUrl, url));
}

void PageHost::relayNewWindow(const QUrl &url)
{
    QMetaObject::invokeMethod(window(), "openNewWindow", Q_ARG(QUrl, url));
}

void PageHost::relayProperties(const QUrl &url)
{
    Raye::Properties::showProperties({url}, window());
}

void PageHost::relayOpenLocation(const QUrl &folder, const QUrl &item)
{
    Q_EMIT openLocationRequested(folder, item);
}

void PageHost::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    // The first control of the page takes the keyboard (Tab goes on from
    // there, like the first item of a folder view).
    if (m_view) {
        m_view->setFocus(event->reason());
        if (QQuickItem *root = m_view->rootObject()) {
            QMetaObject::invokeMethod(root, "focusFirst");
        }
    }
}

}

RAYE_SHOWCASE(homePage, "HomePage", "Home with sample Quick access, Continue and Recent")
{
    using namespace Raye::Pages;
    auto *stats = new StaticStatsSource;
    auto *pins = new StaticPins;
    const QString h = QDir::homePath();
    const QStringList pinned{QStringLiteral("Desktop"), QStringLiteral("Downloads"), QStringLiteral("Documents"), QStringLiteral("Pictures"), QStringLiteral("Music"), QStringLiteral("Videos")};
    for (const QString &p : pinned) {
        HomeResource r;
        r.url = QUrl::fromLocalFile(h + QLatin1Char('/') + p);
        r.title = p;
        r.mimeType = QStringLiteral("inode/directory");
        pins->list.append(r);
    }
    for (const char *f : {"/Documents/Work", "/Pictures/Holiday 2026"}) {
        HomeResource r;
        r.url = QUrl::fromLocalFile(h + QString::fromUtf8(f));
        r.mimeType = QStringLiteral("inode/directory");
        stats->folders.append(r);
    }
    const struct {
        const char *path;
        const char *mime;
        int minutesAgo;
    } files[] = {
        {"/Documents/Work/Budget 2026.ods", "application/vnd.oasis.opendocument.spreadsheet", 12},
        {"/Documents/Work/Report Q3.pdf", "application/pdf", 55},
        {"/Pictures/Holiday 2026/Beach.jpg", "image/jpeg", 140},
        {"/Downloads/setup-notes.txt", "text/plain", 300},
        {"/Documents/Letter to the bank.odt", "application/vnd.oasis.opendocument.text", 1500},
        {"/Music/Playlist.m3u", "audio/x-mpegurl", 3000},
        {"/Documents/Recipes.md", "text/markdown", 5000},
    };
    const QDateTime now(QDate(2026, 10, 31), QTime(16, 30));
    for (const auto &f : files) {
        HomeResource r;
        r.url = QUrl::fromLocalFile(h + QString::fromUtf8(f.path));
        r.mimeType = QString::fromLatin1(f.mime);
        r.used = now.addSecs(-60 * f.minutesAgo);
        stats->recent.append(r);
    }
    stats->favs.append(stats->recent.at(1));
    // The page sits in the content area of the window, inset like the
    // folder view under the address row.
    auto *host = new QWidget(parent);
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(8, 8, 8, 8);
    auto *model = new HomeModel(stats, pins, host);
    model->setConfigName(QStringLiteral("raye-harness-homerc"));
    stats->setParent(host);
    pins->setParent(host);
    QQuickWidget *view = Raye::Jobs::quickPage(QStringLiteral("HomePage"), {{QStringLiteral("model"), QVariant::fromValue<QObject *>(model)}}, host, "bgLayerSolid");
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    layout->addWidget(view);
    return host;
}

RAYE_SHOWCASE(galleryPage, "GalleryPage", "Gallery over RAYE_HARNESS_GALLERY (a folder of pictures and videos)")
{
    using namespace Raye::Pages;
    const QString folder = qEnvironmentVariable("RAYE_HARNESS_GALLERY", QDir::homePath() + QStringLiteral("/Pictures"));
    auto *host = new QWidget(parent);
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(8, 8, 8, 8);
    auto *model = new GalleryModel({folder}, host);
    QQuickWidget *view = Raye::Jobs::quickPage(QStringLiteral("GalleryPage"), {{QStringLiteral("model"), QVariant::fromValue<QObject *>(model)}}, host, "bgLayerSolid");
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    layout->addWidget(view);
    // The harness shoots once the folder is read.
    QEventLoop wait;
    QObject::connect(model, &GalleryModel::changed, &wait, [&wait, model] {
        if (!model->loading()) {
            wait.quit();
        }
    });
    if (model->loading()) {
        wait.exec();
    }
    return host;
}

RAYE_SHOWCASE(thisPcPage, "ThisPcPage", "This PC with sample drives, a phone, network drives and computers")
{
    using namespace Raye::Pages;
    constexpr qint64 GiB = qint64(1024) * 1024 * 1024;
    auto *host = new QWidget(parent);
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(8, 8, 8, 8);
    auto *model = new ThisPcModel(
        [] {
            return QList<Raye::Locations::Drive>{
                {QStringLiteral("/"), Raye::Locations::LocationNames::systemDriveName(), QStringLiteral("drive-harddisk-system"), false},
                {QStringLiteral("/mnt/windows"), QStringLiteral("Windows"), QStringLiteral("drive-harddisk"), false},
                {QStringLiteral("/media/ann/KINGSTON"), QStringLiteral("KINGSTON"), QStringLiteral("drive-removable-media-usb"), true},
            };
        },
        [](const QString &mount) {
            if (mount == QLatin1String("/")) {
                return ThisPcModel::Space{244 * GiB, 77 * GiB};
            }
            if (mount == QLatin1String("/mnt/windows")) {
                return ThisPcModel::Space{476 * GiB, 31 * GiB};
            }
            return ThisPcModel::Space{29 * GiB, 18 * GiB};
        },
        host);
    model->setPlaceListers(
        [] {
            return QList<ThisPcModel::Place>{{QUrl(QStringLiteral("mtp:udi=/sample")), QStringLiteral("Pixel 8"), QStringLiteral("smartphone")}};
        },
        [] {
            ThisPcModel::Place drive{QUrl::fromLocalFile(QStringLiteral("/home/ann/Network/data")),
                                     ThisPcModel::shareName(QStringLiteral("data"), QStringLiteral("192.168.50.222")),
                                     QStringLiteral("folder-network")};
            drive.mapped = true;
            ThisPcModel::Place away{QUrl::fromLocalFile(QStringLiteral("/home/ann/Network/music")), QStringLiteral("music"), QStringLiteral("network-disconnect")};
            away.mapped = true;
            away.connected = false;
            return QList<ThisPcModel::Place>{drive, away};
        });
    model->setComputerFinder([](QObject *, const std::function<void(const QList<ThisPcModel::Place> &)> &found) {
        found({{QUrl(QStringLiteral("smb://nas")), QStringLiteral("NAS"), QStringLiteral("network-server")},
               {QUrl(QStringLiteral("smb://desktop-ann")), QStringLiteral("DESKTOP-ANN"), QStringLiteral("computer")}});
    });
    QQuickWidget *view = Raye::Jobs::quickPage(QStringLiteral("ThisPcPage"), {{QStringLiteral("model"), QVariant::fromValue<QObject *>(model)}}, host, "bgLayerSolid");
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    layout->addWidget(view);
    return host;
}
