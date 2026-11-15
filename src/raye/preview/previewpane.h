/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "preview/previewloader.h"

#include <KFileItem>

#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QQuickWidget;
class QStackedWidget;

namespace KIO
{
class PreviewJob;
}

namespace Raye::Preview
{

// The widget of a KParts part. The code that sees KParts' headers sets it
// (src/views/dolphinview.cpp); without it the pane looks for the widget
// among the children of the container it gave the part.
using PartWidgetResolver = QWidget *(*)(QObject *part);
DOLPHIN_EXPORT void setPartWidgetResolver(PartWidgetResolver resolver);

class ImageView;
class FontView;

// The Preview pane (explorer spec 3.8, Alt+P): the selected file itself,
// read-only. Parts (Okular, Kate's editor) are loaded the first time a file
// of their family shows and kept for the next one; pictures, fonts, audio
// and video are drawn by Files. Anything else: "No preview available."
class DOLPHIN_EXPORT PreviewPane : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewPane(QWidget *parent = nullptr);
    ~PreviewPane() override;

    void showItem(const KFileItem &item);
    // Several items, a folder or nothing: the message.
    void clear();

    KFileItem item() const;
    Kind currentKind() const;
    // The message shown, or an empty string while a preview is up.
    QString message() const;
    // Time taken by the last showItem() until something was on screen, in ms.
    qint64 lastLoadTime() const;
    // The part in use for the current file, if any (for tests).
    QObject *currentPart() const;

Q_SIGNALS:
    void loaded(qint64 milliseconds);

private:
    struct PartSlot {
        QPointer<QObject> part;
        QWidget *container = nullptr;
    };

    void showMessage(const QString &text);
    bool showPart(const Loader &loader, const QUrl &url);
    PartSlot *partFor(const Loader &loader);
    void showImage(const QString &path);
    bool showPlainText(const QString &path);
    void showMedia(const QUrl &url, bool video);
    void showFont(const QString &path);
    void showThumbnail(const KFileItem &item);
    void stopMedia();
    void closeParts();
    void finishLoad();

    QStackedWidget *m_stack = nullptr;
    QLabel *m_message = nullptr;
    ImageView *m_image = nullptr;
    QPlainTextEdit *m_text = nullptr;
    QWidget *m_partPage = nullptr;
    QQuickWidget *m_media = nullptr;
    FontView *m_font = nullptr;
    QLabel *m_thumbnail = nullptr;
    QPointer<KIO::PreviewJob> m_previewJob;
    QHash<QString, PartSlot> m_parts; // by family
    QString m_currentFamily;
    KFileItem m_item;
    Kind m_kind = Kind::None;
    QElapsedTimer m_clock;
    qint64 m_lastLoad = -1;
};

}
