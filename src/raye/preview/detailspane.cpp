/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "detailspane.h"

#include "ui/commandbutton.h"
#include "ui/tokens.h"

#include <KConfigGroup>
#include <KIO/PreviewJob>
#include <KIconEngine>
#include <KIconLoader>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QVBoxLayout>

namespace Raye::Preview
{

namespace
{
constexpr int thumbnailMax = 160;

QPixmap rounded(const QPixmap &source, qreal radius)
{
    if (source.isNull()) {
        return source;
    }
    QPixmap out(source.size());
    out.setDevicePixelRatio(source.devicePixelRatio());
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(QPointF(), source.deviceIndependentSize()), radius, radius);
    p.setClipPath(path);
    p.drawPixmap(0, 0, source);
    return out;
}

QLabel *label(const QString &text, Raye::Ui::TextStyle style, Raye::Ui::TokenSet::Role colour, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setFont(Raye::Ui::textFont(style));
    QPalette pal = l->palette();
    pal.setColor(QPalette::WindowText, Raye::Ui::Tokens::current().color(colour));
    l->setPalette(pal);
    l->setWordWrap(true);
    return l;
}
}

DetailsPane::DetailsPane(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("RayeDetailsPane"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    outer->addWidget(scroll);

    auto *content = new QWidget(scroll);
    content->setAutoFillBackground(false);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(0);

    m_thumbnail = new QLabel(content);
    m_thumbnail->setAlignment(Qt::AlignCenter);
    m_thumbnail->setMinimumHeight(thumbnailMax);
    layout->addWidget(m_thumbnail);
    layout->addSpacing(12);

    m_title = label(QString(), Raye::Ui::TextStyle::BodyStrong, Raye::Ui::TokenSet::textPrimary, content);
    m_title->setAlignment(Qt::AlignHCenter);
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_title);
    layout->addSpacing(12);

    m_buttons = new QWidget(content);
    auto *buttons = new QHBoxLayout(m_buttons);
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(8);
    buttons->addStretch(1);
    auto *share = new Raye::Ui::CommandButton(QIcon(new KIconEngine(QStringLiteral("document-share"), KIconLoader::global())), i18nc("@action:button", "Share"), m_buttons);
    share->setKind(Raye::Ui::CommandButton::Standard);
    connect(share, &QAbstractButton::clicked, this, [this] {
        Q_EMIT shareRequested(m_selection);
    });
    auto *open = new Raye::Ui::CommandButton(QIcon(new KIconEngine(QStringLiteral("document-open"), KIconLoader::global())), i18nc("@action:button", "Open"), m_buttons);
    open->setKind(Raye::Ui::CommandButton::Standard);
    connect(open, &QAbstractButton::clicked, this, [this] {
        Q_EMIT openRequested(m_selection);
    });
    buttons->addWidget(share);
    buttons->addWidget(open);
    buttons->addStretch(1);
    layout->addWidget(m_buttons);
    layout->addSpacing(20);

    m_propertiesTitle = label(i18nc("@title details pane", "Properties"), Raye::Ui::TextStyle::BodyStrong, Raye::Ui::TokenSet::textPrimary, content);
    layout->addWidget(m_propertiesTitle);
    layout->addSpacing(8);

    m_rows = new QWidget(content);
    m_rowsLayout = new QVBoxLayout(m_rows);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(12);
    layout->addWidget(m_rows);
    layout->addStretch(1);

    scroll->setWidget(content);
    setMinimumWidth(240);
    setMaximumWidth(480);
}

DetailsPane::~DetailsPane()
{
    if (m_previewJob) {
        m_previewJob->kill();
    }
}

const Details &DetailsPane::details() const
{
    return m_details;
}

void DetailsPane::showItems(const KFileItemList &selection, const KFileItem &folder)
{
    m_selection = selection;
    m_details = detailsFor(selection, folder);
    m_title->setText(m_details.title);
    m_buttons->setVisible(selection.size() == 1);
    m_propertiesTitle->setVisible(!m_details.rows.isEmpty());
    rebuildRows();

    if (m_previewJob) {
        m_previewJob->kill();
    }
    const KFileItem item = selection.size() == 1 ? selection.first() : (selection.isEmpty() ? folder : KFileItem());
    if (item.isNull()) {
        setIcon(QStringLiteral("document-multiple"));
    } else if (item.isDir()) {
        setIcon(item.iconName());
    } else {
        setIcon(item.iconName());
        startThumbnail(item);
    }
}

void DetailsPane::rebuildRows()
{
    while (QLayoutItem *child = m_rowsLayout->takeAt(0)) {
        delete child->widget();
        delete child;
    }
    for (const DetailRow &row : std::as_const(m_details.rows)) {
        auto *box = new QWidget(m_rows);
        auto *v = new QVBoxLayout(box);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        v->addWidget(label(row.label, Raye::Ui::TextStyle::Caption, Raye::Ui::TokenSet::textSecondary, box));
        QLabel *value = label(QString(), Raye::Ui::TextStyle::Body, Raye::Ui::TokenSet::textPrimary, box);
        if (row.link.isValid()) {
            // A link styled with the accent; the text is escaped, it may hold anything.
            const QColor accent = Raye::Ui::Tokens::current().color(Raye::Ui::TokenSet::accentText);
            value->setTextFormat(Qt::RichText);
            value->setText(QStringLiteral("<a href=\"%1\" style=\"color:%2; text-decoration:none\">%3</a>")
                               .arg(QString::fromUtf8(row.link.toEncoded()).toHtmlEscaped(), accent.name(), row.value.toHtmlEscaped()));
            value->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
            const QUrl link = row.link;
            connect(value, &QLabel::linkActivated, this, [this, link] {
                Q_EMIT urlActivated(link);
            });
        } else {
            value->setTextFormat(Qt::PlainText);
            value->setText(row.value);
            value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        }
        v->addWidget(value);
        m_rowsLayout->addWidget(box);
    }
}

void DetailsPane::setIcon(const QString &iconName)
{
    const QIcon icon(new KIconEngine(iconName.isEmpty() ? QStringLiteral("unknown") : iconName, KIconLoader::global()));
    m_thumbnail->setPixmap(icon.pixmap(QSize(128, 128), devicePixelRatioF()));
}

void DetailsPane::startThumbnail(const KFileItem &item)
{
    const KConfigGroup globalConfig(KSharedConfig::openConfig(QStringLiteral("dolphinrc")), QStringLiteral("PreviewSettings"));
    const QStringList plugins = globalConfig.readEntry("Plugins", KIO::PreviewJob::defaultPlugins());
    m_previewJob = new KIO::PreviewJob(KFileItemList{item}, QSize(thumbnailMax, thumbnailMax), &plugins);
    m_previewJob->setScaleType(KIO::PreviewJob::Scaled);
    m_previewJob->setDevicePixelRatio(devicePixelRatioF());
    connect(m_previewJob.data(), &KIO::PreviewJob::gotPreview, this, [this](const KFileItem &, const QPixmap &pixmap) {
        m_thumbnail->setPixmap(rounded(pixmap, Raye::Ui::Metrics::radiusOverlay));
    });
}

}
