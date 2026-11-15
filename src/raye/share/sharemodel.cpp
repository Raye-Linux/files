/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharemodel.h"
#include "sharetargets.h"

#include <KLocalizedString>

#include <QAbstractItemModel>
#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QMimeData>
#include <QMimeDatabase>
#include <QQmlComponent>
#include <QQmlEngine>

#include <algorithm>

namespace Raye::Share
{
namespace
{
QVariantMap entry(const QString &id, const Target &target, int row)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("name"), target.name},
            {QStringLiteral("icon"), target.iconName},
            {QStringLiteral("glyph"), target.glyph},
            {QStringLiteral("row"), row}};
}

// The type the plugins are told: the files' own when they agree, else the
// common group ("image/*"), else anything.
QString commonMimeType(const QStringList &mimeTypes)
{
    if (mimeTypes.isEmpty()) {
        return QStringLiteral("application/octet-stream");
    }
    const QString first = mimeTypes.first();
    if (std::all_of(mimeTypes.cbegin(), mimeTypes.cend(), [&first](const QString &m) {
            return m == first;
        })) {
        return first;
    }
    const QString group = first.section(QLatin1Char('/'), 0, 0);
    if (std::all_of(mimeTypes.cbegin(), mimeTypes.cend(), [&group](const QString &m) {
            return m.section(QLatin1Char('/'), 0, 0) == group;
        })) {
        return group + QStringLiteral("/*");
    }
    return QStringLiteral("application/octet-stream");
}
}

ShareModel::ShareModel(QObject *parent)
    : QObject(parent)
{
    rebuild();
}

ShareModel::~ShareModel() = default;

void ShareModel::setUrls(const QList<QUrl> &urls, const QStringList &mimeTypes)
{
    m_urls = urls;
    m_mimeTypes = mimeTypes;
    if (m_mimeTypes.size() != m_urls.size()) {
        m_mimeTypes.clear();
        const QMimeDatabase db;
        for (const QUrl &url : urls) {
            m_mimeTypes << (url.isLocalFile() ? db.mimeTypeForFile(url.toLocalFile()) : db.mimeTypeForUrl(url)).name();
        }
    }
    rebuild();
}

void ShareModel::setSource(QAbstractItemModel *source)
{
    if (m_source) {
        m_source->disconnect(this);
    }
    m_source = source;
    if (source) {
        connect(source, &QAbstractItemModel::modelReset, this, &ShareModel::rebuild);
        connect(source, &QAbstractItemModel::rowsInserted, this, &ShareModel::rebuild);
        connect(source, &QAbstractItemModel::rowsRemoved, this, &ShareModel::rebuild);
        connect(source, &QAbstractItemModel::dataChanged, this, &ShareModel::rebuild);
        connect(source, &QObject::destroyed, this, &ShareModel::rebuild);
    }
    rebuild();
}

void ShareModel::rebuild()
{
    struct Item {
        QString id;
        Target target;
        int row;
    };
    QList<Item> row;
    QList<Item> more;

    if (m_source) {
        for (int i = 0; i < m_source->rowCount(); ++i) {
            const QModelIndex index = m_source->index(i, 0);
            const QString id = index.data(pluginIdRole).toString();
            const Target target = describe(id, index.data(Qt::DisplayRole).toString(), index.data(iconNameRole).toString());
            if (target.place == Place::Row) {
                row.append({id, target, i});
            } else if (target.place == Place::More) {
                more.append({id, target, i});
            }
        }
    }
    if (!m_urls.isEmpty()) {
        row.append({QString::fromLatin1(copyTarget), describe(QString::fromLatin1(copyTarget)), -1});
        const bool remote = std::any_of(m_urls.cbegin(), m_urls.cend(), [](const QUrl &u) {
            return !u.isLocalFile();
        });
        if (remote) {
            row.append({QString::fromLatin1(copyLinkTarget), describe(QString::fromLatin1(copyLinkTarget)), -1});
        }
    }
    std::stable_sort(row.begin(), row.end(), [](const Item &a, const Item &b) {
        return a.target.order < b.target.order;
    });
    std::stable_sort(more.begin(), more.end(), [](const Item &a, const Item &b) {
        return a.target.name.localeAwareCompare(b.target.name) < 0;
    });

    m_row.clear();
    m_more.clear();
    for (const Item &i : std::as_const(row)) {
        m_row << entry(i.id, i.target, i.row);
    }
    for (const Item &i : std::as_const(more)) {
        m_more << entry(i.id, i.target, i.row);
    }
    Q_EMIT changed();
}

QStringList ShareModel::rowIds() const
{
    QStringList out;
    for (const QVariant &v : m_row) {
        out << v.toMap().value(QStringLiteral("id")).toString();
    }
    return out;
}

QStringList ShareModel::moreIds() const
{
    QStringList out;
    for (const QVariant &v : m_more) {
        out << v.toMap().value(QStringLiteral("id")).toString();
    }
    return out;
}

QString ShareModel::title() const
{
    if (m_urls.size() == 1) {
        return m_urls.first().fileName();
    }
    return i18ncp("@info the files being shared", "%1 item", "%1 items", m_urls.size());
}

QString ShareModel::subtitle() const
{
    if (m_urls.isEmpty()) {
        return QString();
    }
    const QMimeDatabase db;
    const QString type = commonMimeType(m_mimeTypes);
    if (type.endsWith(QLatin1String("/*")) || type == QLatin1String("application/octet-stream")) {
        return m_urls.size() == 1 ? db.mimeTypeForName(m_mimeTypes.value(0)).comment() : QString();
    }
    return db.mimeTypeForName(type).comment();
}

QString ShareModel::iconName() const
{
    if (m_urls.size() != 1) {
        return QStringLiteral("document-multiple");
    }
    const QMimeDatabase db;
    const QString icon = db.mimeTypeForName(m_mimeTypes.value(0)).iconName();
    return icon.isEmpty() ? QStringLiteral("unknown") : icon;
}

bool ShareModel::activateOwn(const QString &id)
{
    if (id == QLatin1String(copyTarget)) {
        // The way Files' Copy puts files on the clipboard.
        auto *mime = new QMimeData;
        mime->setUrls(m_urls);
        mime->setData(QStringLiteral("application/x-kde-cutselection"), QByteArrayLiteral("0"));
        QGuiApplication::clipboard()->setMimeData(mime);
        Q_EMIT done();
        return true;
    }
    if (id == QLatin1String(copyLinkTarget)) {
        QStringList links;
        for (const QUrl &u : std::as_const(m_urls)) {
            // Never a password, even if the address carried one.
            links << u.toDisplayString(QUrl::RemovePassword);
        }
        QGuiApplication::clipboard()->setText(links.join(QLatin1Char('\n')));
        Q_EMIT done();
        return true;
    }
    return false;
}

QJsonObject ShareModel::inputData(const QList<QUrl> &urls, const QStringList &mimeTypes)
{
    QJsonArray list;
    for (const QUrl &u : urls) {
        list.append(u.toString());
    }
    return {{QStringLiteral("urls"), list}, {QStringLiteral("mimeType"), commonMimeType(mimeTypes)}};
}

QAbstractItemModel *ShareModel::createPurposeModel(QQmlEngine *engine, const QJsonObject &input, QObject *parent, QString *error)
{
    QQmlComponent component(engine);
    component.setData(QByteArrayLiteral("import org.kde.purpose\nPurposeAlternativesModel { pluginType: \"Export\" }\n"), QUrl());
    QObject *object = component.create();
    auto *model = qobject_cast<QAbstractItemModel *>(object);
    if (!model) {
        delete object;
        if (error) {
            *error = component.errorString();
        }
        return nullptr;
    }
    QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
    model->setParent(parent);
    model->setProperty("inputData", input);
    return model;
}

}
