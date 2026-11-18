/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "jobsmodel.h"

#include "askuserlogic.h"

#include <KFormat>
#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KJob>
#include <KLocalizedString>

#include <QDir>
#include <QLocale>

namespace Raye::Jobs
{

QString headline(Operation op, int items, const QString &from, const QString &to, const QString &otherTitle)
{
    const int n = qMax(1, items);
    const bool hasFrom = !from.isEmpty();
    const bool hasTo = !to.isEmpty();
    switch (op) {
    case Operation::Copy:
        if (hasFrom && hasTo) {
            return i18ncp("@info", "Copying %1 item from %2 to %3", "Copying %1 items from %2 to %3", n, from, to);
        }
        if (hasTo) {
            return i18ncp("@info", "Copying %1 item to %2", "Copying %1 items to %2", n, to);
        }
        return i18ncp("@info", "Copying %1 item", "Copying %1 items", n);
    case Operation::Move:
        if (hasFrom && hasTo) {
            return i18ncp("@info", "Moving %1 item from %2 to %3", "Moving %1 items from %2 to %3", n, from, to);
        }
        if (hasTo) {
            return i18ncp("@info", "Moving %1 item to %2", "Moving %1 items to %2", n, to);
        }
        return i18ncp("@info", "Moving %1 item", "Moving %1 items", n);
    case Operation::Link:
        if (hasTo) {
            return i18ncp("@info", "Creating %1 shortcut in %2", "Creating %1 shortcuts in %2", n, to);
        }
        return i18ncp("@info", "Creating %1 shortcut", "Creating %1 shortcuts", n);
    case Operation::Trash:
        if (hasFrom) {
            return i18ncp("@info", "Deleting %1 item from %2", "Deleting %1 items from %2", n, from);
        }
        return i18ncp("@info", "Deleting %1 item", "Deleting %1 items", n);
    case Operation::Delete:
        if (hasFrom) {
            return i18ncp("@info", "Permanently deleting %1 item from %2", "Permanently deleting %1 items from %2", n, from);
        }
        return i18ncp("@info", "Permanently deleting %1 item", "Permanently deleting %1 items", n);
    case Operation::Other:
        break;
    }
    return otherTitle.isEmpty() ? i18nc("@info", "Working on your files") : otherTitle;
}

QString folderName(const QUrl &folder)
{
    if (!folder.isValid() || folder.isEmpty()) {
        return QString();
    }
    if (folder.scheme() == QLatin1String("trash")) {
        return i18nc("@label the trash", "Recycle Bin");
    }
    if (folder.isLocalFile()) {
        const QString path = QDir::cleanPath(folder.toLocalFile());
        if (path == QDir::homePath()) {
            return i18nc("@label the home folder", "Home");
        }
        if (path == QLatin1String("/")) {
            return i18nc("@label the whole computer", "This PC");
        }
    }
    const QString name = folder.adjusted(QUrl::StripTrailingSlash).fileName();
    return name.isEmpty() ? folder.host() : name;
}

QString itemsRemainingText(qulonglong items, qulonglong bytes)
{
    const QString count = QLocale().toString(items);
    if (bytes == 0) {
        return count;
    }
    return i18nc("@info items left, then their size", "%1 (%2)", count, KFormat().formatByteSize(double(bytes), 1, KFormat::JEDECBinaryDialect));
}

JobsModel::JobsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

JobsModel::~JobsModel() = default;

int JobsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QHash<int, QByteArray> JobsModel::roleNames() const
{
    return {
        {HeadlineRole, "headline"},
        {PercentRole, "percent"},
        {PercentTextRole, "percentText"},
        {PausedRole, "paused"},
        {CanPauseRole, "canPause"},
        {SpeedTextRole, "speedText"},
        {SpeedRole, "speed"},
        {HistoryRole, "history"},
        {PeakRole, "peak"},
        {NameRole, "name"},
        {RemainingRole, "remaining"},
        {ItemsRole, "items"},
        {ErrorRole, "error"},
        {ErrorCanRetryRole, "errorCanRetry"},
        {ErrorMultipleRole, "errorMultiple"},
        {DoneRole, "done"},
        {FinishingRole, "finishing"},
    };
}

QVariant JobsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= int(m_rows.size())) {
        return {};
    }
    const Row &r = m_rows[index.row()];
    const qulonglong items = r.itemsTotal ? r.itemsTotal : qulonglong(r.sourceCount);
    const qreal speed = r.probe ? r.probe->sampler().speed() : r.fixedSpeed;
    switch (role) {
    case HeadlineRole:
        return headline(r.op, int(items), folderName(r.from), folderName(r.to), r.otherTitle);
    case PercentRole:
        return r.done ? 100 : r.percent;
    case PercentTextRole:
        if (r.paused) {
            return i18nc("@info progress", "Paused - %1% complete", r.percent);
        }
        if (r.finishing) {
            return i18nc("@info progress, the copy is over and the drive is still being written", "Finishing writing - %1% complete", r.percent);
        }
        return i18nc("@info progress", "%1% complete", r.done ? 100 : r.percent);
    case PausedRole:
        return r.paused;
    case CanPauseRole:
        return r.canPause;
    case SpeedRole:
        return speed;
    case SpeedTextRole: {
        const QString s = speedText(speed);
        return s.isEmpty() ? QString() : i18nc("@info", "Speed: %1", s);
    }
    case HistoryRole: {
        const QList<qreal> h = r.probe ? r.probe->sampler().history() : r.fixedHistory;
        QVariantList list;
        list.reserve(h.size());
        for (qreal v : h) {
            list.append(v);
        }
        return list;
    }
    case PeakRole: {
        qreal top = r.probe ? r.probe->sampler().peak() : 0;
        for (qreal v : r.fixedHistory) {
            top = qMax(top, v);
        }
        return top;
    }
    case NameRole:
        return r.currentName;
    case RemainingRole:
        if (r.paused || r.finishing) {
            return QString();
        }
        return remainingText(r.probe ? r.probe->sampler().remainingMs() : r.fixedRemaining);
    case ItemsRole: {
        const qulonglong leftItems = items > r.itemsDone ? items - r.itemsDone : 0;
        const qulonglong leftBytes = r.bytesTotal > r.bytesDone ? r.bytesTotal - r.bytesDone : 0;
        return itemsRemainingText(leftItems, leftBytes);
    }
    case ErrorRole:
        return r.error;
    case ErrorCanRetryRole:
        return r.errorCanRetry;
    case ErrorMultipleRole:
        return r.errorMultiple;
    case DoneRole:
        return r.done;
    case FinishingRole:
        return r.finishing;
    }
    return {};
}

void JobsModel::addJob(KJob *job)
{
    Row r;
    r.job = job;
    r.canPause = job->capabilities() & KJob::Suspendable;
    if (auto *copy = qobject_cast<KIO::CopyJob *>(job)) {
        const QList<QUrl> sources = copy->srcUrls();
        r.sourceCount = sources.size();
        r.from = sources.isEmpty() ? QUrl() : sources.first().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
        r.to = copy->destUrl();
        switch (copy->operationMode()) {
        case KIO::CopyJob::Copy:
            r.op = Operation::Copy;
            break;
        case KIO::CopyJob::Move:
            r.op = r.to.scheme() == QLatin1String("trash") ? Operation::Trash : Operation::Move;
            break;
        case KIO::CopyJob::Link:
            r.op = Operation::Link;
            break;
        }
        if (r.from == r.to.adjusted(QUrl::StripTrailingSlash)) {
            r.from.clear();
        }
    } else if (auto *del = qobject_cast<KIO::DeleteJob *>(job)) {
        const QList<QUrl> urls = del->urls();
        r.op = Operation::Delete;
        r.sourceCount = urls.size();
        r.from = urls.isEmpty() ? QUrl() : urls.first().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    }
    r.probe = std::make_shared<JobSpeedProbe>(job);
    connect(r.probe.get(), &JobSpeedProbe::sampled, this, [this, job] {
        const int i = rowOf(job);
        if (i >= 0) {
            touch(i);
        }
    });
    addRow(r);
}

void JobsModel::addRow(const Row &row)
{
    const int at = int(m_rows.size());
    beginInsertRows({}, at, at);
    m_rows.push_back(row);
    m_rows.back().id = m_nextId++;
    endInsertRows();
    Q_EMIT countChanged();
    Q_EMIT overallChanged();
}

int JobsModel::rowOf(const KJob *job) const
{
    for (int i = 0; i < int(m_rows.size()); ++i) {
        if (m_rows[i].job == job) {
            return i;
        }
    }
    return -1;
}

int JobsModel::rowOfId(quint64 id) const
{
    for (int i = 0; i < int(m_rows.size()); ++i) {
        if (m_rows[i].id == id) {
            return i;
        }
    }
    return -1;
}

JobsModel::Row *JobsModel::row(int i)
{
    return i >= 0 && i < int(m_rows.size()) ? &m_rows[i] : nullptr;
}

void JobsModel::touch(int i)
{
    const QModelIndex idx = index(i);
    Q_EMIT dataChanged(idx, idx);
    Q_EMIT overallChanged();
}

void JobsModel::removeRow(int i)
{
    if (i < 0 || i >= int(m_rows.size())) {
        return;
    }
    beginRemoveRows({}, i, i);
    m_rows.erase(m_rows.begin() + i);
    endRemoveRows();
    Q_EMIT countChanged();
    Q_EMIT overallChanged();
}

int JobsModel::overallPercent() const
{
    qulonglong total = 0;
    qulonglong done = 0;
    int percents = 0;
    for (const Row &r : m_rows) {
        total += r.bytesTotal;
        done += qMin(r.bytesDone, r.bytesTotal);
        percents += r.done ? 100 : r.percent;
    }
    if (total > 0) {
        return int(done * 100 / total);
    }
    return m_rows.empty() ? 0 : percents / int(m_rows.size());
}

bool JobsModel::showError(KJob *job, const QString &text, bool canRetry, bool multiple, const std::function<void(const SkipAnswer &)> &answer)
{
    const int i = rowOf(job);
    if (i < 0) {
        return false;
    }
    Row &r = m_rows[i];
    r.error = text;
    r.errorCanRetry = canRetry;
    r.errorMultiple = multiple;
    r.errorAnswer = answer;
    touch(i);
    return true;
}

void JobsModel::pause(int i)
{
    Row *r = row(i);
    if (!r || !r->job || !r->canPause) {
        return;
    }
    if (r->paused) {
        r->job->resume();
    } else {
        r->job->suspend();
    }
}

void JobsModel::cancel(int i)
{
    Row *r = row(i);
    if (!r) {
        return;
    }
    if (r->errorAnswer) {
        // A question is open: cancelling answers it.
        answerError(i, 2, false);
    }
    if (r->job) {
        r->job->kill(KJob::EmitResult);
    }
}

void JobsModel::answerError(int i, int choice, bool applyToAll)
{
    Row *r = row(i);
    if (!r || !r->errorAnswer) {
        return;
    }
    SkipAnswer a;
    a.choice = choice == 0 ? SkipChoice::TryAgain : choice == 1 ? SkipChoice::Skip : SkipChoice::Cancel;
    a.applyToAll = applyToAll;
    auto answer = std::move(r->errorAnswer);
    r->errorAnswer = nullptr;
    r->error.clear();
    touch(i);
    answer(a);
}

}
