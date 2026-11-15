/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The search box (explorer spec 1.2, 3.3): placeholder, recent searches,
// Kind / Date modified / Size rules, the search URLs (Dolphin's form, read
// back by Dolphin's own DolphinQuery), the box's behaviour (300 ms wait,
// Enter, Esc, Current folder, options while searching, leaving the
// folder), its command bar set, and a real search on a temporary tree with
// filenamesearch (kio-extras), sub-folders included.

#include "commandbar/commandbar.h"
#include "search/dolphinquery.h"
#include "search/searchbox.h"
#include "search/searchoptions.h"
#include "ui/motion.h"

#include <KConfigGroup>
#include <KIO/ListJob>
#include <KProtocolInfo>
#include <KSharedConfig>

#include <QAction>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QMenu>
#include <QMimeDatabase>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

using namespace Raye::Search;

class RayeSearchTest : public QObject
{
    Q_OBJECT

public:
    static void initMain()
    {
        static QTemporaryDir home;
        qputenv("HOME", home.path().toLocal8Bit());
        qunsetenv("XDG_CONFIG_HOME");
        qunsetenv("XDG_DATA_HOME");
        qunsetenv("XDG_CACHE_HOME");
        QStandardPaths::setTestModeEnabled(true);
    }

private Q_SLOTS:
    void initTestCase();
    void placeholder();
    void recent_data();
    void recent();
    void recentKeepsTen();
    void dates_data();
    void dates();
    void sizes();
    void kinds_data();
    void kinds();
    void kindMimeList();
    void accepts_data();
    void accepts();
    void fileNameUrlRoundTrip();
    void dolphinUrlForRaye();
    void boxPlaceholderAndFolder();
    void boxDebounce();
    void boxEnterRemembers();
    void boxEscapeCloses();
    void boxCurrentFolderFilters();
    void boxOptionsRerun();
    void boxCloseKeepsScope();
    void boxLeavingFolderQuiet();
    void boxIgnoresResultsUrl();
    void boxOptionsMenu();
    void boxRegistersSet();
    void realSearchFindsSubfolders();

private:
    static SearchBox *box(QWidget *parent = nullptr);
    static QStringList listNames(const QUrl &url);
    QTemporaryDir m_tree;
};

void RayeSearchTest::initTestCase()
{
    Raye::Ui::Motion::setSpeedFactor(0);
    QVERIFY(m_tree.isValid());
    const QString root = m_tree.path();
    QVERIFY(QDir(root).mkpath(QStringLiteral("Work/Old")));
    for (const QString &f : {QStringLiteral("report one.txt"), QStringLiteral("Work/Report two.txt"), QStringLiteral("Work/Old/report three.odt"),
                             QStringLiteral("other.txt")}) {
        QFile file(root + QLatin1Char('/') + f);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(f.contains(QLatin1String("other")) ? "the quarterly figures" : "x");
    }
}

void RayeSearchTest::placeholder()
{
    QCOMPARE(placeholderText(QStringLiteral("Documents")), QStringLiteral("Search Documents"));
    QCOMPARE(placeholderText(QString()), QStringLiteral("Search"));
    QCOMPARE(placeholderText(QStringLiteral("  ")), QStringLiteral("Search"));
}

void RayeSearchTest::recent_data()
{
    QTest::addColumn<QStringList>("before");
    QTest::addColumn<QString>("term");
    QTest::addColumn<QStringList>("after");
    QTest::newRow("first") << QStringList{} << QStringLiteral("invoice") << QStringList{QStringLiteral("invoice")};
    QTest::newRow("newest on top") << QStringList{QStringLiteral("a")} << QStringLiteral("b") << QStringList{QStringLiteral("b"), QStringLiteral("a")};
    QTest::newRow("once, case aside") << QStringList{QStringLiteral("a"), QStringLiteral("Invoice")} << QStringLiteral("invoice")
                                      << QStringList{QStringLiteral("invoice"), QStringLiteral("a")};
    QTest::newRow("blank leaves it") << QStringList{QStringLiteral("a")} << QStringLiteral("   ") << QStringList{QStringLiteral("a")};
    QTest::newRow("spaces tidied") << QStringList{} << QStringLiteral("  tax   2025 ") << QStringList{QStringLiteral("tax 2025")};
}

void RayeSearchTest::recent()
{
    QFETCH(QStringList, before);
    QFETCH(QString, term);
    QFETCH(QStringList, after);
    QCOMPARE(addRecent(before, term), after);
}

void RayeSearchTest::recentKeepsTen()
{
    QStringList list;
    for (int i = 0; i < 15; ++i) {
        list = addRecent(list, QString::number(i));
    }
    QCOMPARE(list.size(), recentCount);
    QCOMPARE(list.first(), QStringLiteral("14"));
    QCOMPARE(list.last(), QStringLiteral("5"));
}

void RayeSearchTest::dates_data()
{
    QTest::addColumn<int>("range");
    QTest::addColumn<int>("firstDay");
    QTest::addColumn<QDate>("first");
    QTest::addColumn<QDate>("last");
    // Wednesday 14 October 2026.
    QTest::newRow("today") << int(DateRange::Today) << int(Qt::Monday) << QDate(2026, 10, 14) << QDate(2026, 10, 14);
    QTest::newRow("yesterday") << int(DateRange::Yesterday) << int(Qt::Monday) << QDate(2026, 10, 13) << QDate(2026, 10, 13);
    QTest::newRow("this week, Monday") << int(DateRange::ThisWeek) << int(Qt::Monday) << QDate(2026, 10, 12) << QDate(2026, 10, 14);
    QTest::newRow("this week, Sunday") << int(DateRange::ThisWeek) << int(Qt::Sunday) << QDate(2026, 10, 11) << QDate(2026, 10, 14);
    QTest::newRow("last week") << int(DateRange::LastWeek) << int(Qt::Monday) << QDate(2026, 10, 5) << QDate(2026, 10, 11);
    QTest::newRow("this month") << int(DateRange::ThisMonth) << int(Qt::Monday) << QDate(2026, 10, 1) << QDate(2026, 10, 14);
    QTest::newRow("last month") << int(DateRange::LastMonth) << int(Qt::Monday) << QDate(2026, 9, 1) << QDate(2026, 9, 30);
    QTest::newRow("this year") << int(DateRange::ThisYear) << int(Qt::Monday) << QDate(2026, 1, 1) << QDate(2026, 10, 14);
    QTest::newRow("last year") << int(DateRange::LastYear) << int(Qt::Monday) << QDate(2025, 1, 1) << QDate(2025, 12, 31);
    QTest::newRow("any") << int(DateRange::Any) << int(Qt::Monday) << QDate() << QDate();
}

void RayeSearchTest::dates()
{
    QFETCH(int, range);
    QFETCH(int, firstDay);
    QFETCH(QDate, first);
    QFETCH(QDate, last);
    const auto bounds = dateBounds(DateRange(range), QDate(2026, 10, 14), Qt::DayOfWeek(firstDay));
    QCOMPARE(bounds.first, first);
    QCOMPARE(bounds.second, last);
    // A week that starts today.
    const auto sameDay = dateBounds(DateRange::ThisWeek, QDate(2026, 10, 12), Qt::Monday);
    QCOMPARE(sameDay.first, QDate(2026, 10, 12));
    // January's last month is December of the year before.
    QCOMPARE(dateBounds(DateRange::LastMonth, QDate(2026, 1, 20), Qt::Monday).first, QDate(2025, 12, 1));
}

void RayeSearchTest::sizes()
{
    QCOMPARE(sizeBounds(SizeRange::Empty), (std::pair<qint64, qint64>{0, 1}));
    QCOMPARE(sizeBounds(SizeRange::Tiny), (std::pair<qint64, qint64>{1, 16 * 1024}));
    QCOMPARE(sizeBounds(SizeRange::Small), (std::pair<qint64, qint64>{16 * 1024, 1024 * 1024}));
    QCOMPARE(sizeBounds(SizeRange::Medium), (std::pair<qint64, qint64>{1024 * 1024, 128LL * 1024 * 1024}));
    QCOMPARE(sizeBounds(SizeRange::Large), (std::pair<qint64, qint64>{128LL * 1024 * 1024, 1024LL * 1024 * 1024}));
    QCOMPARE(sizeBounds(SizeRange::Huge), (std::pair<qint64, qint64>{1024LL * 1024 * 1024, 4LL * 1024 * 1024 * 1024}));
    QCOMPARE(sizeBounds(SizeRange::Gigantic), (std::pair<qint64, qint64>{4LL * 1024 * 1024 * 1024, -1}));
    QCOMPARE(sizeBounds(SizeRange::Any), (std::pair<qint64, qint64>{0, -1}));
}

void RayeSearchTest::kinds_data()
{
    QTest::addColumn<QString>("mime");
    QTest::addColumn<bool>("dir");
    QTest::addColumn<int>("kind");
    QTest::addColumn<bool>("matches");
    QTest::newRow("png picture") << QStringLiteral("image/png") << false << int(Kind::Picture) << true;
    QTest::newRow("png not document") << QStringLiteral("image/png") << false << int(Kind::Document) << false;
    QTest::newRow("pdf document") << QStringLiteral("application/pdf") << false << int(Kind::Document) << true;
    QTest::newRow("txt document") << QStringLiteral("text/plain") << false << int(Kind::Document) << true;
    QTest::newRow("docx document") << QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document") << false << int(Kind::Document)
                                   << true;
    QTest::newRow("docx not archive") << QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document") << false
                                      << int(Kind::Archive) << false;
    QTest::newRow("odt document") << QStringLiteral("application/vnd.oasis.opendocument.text") << false << int(Kind::Document) << true;
    QTest::newRow("zip archive") << QStringLiteral("application/zip") << false << int(Kind::Archive) << true;
    QTest::newRow("tar.gz archive") << QStringLiteral("application/x-compressed-tar") << false << int(Kind::Archive) << true;
    QTest::newRow("7z archive") << QStringLiteral("application/x-7z-compressed") << false << int(Kind::Archive) << true;
    QTest::newRow("zip not document") << QStringLiteral("application/zip") << false << int(Kind::Document) << false;
    QTest::newRow("mp3 music") << QStringLiteral("audio/mpeg") << false << int(Kind::Music) << true;
    QTest::newRow("mp4 video") << QStringLiteral("video/mp4") << false << int(Kind::Video) << true;
    QTest::newRow("folder") << QStringLiteral("inode/directory") << true << int(Kind::Folder) << true;
    QTest::newRow("folder not document") << QStringLiteral("inode/directory") << true << int(Kind::Document) << false;
    QTest::newRow("file not folder") << QStringLiteral("text/plain") << false << int(Kind::Folder) << false;
    QTest::newRow("any") << QStringLiteral("application/x-executable") << false << int(Kind::Any) << true;
}

void RayeSearchTest::kinds()
{
    QFETCH(QString, mime);
    QFETCH(bool, dir);
    QFETCH(int, kind);
    QFETCH(bool, matches);
    const QMimeType type = QMimeDatabase().mimeTypeForName(mime);
    QVERIFY(type.isValid());
    QCOMPARE(kindMatches(Kind(kind), type, dir), matches);
}

void RayeSearchTest::kindMimeList()
{
    const QStringList pictures = kindMimeTypes(Kind::Picture);
    QVERIFY(pictures.contains(QStringLiteral("image/png")));
    QVERIFY(pictures.contains(QStringLiteral("image/jpeg")));
    QVERIFY(!pictures.contains(QStringLiteral("text/plain")));
    QCOMPARE(kindMimeTypes(Kind::Folder), QStringList{QStringLiteral("inode/directory")});
    QVERIFY(kindMimeTypes(Kind::Any).isEmpty());
    const QStringList docs = kindMimeTypes(Kind::Document);
    QVERIFY(docs.contains(QStringLiteral("application/pdf")));
    QVERIFY(!docs.contains(QStringLiteral("application/zip")));
}

void RayeSearchTest::accepts_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<int>("date");
    QTest::addColumn<int>("size");
    QTest::addColumn<QString>("mime");
    QTest::addColumn<bool>("dir");
    QTest::addColumn<QDateTime>("modified");
    QTest::addColumn<qint64>("bytes");
    QTest::addColumn<bool>("accepted");

    const QDateTime today(QDate(2026, 10, 14), QTime(9, 30));
    const QDateTime lastYear(QDate(2025, 3, 2), QTime(12, 0));
    QTest::newRow("no filter") << int(Kind::Any) << int(DateRange::Any) << int(SizeRange::Any) << QStringLiteral("text/plain") << false << lastYear << qint64(5)
                               << true;
    QTest::newRow("today ok") << int(Kind::Any) << int(DateRange::Today) << int(SizeRange::Any) << QStringLiteral("text/plain") << false << today << qint64(5)
                              << true;
    QTest::newRow("today, last year's file") << int(Kind::Any) << int(DateRange::Today) << int(SizeRange::Any) << QStringLiteral("text/plain") << false
                                             << lastYear << qint64(5) << false;
    QTest::newRow("last year ok") << int(Kind::Any) << int(DateRange::LastYear) << int(SizeRange::Any) << QStringLiteral("text/plain") << false << lastYear
                                  << qint64(5) << true;
    QTest::newRow("tiny ok") << int(Kind::Any) << int(DateRange::Any) << int(SizeRange::Tiny) << QStringLiteral("text/plain") << false << today
                             << qint64(16 * 1024 - 1) << true;
    QTest::newRow("tiny, at 16 KB") << int(Kind::Any) << int(DateRange::Any) << int(SizeRange::Tiny) << QStringLiteral("text/plain") << false << today
                                    << qint64(16 * 1024) << false;
    QTest::newRow("empty") << int(Kind::Any) << int(DateRange::Any) << int(SizeRange::Empty) << QStringLiteral("text/plain") << false << today << qint64(0)
                           << true;
    QTest::newRow("size leaves folders out") << int(Kind::Any) << int(DateRange::Any) << int(SizeRange::Tiny) << QStringLiteral("inode/directory") << true
                                             << today << qint64(4096) << false;
    QTest::newRow("gigantic") << int(Kind::Any) << int(DateRange::Any) << int(SizeRange::Gigantic) << QStringLiteral("video/mp4") << false << today
                              << qint64(5LL * 1024 * 1024 * 1024) << true;
    QTest::newRow("kind and date") << int(Kind::Picture) << int(DateRange::ThisWeek) << int(SizeRange::Any) << QStringLiteral("image/png") << false << today
                                   << qint64(5) << true;
    QTest::newRow("kind wrong") << int(Kind::Picture) << int(DateRange::ThisWeek) << int(SizeRange::Any) << QStringLiteral("text/plain") << false << today
                                << qint64(5) << false;
    QTest::newRow("no date known") << int(Kind::Any) << int(DateRange::ThisWeek) << int(SizeRange::Any) << QStringLiteral("text/plain") << false
                                   << QDateTime() << qint64(5) << false;
}

void RayeSearchTest::accepts()
{
    QFETCH(int, kind);
    QFETCH(int, date);
    QFETCH(int, size);
    QFETCH(QString, mime);
    QFETCH(bool, dir);
    QFETCH(QDateTime, modified);
    QFETCH(qint64, bytes);
    QFETCH(bool, accepted);
    Options o;
    o.kind = Kind(kind);
    o.date = DateRange(date);
    o.size = SizeRange(size);
    QCOMPARE(acceptsResult(o, QMimeDatabase().mimeTypeForName(mime), dir, modified, bytes, QDate(2026, 10, 14), Qt::Monday), accepted);
}

void RayeSearchTest::fileNameUrlRoundTrip()
{
    Options o;
    o.term = QStringLiteral("  tax & 50% #1 ");
    o.fileContents = true;
    const QUrl folder = QUrl::fromLocalFile(QStringLiteral("/home/ann/My Documents"));
    const QUrl url = fileNameSearchUrl(o, folder, QStringLiteral("Search results"));
    QCOMPARE(url.scheme(), QStringLiteral("filenamesearch"));
    QVERIFY(isSearchUrl(url));
    QVERIFY(!isSearchUrl(folder));
    QCOMPARE(QUrlQuery(url).queryItemValue(QStringLiteral("checkContent")), QStringLiteral("yes"));

    // Dolphin's own reader sees the same search.
    const Search::DolphinQuery back(url, QUrl());
    QCOMPARE(back.searchTerm(), QStringLiteral("tax & 50% #1"));
    QCOMPARE(back.searchPath(), folder);
    QVERIFY(back.searchLocations() == Search::SearchLocations::FromHere);
    QVERIFY(back.searchThrough() == Search::SearchThrough::FileContents);

    o.fileContents = false;
    const Search::DolphinQuery names(fileNameSearchUrl(o, folder), QUrl());
    QVERIFY(names.searchThrough() == Search::SearchThrough::FileNames);
}

void RayeSearchTest::dolphinUrlForRaye()
{
    // A folder Baloo does not index: Dolphin picks filenamesearch.
    Options o;
    o.term = QStringLiteral("report");
    o.fileContents = true;
    const QUrl folder = QUrl::fromLocalFile(m_tree.path());
    const QUrl url = Search::urlForRayeSearch(o, folder);
    QCOMPARE(url.scheme(), QStringLiteral("filenamesearch"));
    const Search::DolphinQuery back(url, QUrl());
    QCOMPARE(back.searchTerm(), QStringLiteral("report"));
    QCOMPARE(back.searchPath().adjusted(QUrl::StripTrailingSlash), folder.adjusted(QUrl::StripTrailingSlash));
    QVERIFY(back.searchThrough() == Search::SearchThrough::FileContents);
    QVERIFY(Search::urlForRayeSearch(o, QUrl()).isEmpty());
}

SearchBox *RayeSearchTest::box(QWidget *parent)
{
    auto *b = new SearchBox(parent);
    b->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    b->setFolder(QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents")), QStringLiteral("Documents"));
    return b;
}

void RayeSearchTest::boxPlaceholderAndFolder()
{
    std::unique_ptr<SearchBox> b(box());
    QCOMPARE(b->lineEdit()->placeholderText(), QStringLiteral("Search Documents"));
    QCOMPARE(b->sizeHint().width(), 240);
    QCOMPARE(b->minimumSizeHint().width(), 160);
    QCOMPARE(b->sizeHint().height(), 38); // 32 plus the glow
    b->setFolder(QUrl::fromLocalFile(QStringLiteral("/home/ann/Pictures")), QStringLiteral("Pictures"));
    QCOMPARE(b->lineEdit()->placeholderText(), QStringLiteral("Search Pictures"));
}

void RayeSearchTest::boxDebounce()
{
    std::unique_ptr<SearchBox> b(box());
    QSignalSpy search(b.get(), &SearchBox::searchRequested);
    QTest::keyClicks(b->lineEdit(), QStringLiteral("rep"));
    QCOMPARE(search.count(), 0);
    QTest::qWait(debounceMs / 2);
    QTest::keyClick(b->lineEdit(), Qt::Key_O);
    QTest::qWait(debounceMs / 2);
    QCOMPARE(search.count(), 0); // the wait starts again with each key
    QTRY_COMPARE(search.count(), 1);
    const Search::DolphinQuery q(search.first().first().toUrl(), QUrl());
    QCOMPARE(q.searchTerm(), QStringLiteral("repo"));
    QCOMPARE(q.searchPath(), QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents")));
    QVERIFY(b->isSearching());
    // Typing alone does not fill the history.
    QVERIFY(b->recentSearches().isEmpty());
}

void RayeSearchTest::boxEnterRemembers()
{
    std::unique_ptr<SearchBox> b(box());
    int built = 0;
    b->setUrlBuilder([&built](const Options &o, const QUrl &folder) {
        ++built;
        return fileNameSearchUrl(o, folder, QStringLiteral("custom"));
    });
    QSignalSpy search(b.get(), &SearchBox::searchRequested);
    QTest::keyClicks(b->lineEdit(), QStringLiteral("invoice"));
    QTest::keyClick(b->lineEdit(), Qt::Key_Return);
    QCOMPARE(search.count(), 1); // at once
    QCOMPARE(built, 1);
    QCOMPARE(QUrlQuery(search.first().first().toUrl()).queryItemValue(QStringLiteral("title")), QStringLiteral("custom"));
    QCOMPARE(b->recentSearches(), QStringList{QStringLiteral("invoice")});
    QTest::qWait(debounceMs + 100);
    QCOMPARE(search.count(), 1); // the pending wait was dropped
}

void RayeSearchTest::boxEscapeCloses()
{
    std::unique_ptr<SearchBox> b(box());
    QSignalSpy closed(b.get(), &SearchBox::searchClosed);
    QTest::keyClicks(b->lineEdit(), QStringLiteral("x"));
    b->searchNow();
    QVERIFY(b->isSearching());
    QTest::keyClick(b->lineEdit(), Qt::Key_Escape);
    QCOMPARE(closed.count(), 1);
    QVERIFY(b->lineEdit()->text().isEmpty());
    QVERIFY(!b->isSearching());
    // Esc again on an empty box closes nothing more.
    QTest::keyClick(b->lineEdit(), Qt::Key_Escape);
    QCOMPARE(closed.count(), 1);
    // Erasing the words ends the search too.
    QTest::keyClicks(b->lineEdit(), QStringLiteral("ab"));
    b->searchNow();
    QTest::keyClick(b->lineEdit(), Qt::Key_Backspace);
    QTest::keyClick(b->lineEdit(), Qt::Key_Backspace);
    QCOMPARE(closed.count(), 2);
}

void RayeSearchTest::boxCurrentFolderFilters()
{
    std::unique_ptr<SearchBox> b(box());
    Options o;
    o.scope = Scope::CurrentFolder;
    o.kind = Kind::Picture;
    o.term = QStringLiteral(" beach ");
    b->setOptions(o);
    QSignalSpy filter(b.get(), &SearchBox::filterRequested);
    QSignalSpy search(b.get(), &SearchBox::searchRequested);
    b->searchNow();
    QCOMPARE(search.count(), 0);
    QCOMPARE(filter.count(), 1);
    QCOMPARE(filter.first().at(0).toString(), QStringLiteral("beach"));
    QVERIFY(filter.first().at(1).toStringList().contains(QStringLiteral("image/png")));
    QVERIFY(b->isSearching());

    // Back to All subfolders: the filter goes, the search comes.
    for (QAction *a : b->optionsMenu()->actions()) {
        if (a->objectName() == QLatin1String("scope_all")) {
            a->trigger();
        }
    }
    QCOMPARE(filter.count(), 2);
    QVERIFY(filter.last().at(0).toString().isEmpty());
    QVERIFY(filter.last().at(1).toStringList().isEmpty());
    QCOMPARE(search.count(), 1);
}

static QAction *findAction(QMenu *menu, const QString &name)
{
    for (QAction *a : menu->actions()) {
        if (a->objectName() == name) {
            return a;
        }
        if (a->menu()) {
            if (QAction *inner = findAction(a->menu(), name)) {
                return inner;
            }
        }
    }
    return nullptr;
}

void RayeSearchTest::boxOptionsRerun()
{
    std::unique_ptr<SearchBox> b(box());
    QSignalSpy search(b.get(), &SearchBox::searchRequested);
    QSignalSpy changed(b.get(), &SearchBox::optionsChanged);
    QTest::keyClicks(b->lineEdit(), QStringLiteral("report"));
    b->searchNow();
    QCOMPARE(search.count(), 1);
    findAction(b->optionsMenu(), QStringLiteral("raye_search_contents"))->trigger();
    QCOMPARE(changed.count(), 1);
    QCOMPARE(search.count(), 2);
    QVERIFY(b->options().fileContents);
    QCOMPARE(QUrlQuery(search.last().first().toUrl()).queryItemValue(QStringLiteral("checkContent")), QStringLiteral("yes"));
    findAction(b->optionsMenu(), QStringLiteral("kind_document"))->trigger();
    QCOMPARE(search.count(), 3);
    QVERIFY(b->options().kind == Kind::Document);
    // The same choice again changes nothing.
    findAction(b->optionsMenu(), QStringLiteral("kind_document"))->trigger();
    QCOMPARE(search.count(), 3);

    // A filter alone, no words: still a search (Windows does the same).
    std::unique_ptr<SearchBox> c(box());
    QSignalSpy search2(c.get(), &SearchBox::searchRequested);
    findAction(c->optionsMenu(), QStringLiteral("date_today"))->trigger();
    QCOMPARE(search2.count(), 1);
}

void RayeSearchTest::boxCloseKeepsScope()
{
    std::unique_ptr<SearchBox> b(box());
    Options o;
    o.term = QStringLiteral("a");
    o.kind = Kind::Music;
    o.size = SizeRange::Large;
    o.date = DateRange::LastYear;
    o.fileContents = true;
    o.scope = Scope::CurrentFolder;
    b->setOptions(o);
    b->searchNow();
    QSignalSpy closed(b.get(), &SearchBox::searchClosed);
    b->closeAction()->trigger();
    QCOMPARE(closed.count(), 1);
    QVERIFY(b->options().kind == Kind::Any);
    QVERIFY(b->options().size == SizeRange::Any);
    QVERIFY(b->options().date == DateRange::Any);
    QVERIFY(b->options().term.isEmpty());
    QVERIFY(b->options().fileContents);
    QVERIFY(b->options().scope == Scope::CurrentFolder);
    QVERIFY(findAction(b->optionsMenu(), QStringLiteral("kind_any"))->isChecked());
    QVERIFY(!b->closeAction()->isEnabled());
}

void RayeSearchTest::boxLeavingFolderQuiet()
{
    std::unique_ptr<SearchBox> b(box());
    QSignalSpy closed(b.get(), &SearchBox::searchClosed);
    QTest::keyClicks(b->lineEdit(), QStringLiteral("report"));
    b->searchNow();
    b->setFolder(QUrl::fromLocalFile(QStringLiteral("/home/ann/Music")), QStringLiteral("Music"));
    QCOMPARE(closed.count(), 0); // the window already left
    QVERIFY(!b->isSearching());
    QVERIFY(b->lineEdit()->text().isEmpty());
    QCOMPARE(b->lineEdit()->placeholderText(), QStringLiteral("Search Music"));
}

void RayeSearchTest::boxIgnoresResultsUrl()
{
    std::unique_ptr<SearchBox> b(box());
    QSignalSpy search(b.get(), &SearchBox::searchRequested);
    QTest::keyClicks(b->lineEdit(), QStringLiteral("report"));
    b->searchNow();
    // The window shows the results: that URL is not a new folder.
    b->setFolder(search.first().first().toUrl(), QStringLiteral("Search results"));
    QVERIFY(b->isSearching());
    QCOMPARE(b->lineEdit()->text(), QStringLiteral("report"));
    QCOMPARE(b->folder(), QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents")));
}

void RayeSearchTest::boxOptionsMenu()
{
    std::unique_ptr<SearchBox> b(box());
    QStringList top;
    for (QAction *a : b->optionsMenu()->actions()) {
        top << (a->isSeparator() ? QStringLiteral("-") : a->objectName());
    }
    QCOMPARE(top,
             (QStringList{QStringLiteral("raye_search_date"), QStringLiteral("raye_search_kind"), QStringLiteral("raye_search_size"), QStringLiteral("-"),
                          QStringLiteral("raye_search_contents"), QStringLiteral("-"), QStringLiteral("scope_current"), QStringLiteral("scope_all"),
                          QStringLiteral("-"), QStringLiteral("raye_close_search")}));
    QVERIFY(findAction(b->optionsMenu(), QStringLiteral("scope_all"))->isChecked());
    QVERIFY(findAction(b->optionsMenu(), QStringLiteral("date_any"))->isChecked());
    QCOMPARE(b->optionsAction()->menu(), b->optionsMenu());
    QCOMPARE(b->optionsAction()->text(), QStringLiteral("Search options"));
    QCOMPARE(findAction(b->optionsMenu(), QStringLiteral("size_tiny"))->text(), QString::fromUtf8("Tiny (0 – 16 KB)"));
}

void RayeSearchTest::boxRegistersSet()
{
    Raye::Commands::CommandBar bar;
    std::unique_ptr<SearchBox> b(box());
    b->registerWith(&bar);
    QVERIFY(bar.setIds().contains(QStringLiteral("search")));
    bar.resize(1280, Raye::Commands::CommandBar::rowHeight);
    Raye::Commands::CommandContext context;
    context.url = QUrl::fromLocalFile(QStringLiteral("/home/ann/Documents"));
    bar.setContext(context);
    QTRY_VERIFY(!bar.visibleIds().contains(QStringLiteral("raye_search_options")));
    context.searching = true;
    bar.setContext(context);
    QTRY_VERIFY(bar.visibleIds().contains(QStringLiteral("raye_search_options")));
    QVERIFY(bar.visibleIds().contains(QStringLiteral("raye_close_search")));
}

QStringList RayeSearchTest::listNames(const QUrl &url)
{
    QStringList names;
    KIO::ListJob *job = KIO::listDir(url, KIO::HideProgressInfo);
    QObject::connect(job, &KIO::ListJob::entries, job, [&names](KIO::Job *, const KIO::UDSEntryList &entries) {
        for (const KIO::UDSEntry &e : entries) {
            const QString name = e.stringValue(KIO::UDSEntry::UDS_NAME);
            if (name != QLatin1String(".") && name != QLatin1String("..")) {
                names << QFileInfo(name).fileName();
            }
        }
    });
    QSignalSpy done(job, &KJob::result);
    if (!done.wait(20000)) {
        return {QStringLiteral("<timeout>")};
    }
    if (job->error()) {
        return {QStringLiteral("<error> ") + job->errorString()};
    }
    names.sort();
    return names;
}

void RayeSearchTest::realSearchFindsSubfolders()
{
    if (!KProtocolInfo::isKnownProtocol(QStringLiteral("filenamesearch"))) {
        // A package build checks what it says it checks.
        if (qEnvironmentVariableIsSet("RAYE_TESTS_STRICT")) {
            QFAIL("kio-extras (filenamesearch) is not installed");
        }
        QSKIP("kio-extras (filenamesearch) is not installed");
    }
    const QUrl folder = QUrl::fromLocalFile(m_tree.path());
    Options o;
    o.term = QStringLiteral("report");
    QCOMPARE(listNames(fileNameSearchUrl(o, folder)),
             (QStringList{QStringLiteral("Report two.txt"), QStringLiteral("report one.txt"), QStringLiteral("report three.odt")}));
    // The same through Dolphin's builder.
    QCOMPARE(listNames(Search::urlForRayeSearch(o, folder)).size(), 3);
    // File contents: only "other.txt" says "quarterly".
    o.term = QStringLiteral("quarterly");
    o.fileContents = true;
    QCOMPARE(listNames(fileNameSearchUrl(o, folder)), QStringList{QStringLiteral("other.txt")});
    // Nothing found: an empty list, not an error.
    o.term = QStringLiteral("zzzz-nothing");
    o.fileContents = false;
    QVERIFY(listNames(fileNameSearchUrl(o, folder)).isEmpty());
}

QTEST_MAIN(RayeSearchTest)

#include "test_search.moc"
