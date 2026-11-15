/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "previewpane.h"

#include "ui/tokens.h"

#include <KConfigGroup>
#include <KIO/PreviewJob>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KSharedConfig>

#include <QFile>
#include <QFontDatabase>
#include <QImageReader>
#include <QLabel>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>
#include <QStackedWidget>
#include <QStringDecoder>
#include <QVBoxLayout>
#include <QWheelEvent>

Q_LOGGING_CATEGORY(RayePreview, "raye.files.preview", QtInfoMsg)

namespace Raye::Preview
{

// A picture fitted to the pane; the wheel zooms around the pointer.
class ImageView : public QWidget
{
public:
    explicit ImageView(QWidget *parent)
        : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
    }

    void setPath(const QString &path)
    {
        m_zoom = 1.0;
        m_offset = QPointF();
        QImageReader reader(path);
        reader.setAutoTransform(true);
        // Decode no bigger than twice the pane: a 50 megapixel photo opens fast.
        const QSize full = reader.size();
        const QSize limit = size().isEmpty() ? QSize(1280, 1280) : size() * 2 * devicePixelRatioF();
        if (full.isValid() && (full.width() > limit.width() || full.height() > limit.height())) {
            reader.setScaledSize(full.scaled(limit, Qt::KeepAspectRatio));
        }
        m_image = reader.read();
        update();
    }

    bool isNull() const
    {
        return m_image.isNull();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_image.isNull()) {
            return;
        }
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF area = QRectF(rect()).adjusted(12, 12, -12, -12);
        QSizeF fitted = QSizeF(m_image.size()) / m_image.devicePixelRatio();
        if (fitted.width() > area.width() || fitted.height() > area.height()) {
            fitted.scale(area.size(), Qt::KeepAspectRatio);
        }
        fitted *= m_zoom;
        QRectF target(QPointF(), fitted);
        target.moveCenter(area.center() + m_offset);
        QPainterPath clip;
        clip.addRoundedRect(target, Raye::Ui::Metrics::radiusOverlay, Raye::Ui::Metrics::radiusOverlay);
        p.setClipPath(clip);
        p.drawImage(target, m_image);
    }

    void wheelEvent(QWheelEvent *event) override
    {
        const qreal factor = event->angleDelta().y() > 0 ? 1.25 : 0.8;
        const qreal zoom = qBound(1.0, m_zoom * factor, 8.0);
        if (zoom == m_zoom) {
            return;
        }
        // Keep the point under the pointer where it is.
        const QPointF centre = QRectF(rect()).center() + m_offset;
        const QPointF from = event->position() - centre;
        m_offset += from - from * (zoom / m_zoom);
        if (zoom == 1.0) {
            m_offset = QPointF();
        }
        m_zoom = zoom;
        update();
        event->accept();
    }

private:
    QImage m_image;
    qreal m_zoom = 1.0;
    QPointF m_offset;
};

// A font file: its name, the alphabet, then a sample line growing in size,
// as Windows' font preview shows it.
class FontView : public QWidget
{
public:
    explicit FontView(QWidget *parent)
        : QWidget(parent)
    {
    }

    ~FontView() override
    {
        release();
    }

    bool setPath(const QString &path)
    {
        release();
        m_id = QFontDatabase::addApplicationFont(path);
        const QStringList families = m_id >= 0 ? QFontDatabase::applicationFontFamilies(m_id) : QStringList();
        m_family = families.value(0);
        update();
        return !m_family.isEmpty();
    }

    QString family() const
    {
        return m_family;
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_family.isEmpty()) {
            return;
        }
        const Raye::Ui::TokenSet &t = Raye::Ui::Tokens::current();
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing);
        p.setClipRect(rect());
        int y = 16;
        p.setPen(t.color(Raye::Ui::TokenSet::textPrimary));
        QFont title = Raye::Ui::textFont(Raye::Ui::TextStyle::Subtitle);
        p.setFont(title);
        p.drawText(QRect(16, y, width() - 32, 32), Qt::AlignLeft | Qt::AlignVCenter, m_family);
        y += 40;
        QFont sample(m_family);
        sample.setPixelSize(16);
        p.setFont(sample);
        p.setPen(t.color(Raye::Ui::TokenSet::textSecondary));
        const QString alphabet = QStringLiteral("abcdefghijklmnopqrstuvwxyz\nABCDEFGHIJKLMNOPQRSTUVWXYZ\n1234567890.:,;'\"(!?)+-*/=");
        const QRect alphaRect(16, y, width() - 32, 72);
        p.drawText(alphaRect, Qt::AlignLeft | Qt::TextWordWrap, alphabet);
        y += 84;
        p.setPen(t.color(Raye::Ui::TokenSet::textPrimary));
        const QString line = i18nc("@info font sample, a sentence with every letter", "The quick brown fox jumps over the lazy dog. 1234567890");
        for (int size : {12, 18, 24, 36, 48, 60, 72}) {
            sample.setPixelSize(size);
            p.setFont(sample);
            const int h = QFontMetrics(sample).height();
            if (y + h > height()) {
                break;
            }
            p.drawText(QRect(16, y, width() - 32, h), Qt::AlignLeft | Qt::AlignVCenter, line);
            y += h + 6;
        }
    }

private:
    void release()
    {
        if (m_id >= 0) {
            QFontDatabase::removeApplicationFont(m_id);
            m_id = -1;
        }
        m_family.clear();
    }

    int m_id = -1;
    QString m_family;
};

namespace
{
// Audio and video: Qt Multimedia from QML, with Raye's play button and seek
// line. Colours come from the widget's tokens.
const char mediaQml[] = R"QML(
import QtQuick
import QtMultimedia

Item {
    id: root
    property url source
    property bool video: false
    // set from the tokens before the item shows
    property color textColor
    property color secondaryColor
    property color accentColor
    property color trackColor

    function stop() { player.stop(); player.source = ""; }

    MediaPlayer {
        id: player
        source: root.source
        audioOutput: AudioOutput {}
        videoOutput: output
    }

    VideoOutput {
        id: output
        visible: root.video
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: bar.top; margins: 12 }
    }

    Text {
        visible: !root.video
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -24
        text: "♫"
        color: root.secondaryColor
        font.pixelSize: 64
    }

    Item {
        id: bar
        height: 48
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12 }

        Rectangle {
            id: play
            objectName: "playButton"
            width: 32; height: 32; radius: 16
            anchors.verticalCenter: parent.verticalCenter
            color: root.accentColor
            Canvas {
                anchors.centerIn: parent
                width: 12; height: 12
                property bool playing: player.playbackState === MediaPlayer.PlayingState
                onPlayingChanged: requestPaint()
                onPaint: {
                    var c = getContext("2d");
                    c.reset();
                    c.fillStyle = "white";
                    if (playing) {
                        c.fillRect(1, 0, 3.5, 12);
                        c.fillRect(7.5, 0, 3.5, 12);
                    } else {
                        c.beginPath(); c.moveTo(2, 0); c.lineTo(12, 6); c.lineTo(2, 12); c.closePath(); c.fill();
                    }
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: player.playbackState === MediaPlayer.PlayingState ? player.pause() : player.play()
            }
        }

        Rectangle {
            id: track
            height: 4; radius: 2
            color: root.trackColor
            anchors { left: play.right; leftMargin: 12; right: time.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
            Rectangle {
                height: parent.height; radius: 2
                color: root.accentColor
                width: player.duration > 0 ? parent.width * player.position / player.duration : 0
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -8
                onClicked: (mouse) => { if (player.seekable) player.position = player.duration * Math.max(0, Math.min(1, mouse.x / track.width)) }
            }
        }

        Text {
            id: time
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            color: root.secondaryColor
            font.pixelSize: 12
            function two(n) { return n < 10 ? "0" + n : "" + n }
            function clock(ms) { var s = Math.floor(ms / 1000); return Math.floor(s / 60) + ":" + two(s % 60) }
            text: clock(player.position) + " / " + clock(player.duration)
        }
    }
}
)QML";

using CreateFunction = QObject *(KPluginFactory::*)(const char *, QWidget *, QObject *, const QVariantList &);

// KPluginFactory::create(iface, ...) is protected; the read-only flavour of
// a part is only reachable by naming the interface, so take the member
// through a derived class (the standard, well-defined way to do it).
struct FactoryAccess : KPluginFactory {
    static CreateFunction createFunction()
    {
        return static_cast<CreateFunction>(&FactoryAccess::create);
    }
};

PartWidgetResolver s_partWidgetResolver = nullptr;

// Okular's settings for the pane, apart from the Okular app's: no
// "Welcome" bubble, no banner about embedded files, no page bar. Only keys
// the user never set are written.
void prepareOkularSettings()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("raye-files-previewrc"), KConfig::SimpleConfig);
    KConfigGroup general = config->group(QStringLiteral("General"));
    if (!general.hasKey("ShowOSD")) {
        general.writeEntry("ShowOSD", false);
    }
    if (!general.hasKey("ShowEmbeddedContentMessages")) {
        general.writeEntry("ShowEmbeddedContentMessages", false);
    }
    KConfigGroup view = config->group(QStringLiteral("Main View"));
    if (!view.hasKey("ShowBottomBar")) {
        view.writeEntry("ShowBottomBar", false);
    }
    config->sync();
}

QVariantList partArguments(const QString &partId)
{
    if (partId == QLatin1String("okularpart")) {
        prepareOkularSettings();
        // The bare page view, and settings apart from the Okular app's.
        return {QStringLiteral("ViewerWidget"), QStringLiteral("ConfigFileName=raye-files-previewrc")};
    }
    return {};
}
}

void setPartWidgetResolver(PartWidgetResolver resolver)
{
    s_partWidgetResolver = resolver;
}

PreviewPane::PreviewPane(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("RayePreviewPane"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack);

    m_message = new QLabel(this);
    m_message->setAlignment(Qt::AlignCenter);
    m_message->setWordWrap(true);
    m_message->setFont(Raye::Ui::textFont(Raye::Ui::TextStyle::Body));
    QPalette pal = m_message->palette();
    pal.setColor(QPalette::WindowText, Raye::Ui::Tokens::current().color(Raye::Ui::TokenSet::textSecondary));
    m_message->setPalette(pal);
    m_stack->addWidget(m_message);

    m_image = new ImageView(this);
    m_stack->addWidget(m_image);

    m_text = new QPlainTextEdit(this);
    m_text->setReadOnly(true);
    m_text->setFrameShape(QFrame::NoFrame);
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono(QStringLiteral("Cascadia Mono"));
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSizeF(9.75);
    m_text->setFont(mono);
    m_stack->addWidget(m_text);

    m_partPage = new QWidget(this);
    auto *partLayout = new QVBoxLayout(m_partPage);
    partLayout->setContentsMargins(0, 0, 0, 0);
    m_stack->addWidget(m_partPage);

    m_font = new FontView(this);
    m_stack->addWidget(m_font);

    m_thumbnail = new QLabel(this);
    m_thumbnail->setAlignment(Qt::AlignCenter);
    m_stack->addWidget(m_thumbnail);

    clear();
}

PreviewPane::~PreviewPane()
{
    stopMedia();
    closeParts();
}

KFileItem PreviewPane::item() const
{
    return m_item;
}

Kind PreviewPane::currentKind() const
{
    return m_kind;
}

QString PreviewPane::message() const
{
    return m_stack->currentWidget() == m_message ? m_message->text() : QString();
}

qint64 PreviewPane::lastLoadTime() const
{
    return m_lastLoad;
}

QObject *PreviewPane::currentPart() const
{
    if (m_currentFamily.isEmpty() || m_stack->currentWidget() != m_partPage) {
        return nullptr;
    }
    return m_parts.value(m_currentFamily).part;
}

void PreviewPane::clear()
{
    m_item = KFileItem();
    stopMedia();
    showMessage(i18nc("@info preview pane", "Select a file to preview."));
}

void PreviewPane::showMessage(const QString &text)
{
    m_kind = Kind::None;
    m_message->setText(text);
    m_stack->setCurrentWidget(m_message);
}

void PreviewPane::finishLoad()
{
    m_lastLoad = m_clock.elapsed();
    // The spec's budget is 200 ms; anything slower is worth a line in the log.
    if (m_lastLoad > 200) {
        qCInfo(RayePreview) << "preview of" << m_item.url().fileName() << "took" << m_lastLoad << "ms";
    } else {
        qCDebug(RayePreview) << "preview of" << m_item.url().fileName() << "took" << m_lastLoad << "ms";
    }
    Q_EMIT loaded(m_lastLoad);
}

void PreviewPane::showItem(const KFileItem &item)
{
    if (item.isNull() || item.isDir()) {
        clear();
        return;
    }
    if (!m_item.isNull() && item.url() == m_item.url() && item.time(KFileItem::ModificationTime) == m_item.time(KFileItem::ModificationTime)) {
        return;
    }
    m_clock.start();
    m_item = item;
    if (m_previewJob) {
        m_previewJob->kill();
    }
    stopMedia();

    const Loader loader = loaderFor(item.mimetype());
    const bool local = item.isLocalFile() && !item.localPath().isEmpty();
    const QString path = item.localPath();
    m_kind = loader.kind;

    switch (loader.kind) {
    case Kind::Document:
    case Kind::Text:
        if (!loader.partId.isEmpty() && showPart(loader, item.targetUrl())) {
            break;
        }
        if (loader.kind == Kind::Text && local && showPlainText(path)) {
            m_kind = Kind::Text;
            break;
        }
        showThumbnail(item);
        break;
    case Kind::Image:
        if (local) {
            showImage(path);
        } else {
            showThumbnail(item);
        }
        break;
    case Kind::Audio:
    case Kind::Video:
        showMedia(item.targetUrl(), loader.kind == Kind::Video);
        break;
    case Kind::Font:
        if (local) {
            showFont(path);
        } else {
            showThumbnail(item);
        }
        break;
    case Kind::Thumbnail:
        showThumbnail(item);
        break;
    case Kind::None:
        showMessage(i18nc("@info preview pane", "No preview available."));
        break;
    }
    if (m_kind != Kind::Thumbnail) {
        finishLoad();
    }
}

PreviewPane::PartSlot *PreviewPane::partFor(const Loader &loader)
{
    auto it = m_parts.find(loader.family);
    if (it != m_parts.end() && it->part) {
        return &it.value();
    }
    const auto metaData = KPluginMetaData::findPluginById(QStringLiteral("kf6/parts"), loader.partId);
    if (!metaData.isValid()) {
        return nullptr;
    }
    const auto result = KPluginFactory::loadFactory(metaData);
    if (!result.plugin) {
        qCWarning(RayePreview) << "cannot load" << loader.partId << result.errorString;
        return nullptr;
    }
    PartSlot slot;
    slot.container = new QWidget(m_partPage);
    auto *layout = new QVBoxLayout(slot.container);
    layout->setContentsMargins(0, 0, 0, 0);
    slot.part = (result.plugin->*FactoryAccess::createFunction())("KParts::ReadOnlyPart", slot.container, this, partArguments(loader.partId));
    if (!slot.part) {
        delete slot.container;
        return nullptr;
    }
    // The part's widget: asked of KParts when we can, else the child the
    // part made in the container we gave it.
    QWidget *widget = s_partWidgetResolver ? s_partWidgetResolver(slot.part) : nullptr;
    if (widget) {
        widget->setParent(slot.container);
        layout->addWidget(widget);
        widget->show();
    } else {
        const auto children = slot.container->findChildren<QWidget *>(Qt::FindDirectChildrenOnly);
        for (QWidget *child : children) {
            layout->addWidget(child);
        }
    }
    m_partPage->layout()->addWidget(slot.container);
    slot.container->hide();
    connect(slot.part, &QObject::destroyed, slot.container, &QWidget::deleteLater);
    m_parts.insert(loader.family, slot);
    return &m_parts[loader.family];
}

bool PreviewPane::showPart(const Loader &loader, const QUrl &url)
{
    PartSlot *slot = partFor(loader);
    if (!slot) {
        return false;
    }
    bool ok = false;
    QMetaObject::invokeMethod(slot->part, "openUrl", Qt::DirectConnection, Q_RETURN_ARG(bool, ok), Q_ARG(QUrl, url));
    if (!ok) {
        return false;
    }
    for (auto it = m_parts.begin(); it != m_parts.end(); ++it) {
        if (it->container) {
            it->container->setVisible(it.key() == loader.family);
        }
    }
    m_currentFamily = loader.family;
    m_stack->setCurrentWidget(m_partPage);
    return true;
}

void PreviewPane::showImage(const QString &path)
{
    m_stack->setCurrentWidget(m_image);
    m_image->setPath(path);
    if (m_image->isNull()) {
        showThumbnail(m_item);
    }
}

bool PreviewPane::showPlainText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = file.read(maxPlainTextBytes);
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString text = decoder(bytes);
    if (decoder.hasError()) {
        text = QString::fromLatin1(bytes);
    }
    m_text->setPlainText(text);
    m_stack->setCurrentWidget(m_text);
    return true;
}

void PreviewPane::showMedia(const QUrl &url, bool video)
{
    if (!m_media) {
        m_media = new QQuickWidget(this);
        m_media->setResizeMode(QQuickWidget::SizeRootObjectToView);
        m_media->setClearColor(Qt::transparent);
        m_media->setAttribute(Qt::WA_AlwaysStackOnTop);
        QQmlComponent component(m_media->engine());
        component.setData(QByteArray(mediaQml), QUrl(QStringLiteral("qrc:/raye/preview/Media.qml")));
        if (component.isError()) {
            qCWarning(RayePreview) << component.errorString();
        }
        auto *root = qobject_cast<QQuickItem *>(component.create(m_media->rootContext()));
        if (root) {
            m_media->setContent(component.url(), &component, root);
        }
        m_stack->addWidget(m_media);
    }
    QQuickItem *root = m_media->rootObject();
    if (!root) {
        showMessage(i18nc("@info preview pane", "No preview available."));
        return;
    }
    const Raye::Ui::TokenSet &t = Raye::Ui::Tokens::current();
    root->setProperty("textColor", t.color(Raye::Ui::TokenSet::textPrimary));
    root->setProperty("secondaryColor", t.color(Raye::Ui::TokenSet::textSecondary));
    root->setProperty("accentColor", t.color(Raye::Ui::TokenSet::accent));
    root->setProperty("trackColor", t.color(Raye::Ui::TokenSet::strokeDivider));
    root->setProperty("video", video);
    root->setProperty("source", url);
    m_stack->setCurrentWidget(m_media);
}

void PreviewPane::stopMedia()
{
    if (m_media && m_media->rootObject()) {
        QMetaObject::invokeMethod(m_media->rootObject(), "stop");
    }
}

void PreviewPane::showFont(const QString &path)
{
    if (m_font->setPath(path)) {
        m_stack->setCurrentWidget(m_font);
    } else {
        showMessage(i18nc("@info preview pane", "No preview available."));
    }
}

void PreviewPane::showThumbnail(const KFileItem &item)
{
    m_kind = Kind::Thumbnail;
    m_thumbnail->clear();
    m_stack->setCurrentWidget(m_thumbnail);
    const KConfigGroup globalConfig(KSharedConfig::openConfig(QStringLiteral("dolphinrc")), QStringLiteral("PreviewSettings"));
    const QStringList plugins = globalConfig.readEntry("Plugins", KIO::PreviewJob::defaultPlugins());
    const QSize size = (m_stack->size().isEmpty() ? QSize(320, 320) : m_stack->size()).boundedTo(QSize(512, 512));
    m_previewJob = new KIO::PreviewJob(KFileItemList{item}, size, &plugins);
    m_previewJob->setScaleType(KIO::PreviewJob::Scaled);
    m_previewJob->setDevicePixelRatio(devicePixelRatioF());
    connect(m_previewJob.data(), &KIO::PreviewJob::gotPreview, this, [this](const KFileItem &, const QPixmap &pixmap) {
        m_thumbnail->setPixmap(pixmap);
        finishLoad();
    });
    connect(m_previewJob.data(), &KIO::PreviewJob::failed, this, [this](const KFileItem &) {
        showMessage(i18nc("@info preview pane", "No preview available."));
        finishLoad();
    });
}

void PreviewPane::closeParts()
{
    for (auto it = m_parts.begin(); it != m_parts.end(); ++it) {
        // A read-only part has nothing to save: deleting it closes the file.
        delete it->part;
    }
    m_parts.clear();
}

}
