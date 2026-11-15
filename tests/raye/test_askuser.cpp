/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The answers of the Raye "Replace or skip files", error and delete
// dialogs (explorer spec 3.11, 3.13), checked against what KIO does with
// them: a real KIO::copy into a temporary folder with conflicts, the
// questions answered by a script.

#include "raye/jobs/askuser.h"
#include "raye/jobs/askuserlogic.h"
#include "raye/jobs/jobsmodel.h"
#include "raye/jobs/jobuidelegate.h"
#include "raye/jobs/undobar.h"

#include <KConfigGroup>
#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KJobWidgets>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Raye::Jobs;

namespace
{
// Answers from a list, and remembers what it was asked.
class ScriptedPrompter : public Prompter
{
public:
    QList<ConflictAnswer> conflicts;
    QList<SkipAnswer> skips;
    QList<bool> deletes;
    QList<int> messages;
    QList<ConflictInfo> askedConflicts;
    QList<SkipInfo> askedSkips;
    int askedDeletes = 0;

    void askConflict(KJob *, QWidget *, const ConflictInfo &info, const std::function<void(const ConflictAnswer &)> &done) override
    {
        askedConflicts.append(info);
        done(conflicts.isEmpty() ? ConflictAnswer{} : conflicts.takeFirst());
    }
    void askSkip(KJob *, QWidget *, const SkipInfo &info, const std::function<void(const SkipAnswer &)> &done) override
    {
        askedSkips.append(info);
        done(skips.isEmpty() ? SkipAnswer{} : skips.takeFirst());
    }
    void askDelete(const QList<QUrl> &, DeletionType, QWidget *, const std::function<void(bool)> &done) override
    {
        ++askedDeletes;
        done(deletes.isEmpty() ? false : deletes.takeFirst());
    }
    void askMessage(const MessageInfo &, QWidget *, const std::function<void(int)> &done) override
    {
        done(messages.isEmpty() ? 2 : messages.takeFirst());
    }
};

void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

bool none(const QUrl &)
{
    return false;
}
}

class TestAskUser : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();

    // rules
    void conflictMapping_data();
    void conflictMapping();
    void choicesOffered();
    void keepBothNames_data();
    void keepBothNames();
    void skipMapping();
    void deleteConfirmation();
    void deleteQuestions();
    void messageResults();
    void headlines();
    void undoText();

    // a real KIO::copy
    void copyReplace();
    void copySkip();
    void copySkipSingle();
    void copyKeepBoth();
    void copyApplyToAll();
    void copyAutoSkip();
    void copyCancel();
    void folderMerge();
    void deleteAsks();
    void delegateFindsAskUser();

private:
    // src gets a.txt, b.txt, c.txt; dst already has the same names
    void makeConflicts(int count);
    KJob *copyWith(const std::shared_ptr<ScriptedPrompter> &p, const QList<QUrl> &sources, const QUrl &dest);

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_src;
    QString m_dst;
};

void TestAskUser::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestAskUser::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_src = m_dir->filePath(QStringLiteral("src"));
    m_dst = m_dir->filePath(QStringLiteral("dst"));
    QVERIFY(QDir().mkpath(m_src));
    QVERIFY(QDir().mkpath(m_dst));
}

void TestAskUser::conflictMapping_data()
{
    QTest::addColumn<int>("options");
    QTest::addColumn<int>("choice");
    QTest::addColumn<bool>("all");
    QTest::addColumn<int>("result");

    const int many = KIO::RenameDialog_Overwrite | KIO::RenameDialog_Skip | KIO::RenameDialog_MultipleItems;
    const int one = KIO::RenameDialog_Overwrite;
    QTest::newRow("replace") << one << int(ConflictChoice::Replace) << false << int(KIO::Result_Overwrite);
    QTest::newRow("replace all") << many << int(ConflictChoice::Replace) << true << int(KIO::Result_OverwriteAll);
    // "for all" means nothing with a single item
    QTest::newRow("replace all, one item") << one << int(ConflictChoice::Replace) << true << int(KIO::Result_Overwrite);
    QTest::newRow("skip") << many << int(ConflictChoice::Skip) << false << int(KIO::Result_Skip);
    QTest::newRow("skip all") << many << int(ConflictChoice::Skip) << true << int(KIO::Result_AutoSkip);
    QTest::newRow("skip not offered") << one << int(ConflictChoice::Skip) << false << int(KIO::Result_Cancel);
    QTest::newRow("keep both") << many << int(ConflictChoice::KeepBoth) << false << int(KIO::Result_Rename);
    QTest::newRow("keep both all") << many << int(ConflictChoice::KeepBoth) << true << int(KIO::Result_AutoRename);
    QTest::newRow("keep both, no rename") << int(many | KIO::RenameDialog_NoRename) << int(ConflictChoice::KeepBoth) << false << int(KIO::Result_Cancel);
    QTest::newRow("cancel") << many << int(ConflictChoice::Cancel) << false << int(KIO::Result_Cancel);
    QTest::newRow("onto itself") << int(KIO::RenameDialog_OverwriteItself | KIO::RenameDialog_Overwrite | KIO::RenameDialog_Skip) << int(ConflictChoice::Replace)
                                 << false << int(KIO::Result_Cancel);
    QTest::newRow("merge") << int(many | KIO::RenameDialog_SourceIsDirectory | KIO::RenameDialog_DestIsDirectory) << int(ConflictChoice::Replace) << false
                           << int(KIO::Result_Overwrite);
}

void TestAskUser::conflictMapping()
{
    QFETCH(int, options);
    QFETCH(int, choice);
    QFETCH(bool, all);
    QFETCH(int, result);
    ConflictInfo info;
    info.source = QUrl::fromLocalFile(QStringLiteral("/a/report.pdf"));
    info.destination = QUrl::fromLocalFile(QStringLiteral("/b/report.pdf"));
    info.options = KIO::RenameDialog_Options(options);
    const ConflictResult r = conflictResult(info, {ConflictChoice(choice), all}, none);
    QCOMPARE(int(r.result), result);
    if (r.result == KIO::Result_Rename) {
        QCOMPARE(r.newDestination, QUrl::fromLocalFile(QStringLiteral("/b/report (2).pdf")));
    } else {
        QVERIFY(r.newDestination.isEmpty());
    }
}

void TestAskUser::choicesOffered()
{
    ConflictInfo info;
    info.options = KIO::RenameDialog_Overwrite | KIO::RenameDialog_Skip;
    QVERIFY(info.canReplace() && info.canSkip() && info.canKeepBoth());
    QVERIFY(!info.multiple() && !info.isFolderMerge() && !info.isSameFile());
    info.options |= KIO::RenameDialog_OverwriteItself;
    QVERIFY(info.isSameFile());
    QVERIFY(!info.canReplace());
    info.options = KIO::RenameDialog_SourceIsDirectory | KIO::RenameDialog_DestIsDirectory | KIO::RenameDialog_MultipleItems;
    QVERIFY(info.isFolderMerge());
    QVERIFY(info.multiple());
}

void TestAskUser::keepBothNames_data()
{
    QTest::addColumn<QString>("dest");
    QTest::addColumn<bool>("folder");
    QTest::addColumn<QStringList>("taken");
    QTest::addColumn<QString>("expected");

    QTest::newRow("file") << "/d/a.txt" << false << QStringList() << "/d/a (2).txt";
    QTest::newRow("next free") << "/d/a.txt" << false << QStringList{QStringLiteral("/d/a (2).txt"), QStringLiteral("/d/a (3).txt")} << "/d/a (4).txt";
    QTest::newRow("double extension") << "/d/backup.tar.gz" << false << QStringList() << "/d/backup (2).tar.gz";
    QTest::newRow("folder with a dot") << "/d/v1.2" << true << QStringList() << "/d/v1.2 (2)";
    QTest::newRow("no extension") << "/d/Makefile" << false << QStringList() << "/d/Makefile (2)";
    QTest::newRow("hidden file") << "/d/.bashrc" << false << QStringList() << "/d/.bashrc (2)";
    QTest::newRow("already numbered") << "/d/a (2).txt" << false << QStringList() << "/d/a (3).txt";
    QTest::newRow("spaces") << "/d/my report.final.pdf" << false << QStringList() << "/d/my report.final (2).pdf";
}

void TestAskUser::keepBothNames()
{
    QFETCH(QString, dest);
    QFETCH(bool, folder);
    QFETCH(QStringList, taken);
    QFETCH(QString, expected);
    const QUrl u = keepBothName(QUrl::fromLocalFile(dest), folder, [&taken](const QUrl &url) {
        return taken.contains(url.toLocalFile());
    });
    QCOMPARE(u.toLocalFile(), expected);
}

void TestAskUser::skipMapping()
{
    const auto many = KIO::SkipDialog_Options(KIO::SkipDialog_MultipleItems);
    QCOMPARE(skipResult(many, {SkipChoice::TryAgain, false, false}), KIO::Result_Retry);
    QCOMPARE(skipResult(many, {SkipChoice::Skip, false, false}), KIO::Result_Skip);
    QCOMPARE(skipResult(many, {SkipChoice::Skip, true, false}), KIO::Result_AutoSkip);
    QCOMPARE(skipResult({}, {SkipChoice::Skip, true, false}), KIO::Result_Skip);
    QCOMPARE(skipResult(many, {SkipChoice::Cancel, false, false}), KIO::Result_Cancel);
    QCOMPARE(skipResult(KIO::SkipDialog_Hide_Retry, {SkipChoice::TryAgain, false, false}), KIO::Result_Cancel);
    QCOMPARE(skipResult(KIO::SkipDialog_Replace_Invalid_Chars, {SkipChoice::Skip, false, true}), KIO::Result_ReplaceInvalidChars);
    QCOMPARE(skipResult(KIO::SkipDialog_Replace_Invalid_Chars, {SkipChoice::Skip, true, true}), KIO::Result_ReplaceAllInvalidChars);
    // replace offered only when KIO asks for it
    QCOMPARE(skipResult({}, {SkipChoice::Skip, false, true}), KIO::Result_Skip);
}

void TestAskUser::deleteConfirmation()
{
    using I = KIO::AskUserActionInterface;
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup g = config.group(QStringLiteral("Confirmations"));
    // KIO's defaults: ask before deleting for good, not before the bin
    QVERIFY(needsConfirmation(I::Delete, I::DefaultConfirmation, g));
    QVERIFY(!needsConfirmation(I::Trash, I::DefaultConfirmation, g));
    QVERIFY(needsConfirmation(I::EmptyTrash, I::DefaultConfirmation, g));
    QVERIFY(needsConfirmation(I::DeleteInsteadOfTrash, I::DefaultConfirmation, g));
    QVERIFY(needsConfirmation(I::DeleteNoTrashAvailable, I::DefaultConfirmation, g));
    // kiorc
    g.writeEntry("ConfirmTrash", true);
    g.writeEntry("ConfirmDelete", false);
    QVERIFY(needsConfirmation(I::Trash, I::DefaultConfirmation, g));
    QVERIFY(!needsConfirmation(I::Delete, I::DefaultConfirmation, g));
    // Shift+Delete forces it
    QVERIFY(needsConfirmation(I::Delete, I::ForceConfirmation, g));
}

void TestAskUser::deleteQuestions()
{
    using I = KIO::AskUserActionInterface;
    DeleteQuestion q = deleteQuestion(I::Delete, 1, false);
    QCOMPARE(q.title, QStringLiteral("Delete file"));
    QCOMPARE(q.question, QStringLiteral("Are you sure you want to permanently delete this file?"));
    QCOMPARE(q.yes, QStringLiteral("Yes"));
    q = deleteQuestion(I::Delete, 1, true);
    QCOMPARE(q.title, QStringLiteral("Delete folder"));
    QCOMPARE(q.question, QStringLiteral("Are you sure you want to permanently delete this folder?"));
    q = deleteQuestion(I::Delete, 7, false);
    QCOMPARE(q.title, QStringLiteral("Delete multiple items"));
    QCOMPARE(q.question, QStringLiteral("Are you sure you want to permanently delete these 7 items?"));
    q = deleteQuestion(I::EmptyTrash, 5, false);
    QCOMPARE(q.title, QStringLiteral("Empty Recycle Bin"));
    QCOMPARE(q.question, QStringLiteral("Are you sure you want to permanently delete these 5 items?"));
    q = deleteQuestion(I::Trash, 1, false);
    QVERIFY(q.question.contains(QLatin1String("Recycle Bin")));
    q = deleteQuestion(I::DeleteInsteadOfTrash, 2, false);
    QVERIFY(q.question.contains(QLatin1String("too big")));
    for (int type = I::Delete; type <= I::DeleteNoTrashAvailable; ++type) {
        const DeleteQuestion each = deleteQuestion(I::DeletionType(type), 3, false);
        QVERIFY(!each.question.contains(QLatin1String("Trash")));
        QVERIFY(!each.title.isEmpty());
    }
}

void TestAskUser::messageResults()
{
    using I = KIO::AskUserActionInterface;
    QCOMPARE(messageResult(I::QuestionTwoActions, 0), int(MessagePrimary));
    QCOMPARE(messageResult(I::QuestionTwoActions, 1), int(MessageSecondary));
    QCOMPARE(messageResult(I::QuestionTwoActionsCancel, 2), int(MessageCancel));
    QCOMPARE(messageResult(I::WarningContinueCancel, 0), int(MessageContinue));
    QCOMPARE(messageResult(I::WarningContinueCancel, 2), int(MessageCancel));
    QCOMPARE(messageResult(I::Error, 0), int(MessageOk));
}

void TestAskUser::headlines()
{
    QCOMPARE(headline(Operation::Copy, 1204, QStringLiteral("Downloads"), QStringLiteral("USB Drive")),
             QStringLiteral("Copying 1204 items from Downloads to USB Drive"));
    QCOMPARE(headline(Operation::Copy, 1, QStringLiteral("Downloads"), QStringLiteral("Documents")), QStringLiteral("Copying 1 item from Downloads to Documents"));
    QCOMPARE(headline(Operation::Move, 3, QString(), QStringLiteral("Documents")), QStringLiteral("Moving 3 items to Documents"));
    QCOMPARE(headline(Operation::Trash, 2, QStringLiteral("Pictures"), QString()), QStringLiteral("Deleting 2 items from Pictures"));
    QCOMPARE(headline(Operation::Other, 0, QString(), QString(), QStringLiteral("Extracting")), QStringLiteral("Extracting"));
    QCOMPARE(folderName(QUrl(QStringLiteral("trash:/"))), QStringLiteral("Recycle Bin"));
    QCOMPARE(folderName(QUrl::fromLocalFile(QDir::homePath())), QStringLiteral("Home"));
    QCOMPARE(folderName(QUrl::fromLocalFile(QStringLiteral("/"))), QStringLiteral("This PC"));
    QCOMPARE(folderName(QUrl::fromLocalFile(QStringLiteral("/media/ann/USB Drive/"))), QStringLiteral("USB Drive"));
    QCOMPARE(itemsRemainingText(120, 0), QStringLiteral("120"));
    QVERIFY(itemsRemainingText(120, 1288490188ULL).startsWith(QLatin1String("120 (1.2 GB")));
}

void TestAskUser::undoText()
{
    QCOMPARE(deletedText({QUrl::fromLocalFile(QStringLiteral("/x/report.pdf"))}), QStringLiteral("Deleted report.pdf"));
    QCOMPARE(deletedText({QUrl::fromLocalFile(QStringLiteral("/x/a")), QUrl::fromLocalFile(QStringLiteral("/x/b"))}), QStringLiteral("Deleted 2 items"));
    UndoBar bar;
    QSignalSpy undo(&bar, &UndoBar::undoRequested);
    bar.showDeleted({QUrl::fromLocalFile(QStringLiteral("/x/report.pdf"))});
    QVERIFY(bar.isVisible());
    QCOMPARE(bar.text(), QStringLiteral("Deleted report.pdf"));
}

void TestAskUser::makeConflicts(int count)
{
    for (int i = 0; i < count; ++i) {
        const QString name = QStringLiteral("%1.txt").arg(QChar(u'a' + i));
        writeFile(m_src + QLatin1Char('/') + name, "new " + name.toUtf8());
        writeFile(m_dst + QLatin1Char('/') + name, "old " + name.toUtf8());
    }
}

KJob *TestAskUser::copyWith(const std::shared_ptr<ScriptedPrompter> &p, const QList<QUrl> &sources, const QUrl &dest)
{
    KIO::CopyJob *job = KIO::copy(sources, dest, KIO::HideProgressInfo);
    job->setUiDelegate(new JobUiDelegate(KJobUiDelegate::AutoHandlingDisabled, nullptr, p));
    return job;
}

void TestAskUser::copyReplace()
{
    makeConflicts(1);
    auto p = std::make_shared<ScriptedPrompter>();
    p->conflicts = {{ConflictChoice::Replace, false}};
    KJob *job = copyWith(p, {QUrl::fromLocalFile(m_src + QStringLiteral("/a.txt"))}, QUrl::fromLocalFile(m_dst));
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(p->askedConflicts.size(), 1);
    QCOMPARE(p->askedConflicts.first().destination.fileName(), QStringLiteral("a.txt"));
    QVERIFY(p->askedConflicts.first().canReplace());
    QCOMPARE(readFile(m_dst + QStringLiteral("/a.txt")), QByteArray("new a.txt"));
}

void TestAskUser::copySkip()
{
    makeConflicts(2);
    auto p = std::make_shared<ScriptedPrompter>();
    p->conflicts = {{ConflictChoice::Skip, false}, {ConflictChoice::Replace, false}};
    KJob *job = copyWith(p, {QUrl::fromLocalFile(m_src + QStringLiteral("/a.txt")), QUrl::fromLocalFile(m_src + QStringLiteral("/b.txt"))}, QUrl::fromLocalFile(m_dst));
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(p->askedConflicts.size(), 2);
    QCOMPARE(readFile(m_dst + QStringLiteral("/a.txt")), QByteArray("old a.txt"));
    QCOMPARE(readFile(m_dst + QStringLiteral("/b.txt")), QByteArray("new b.txt"));
    QCOMPARE(QDir(m_dst).entryList(QDir::Files).size(), 2);
}

void TestAskUser::copySkipSingle()
{
    // One file: KIO offers no skip, the answer is a quiet cancel and the
    // file in the destination is left alone.
    makeConflicts(1);
    auto p = std::make_shared<ScriptedPrompter>();
    p->conflicts = {{ConflictChoice::Skip, false}};
    KJob *job = copyWith(p, {QUrl::fromLocalFile(m_src + QStringLiteral("/a.txt"))}, QUrl::fromLocalFile(m_dst));
    job->exec();
    QVERIFY(!p->askedConflicts.first().canSkip());
    QCOMPARE(job->error(), int(KIO::ERR_USER_CANCELED));
    QCOMPARE(readFile(m_dst + QStringLiteral("/a.txt")), QByteArray("old a.txt"));
    QCOMPARE(QDir(m_dst).entryList(QDir::Files).size(), 1);
}

void TestAskUser::copyKeepBoth()
{
    makeConflicts(1);
    // a (2).txt is taken too: the copy lands on a (3).txt
    writeFile(m_dst + QStringLiteral("/a (2).txt"), "other");
    auto p = std::make_shared<ScriptedPrompter>();
    p->conflicts = {{ConflictChoice::KeepBoth, false}};
    KJob *job = copyWith(p, {QUrl::fromLocalFile(m_src + QStringLiteral("/a.txt"))}, QUrl::fromLocalFile(m_dst));
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(readFile(m_dst + QStringLiteral("/a.txt")), QByteArray("old a.txt"));
    QCOMPARE(readFile(m_dst + QStringLiteral("/a (2).txt")), QByteArray("other"));
    QCOMPARE(readFile(m_dst + QStringLiteral("/a (3).txt")), QByteArray("new a.txt"));
}

void TestAskUser::copyApplyToAll()
{
    makeConflicts(3);
    auto p = std::make_shared<ScriptedPrompter>();
    p->conflicts = {{ConflictChoice::Replace, true}};
    KJob *job = copyWith(p,
                         {QUrl::fromLocalFile(m_src + QStringLiteral("/a.txt")),
                          QUrl::fromLocalFile(m_src + QStringLiteral("/b.txt")),
                          QUrl::fromLocalFile(m_src + QStringLiteral("/c.txt"))},
                         QUrl::fromLocalFile(m_dst));
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    // asked once, for the first conflict, with "for all" offered
    QCOMPARE(p->askedConflicts.size(), 1);
    QVERIFY(p->askedConflicts.first().multiple());
    for (const char *name : {"a.txt", "b.txt", "c.txt"}) {
        QCOMPARE(readFile(m_dst + QLatin1Char('/') + QLatin1String(name)), QByteArray("new ") + name);
    }
}

void TestAskUser::copyAutoSkip()
{
    makeConflicts(3);
    writeFile(m_src + QStringLiteral("/d.txt"), "new d.txt");
    auto p = std::make_shared<ScriptedPrompter>();
    p->conflicts = {{ConflictChoice::Skip, true}};
    KJob *job = copyWith(p,
                         {QUrl::fromLocalFile(m_src + QStringLiteral("/a.txt")),
                          QUrl::fromLocalFile(m_src + QStringLiteral("/b.txt")),
                          QUrl::fromLocalFile(m_src + QStringLiteral("/c.txt")),
                          QUrl::fromLocalFile(m_src + QStringLiteral("/d.txt"))},
                         QUrl::fromLocalFile(m_dst));
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(p->askedConflicts.size(), 1);
    for (const char *name : {"a.txt", "b.txt", "c.txt"}) {
        QCOMPARE(readFile(m_dst + QLatin1Char('/') + QLatin1String(name)), QByteArray("old ") + name);
    }
    // the file with no conflict is copied
    QCOMPARE(readFile(m_dst + QStringLiteral("/d.txt")), QByteArray("new d.txt"));
}

void TestAskUser::copyCancel()
{
    makeConflicts(2);
    auto p = std::make_shared<ScriptedPrompter>();
    p->conflicts = {{ConflictChoice::Cancel, false}};
    KJob *job = copyWith(p, {QUrl::fromLocalFile(m_src + QStringLiteral("/a.txt")), QUrl::fromLocalFile(m_src + QStringLiteral("/b.txt"))}, QUrl::fromLocalFile(m_dst));
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), int(KIO::ERR_USER_CANCELED));
    QCOMPARE(readFile(m_dst + QStringLiteral("/a.txt")), QByteArray("old a.txt"));
    QCOMPARE(readFile(m_dst + QStringLiteral("/b.txt")), QByteArray("old b.txt"));
}

void TestAskUser::folderMerge()
{
    // src/Photos/x.jpg and dst/Photos/y.jpg: merging gives both in dst
    QVERIFY(QDir().mkpath(m_src + QStringLiteral("/Photos")));
    QVERIFY(QDir().mkpath(m_dst + QStringLiteral("/Photos")));
    writeFile(m_src + QStringLiteral("/Photos/x.jpg"), "x");
    writeFile(m_dst + QStringLiteral("/Photos/y.jpg"), "y");
    auto p = std::make_shared<ScriptedPrompter>();
    p->conflicts = {{ConflictChoice::Replace, false}};
    KJob *job = copyWith(p, {QUrl::fromLocalFile(m_src + QStringLiteral("/Photos"))}, QUrl::fromLocalFile(m_dst));
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(p->askedConflicts.size(), 1);
    QVERIFY(p->askedConflicts.first().isFolderMerge());
    QCOMPARE(readFile(m_dst + QStringLiteral("/Photos/x.jpg")), QByteArray("x"));
    QCOMPARE(readFile(m_dst + QStringLiteral("/Photos/y.jpg")), QByteArray("y"));
}

void TestAskUser::deleteAsks()
{
    // kiorc of this test: the defaults
    auto p = std::make_shared<ScriptedPrompter>();
    AskUser ask(p);
    ask.setConfirmationsConfig(m_dir->filePath(QStringLiteral("kiorc")));
    QSignalSpy spy(&ask, &KIO::AskUserActionInterface::askUserDeleteResult);
    const QList<QUrl> urls{QUrl::fromLocalFile(m_src + QStringLiteral("/a.txt"))};

    // moving to the bin: no question
    ask.askUserDelete(urls, KIO::AskUserActionInterface::Trash, KIO::AskUserActionInterface::DefaultConfirmation);
    QCOMPARE(p->askedDeletes, 0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);

    // Shift+Delete: asked, answered No
    p->deletes = {false};
    ask.askUserDelete(urls, KIO::AskUserActionInterface::Delete, KIO::AskUserActionInterface::ForceConfirmation);
    QCOMPARE(p->askedDeletes, 1);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), false);

    // with ConfirmTrash on, the bin asks too
    KSharedConfig::openConfig(m_dir->filePath(QStringLiteral("kiorc")), KConfig::NoGlobals)->group(QStringLiteral("Confirmations")).writeEntry("ConfirmTrash", true);
    p->deletes = {true};
    ask.askUserDelete(urls, KIO::AskUserActionInterface::Trash, KIO::AskUserActionInterface::DefaultConfirmation);
    QCOMPARE(p->askedDeletes, 2);
    QCOMPARE(spy.at(2).at(0).toBool(), true);
}

void TestAskUser::delegateFindsAskUser()
{
    // KIO finds the Raye interface among the delegate's direct children,
    // and only one of them.
    KIO::CopyJob *job = KIO::copy(QUrl::fromLocalFile(m_src), QUrl::fromLocalFile(m_dst), KIO::HideProgressInfo);
    job->setUiDelegate(new JobUiDelegate(KJobUiDelegate::AutoHandlingDisabled, nullptr, std::make_shared<ScriptedPrompter>()));
    auto *found = KIO::delegateExtension<KIO::AskUserActionInterface *>(job);
    QVERIFY(qobject_cast<AskUser *>(found));
    QCOMPARE(job->uiDelegate()->findChildren<KIO::AskUserActionInterface *>(QString(), Qt::FindDirectChildrenOnly).size(), 1);
    // other interfaces registered for the delegate come with it
    addDelegateInterface([] {
        auto *o = new QObject;
        o->setObjectName(QStringLiteral("extra"));
        return o;
    });
    JobUiDelegate second;
    QVERIFY(second.findChild<QObject *>(QStringLiteral("extra"), Qt::FindDirectChildrenOnly));
    QVERIFY(!installed());
    job->kill();
}

QTEST_MAIN(TestAskUser)

#include "test_askuser.moc"
