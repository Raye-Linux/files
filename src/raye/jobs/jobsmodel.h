/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "speedsampler.h"

#include <QAbstractListModel>
#include <QPointer>
#include <QUrl>

#include <functional>
#include <memory>
#include <vector>

class KJob;

namespace Raye::Jobs
{

struct SkipAnswer;

// What an operation is doing, for its headline.
enum class Operation {
    Copy,
    Move,
    Link,
    Delete,
    Trash,
    Other,
};

// "Copying 1,204 items from Downloads to USB Drive" (explorer spec 1.13).
// from/to are folder names; an empty one drops its part.
DOLPHIN_EXPORT QString headline(Operation op, int items, const QString &from, const QString &to, const QString &otherTitle = QString());
// The name Files shows for a folder in a headline: "Recycle Bin" for
// trash:/, "Home" for the home folder, else the folder's own name.
DOLPHIN_EXPORT QString folderName(const QUrl &folder);
// "120 (1.2 GB)"
DOLPHIN_EXPORT QString itemsRemainingText(qulonglong items, qulonglong bytes);

// The operations of the progress window, one row each. A row follows a
// KJob (the tracker feeds it) or carries fixed values (the harness).
class DOLPHIN_EXPORT JobsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int overallPercent READ overallPercent NOTIFY overallChanged)

public:
    enum Roles {
        HeadlineRole = Qt::UserRole + 1,
        PercentRole,
        PercentTextRole,
        PausedRole,
        CanPauseRole,
        SpeedTextRole,
        SpeedRole,
        HistoryRole,
        PeakRole,
        NameRole,
        RemainingRole,
        ItemsRole,
        ErrorRole,
        ErrorCanRetryRole,
        ErrorMultipleRole,
        DoneRole,
        FinishingRole,
    };

    struct Row {
        quint64 id = 0; // set by addRow
        QPointer<KJob> job;
        Operation op = Operation::Other;
        QString otherTitle;
        QUrl from;
        QUrl to;
        int sourceCount = 0;
        qulonglong itemsTotal = 0;
        qulonglong itemsDone = 0;
        qulonglong bytesTotal = 0;
        qulonglong bytesDone = 0;
        int percent = 0;
        bool paused = false;
        bool canPause = false;
        QString currentName;
        bool done = false;
        // a copy to a USB drive: the card stays until the drive has it all
        bool flushes = false;
        bool finishing = false;
        QString flushPath;
        qulonglong pendingBase = 0;
        qulonglong pending = 0;
        // fixed values for the harness (no job)
        QList<qreal> fixedHistory;
        qreal fixedSpeed = 0;
        qint64 fixedRemaining = -1;
        // inline error
        QString error;
        bool errorCanRetry = true;
        bool errorMultiple = false;
        std::function<void(const SkipAnswer &)> errorAnswer;
        std::shared_ptr<JobSpeedProbe> probe;
    };

    explicit JobsModel(QObject *parent = nullptr);
    ~JobsModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // A job the tracker follows; fills op, from, to from the job's kind.
    void addJob(KJob *job);
    void addRow(const Row &row);
    int rowOf(const KJob *job) const;
    int rowOfId(quint64 id) const;
    Row *row(int i);
    // Something about the row changed: tell the views.
    void touch(int i);
    void removeRow(int i);

    int overallPercent() const;
    // True when a job's row exists and is shown (errors go there).
    bool showError(KJob *job, const QString &text, bool canRetry, bool multiple, const std::function<void(const SkipAnswer &)> &answer);

    Q_INVOKABLE void pause(int row);
    Q_INVOKABLE void cancel(int row);
    // choice: 0 try again, 1 skip, 2 cancel
    Q_INVOKABLE void answerError(int row, int choice, bool applyToAll);

Q_SIGNALS:
    void countChanged();
    void overallChanged();

private:
    std::vector<Row> m_rows;
    quint64 m_nextId = 1;
};

}
