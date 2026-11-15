/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "thumbnails.h"

#include <KConfigGroup>
#include <KFileItem>
#include <KIO/PreviewJob>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QPointer>

namespace Raye::Pages
{
namespace
{
class Response : public QQuickImageResponse
{
public:
    Response(const QUrl &url, const QSize &size)
    {
        // KIO jobs live on the GUI thread, and so does the answer: the
        // loader thread that made it may run no event loop of its own.
        moveToThread(qApp->thread());
        QMetaObject::invokeMethod(qApp, [this, url, size, guard = QPointer<Response>(this)] {
            if (!guard) {
                return;
            }
            const KConfigGroup g(KSharedConfig::openConfig(QStringLiteral("dolphinrc")), QStringLiteral("PreviewSettings"));
            const QStringList plugins = g.readEntry("Plugins", KIO::PreviewJob::defaultPlugins());
            auto *job = new KIO::PreviewJob(KFileItemList{KFileItem(url)}, size.isValid() ? size : QSize(256, 256), &plugins);
            job->setScaleType(KIO::PreviewJob::Scaled);
            QObject::connect(job, &KIO::PreviewJob::gotPreview, this, [this](const KFileItem &, const QPixmap &pixmap) {
                m_image = pixmap.toImage();
            });
            QObject::connect(job, &KJob::result, this, [this] {
                Q_EMIT finished();
            });
        });
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

private:
    QImage m_image;
};
}

QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    return new Response(QUrl(id), requestedSize);
}
}
