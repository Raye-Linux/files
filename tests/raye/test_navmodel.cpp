/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The navigation pane (explorer spec 3.5): its rows on a temp places list
// and a fake drive list, folders listed on demand, pins, the drop rules, and
// the pane's row sizes and selection.

#include "firstrun/firstrun.h"
#include "navpane/droprules.h"
#include "navpane/navigationmodel.h"
#include "navpane/navigationpane.h"

#include <KFilePlacesModel>
#include <KLocalizedString>

#include <QAbstractItemModelTester>
#include <QDir>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Raye::NavPane;

namespace
{
QTemporaryDir *sandbox()
{
    static QTemporaryDir dir;
    return &dir;
}

QString home()
{
    return sandbox()->path() + QStringLiteral("/home");
}

QUrl at(const QString &relative)
{
    return QUrl::fromLocalFile(home() + QLatin1Char('/') + relative);
}

QList<Drive> sampleDrives()
{
    Drive raye;
    raye.id = QStringLiteral("raye");
    raye.name = QStringLiteral("Raye");
    raye.iconName = QStringLiteral("drive-harddisk");
    raye.url = QUrl::fromLocalFile(QStringLiteral("/"));
    raye.usage = 0.4;
    Drive windows;
    windows.id = QStringLiteral("win");
    windows.name = QStringLiteral("Windows");
    windows.url = QUrl::fromLocalFile(sandbox()->path() + QStringLiteral("/windows"));
    windows.usage = 0.7;
    return {raye, windows};
}

QStringList topTexts(const NavigationModel &model)
{
    QStringList list;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex i = model.index(row, 0);
        if (i.data(NavigationModel::KindRole).toInt() == NavigationModel::Separator) {
            list << QStringLiteral("---");
        } else {
            list << i.data().toString();
        }
    }
    return list;
}

QStringList childTexts(const QModelIndex &parent)
{
    QStringList list;
    const QAbstractItemModel *m = parent.model();
    for (int row = 0; row < m->rowCount(parent); ++row) {
        list << m->index(row, 0, parent).data().toString();
    }
    return list;
}
}

class NavModelTest : public QObject
{
    Q_OBJECT

public:
    static void initMain()
    {
        const QString root = sandbox()->path();
        QDir().mkpath(home());
        qputenv("HOME", home().toUtf8());
        qputenv("XDG_DATA_HOME", (root + QStringLiteral("/data")).toUtf8());
        qputenv("XDG_CONFIG_HOME", (root + QStringLiteral("/config")).toUtf8());
        qputenv("XDG_CACHE_HOME", (root + QStringLiteral("/cache")).toUtf8());
        qputenv("XDG_CONFIG_DIRS", (root + QStringLiteral("/etc-xdg")).toUtf8());
        for (const char *dir : {"Desktop", "Downloads", "Documents", "Pictures", "Music", "Videos", "Documents/Work", "Documents/archive 10",
                                "Documents/archive 9", "Documents/.secret"}) {
            QDir().mkpath(home() + QLatin1Char('/') + QLatin1String(dir));
        }
        QDir().mkpath(root + QStringLiteral("/windows/Users"));
        QFile file(home() + QStringLiteral("/Documents/notes.txt"));
        file.open(QIODevice::WriteOnly);
    }

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setLanguages({QStringLiteral("en")});
        m_places = new KFilePlacesModel(this);
        Raye::FirstRun::migratePlaces(m_places);
        m_drives = new StaticDriveSource(sampleDrives(), this);
    }

    void topLevel()
    {
        NavigationModel model(m_places, m_drives);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(topTexts(model),
                 QStringList({QStringLiteral("Home"), QStringLiteral("Gallery"), QStringLiteral("---"), QStringLiteral("Desktop"),
                              QStringLiteral("Downloads"), QStringLiteral("Documents"), QStringLiteral("Pictures"), QStringLiteral("Music"),
                              QStringLiteral("Videos"), QStringLiteral("---"), QStringLiteral("This PC"), QStringLiteral("Network")}));
        QCOMPARE(model.index(0, 0).data(NavigationModel::UrlRole).toUrl(), NavigationModel::homeUrl());
        QCOMPARE(model.index(5, 0).data(NavigationModel::UrlRole).toUrl().adjusted(QUrl::StripTrailingSlash), at(QStringLiteral("Documents")));
        QCOMPARE(model.index(5, 0).data(NavigationModel::KindRole).toInt(), int(NavigationModel::Pin));
        // No KDE leftovers: no Trash, no root, no Recent, no Search For.
        for (int row = 0; row < model.rowCount(); ++row) {
            const QUrl url = model.index(row, 0).data(NavigationModel::UrlRole).toUrl();
            QVERIFY2(url.scheme() != QLatin1String("trash") && url != QUrl::fromLocalFile(QStringLiteral("/"))
                         && url.scheme() != QLatin1String("recentlyused") && url.scheme() != QLatin1String("timeline"),
                     qPrintable(url.toString()));
        }
    }

    void flags()
    {
        NavigationModel model(m_places, m_drives);
        QVERIFY(!(model.flags(model.index(0, 0)) & Qt::ItemIsDropEnabled)); // Home is a page
        QCOMPARE(model.flags(model.index(2, 0)), Qt::NoItemFlags); // separator
        QVERIFY(model.flags(model.index(3, 0)) & Qt::ItemIsDropEnabled); // Desktop
        QVERIFY(model.hasChildren(model.index(3, 0))); // a chevron before listing
        const QModelIndex network = model.indexForUrl(NavigationModel::networkUrl());
        QCOMPARE(network.data().toString(), QStringLiteral("Network"));
        QVERIFY(!(model.flags(network) & Qt::ItemIsDropEnabled));
    }

    void recycleBin()
    {
        NavigationModel model(m_places, m_drives);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        const int rows = model.rowCount();
        model.setShowAllFolders(true);
        QCOMPARE(model.rowCount(), rows + 1);
        const QModelIndex bin = model.index(rows, 0);
        QCOMPARE(bin.data().toString(), QStringLiteral("Recycle Bin"));
        QCOMPARE(bin.data(NavigationModel::UrlRole).toUrl(), QUrl(QStringLiteral("trash:/")));
        QVERIFY(model.flags(bin) & Qt::ItemIsDropEnabled);
        model.setShowAllFolders(false);
        QCOMPARE(model.rowCount(), rows);
    }

    void drives()
    {
        StaticDriveSource source(sampleDrives());
        NavigationModel model(m_places, &source);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        const QModelIndex pc = model.indexForUrl(NavigationModel::thisPcUrl());
        QVERIFY(pc.isValid());
        QCOMPARE(childTexts(pc), QStringList({QStringLiteral("Raye"), QStringLiteral("Windows")}));
        QCOMPARE(model.index(0, 0, pc).data(NavigationModel::KindRole).toInt(), int(NavigationModel::Drive));
        QCOMPARE(model.index(0, 0, pc).data(NavigationModel::UsageRole).toReal(), 0.4);

        // Usage changes: same rows, data changed, nothing removed.
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        QList<Drive> list = sampleDrives();
        list[0].usage = 0.5;
        source.setDrives(list);
        QCOMPARE(removed.count(), 0);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(model.index(0, 0, pc).data(NavigationModel::UsageRole).toReal(), 0.5);

        // A USB stick plugged in.
        Drive usb;
        usb.id = QStringLiteral("usb");
        usb.name = QStringLiteral("USB Drive");
        usb.removable = true;
        list.append(usb);
        source.setDrives(list);
        QCOMPARE(childTexts(pc).last(), QStringLiteral("USB Drive"));
        QVERIFY(model.index(2, 0, pc).data(NavigationModel::RemovableRole).toBool());
        // Not mounted: nowhere to open, nothing to expand or drop on.
        QVERIFY(!model.hasChildren(model.index(2, 0, pc)));
        QVERIFY(!(model.flags(model.index(2, 0, pc)) & Qt::ItemIsDropEnabled));
    }

    void lazyFolders()
    {
        // No model tester here: it fetches every row by itself.
        NavigationModel model(m_places, m_drives);
        const QModelIndex docs = model.index(5, 0);
        QCOMPARE(model.rowCount(docs), 0);
        QVERIFY(model.canFetchMore(docs));
        // Before listing, a folder below a pin is found as the pin itself.
        QCOMPARE(model.indexForUrl(at(QStringLiteral("Documents/Work"))), docs);

        QSignalSpy finished(&model, &NavigationModel::loadingFinished);
        model.fetchMore(docs);
        QVERIFY(model.isLoading());
        QVERIFY(finished.wait(5000));
        // Folders only, no hidden ones, numbers in their natural order.
        QCOMPARE(childTexts(docs), QStringList({QStringLiteral("archive 9"), QStringLiteral("archive 10"), QStringLiteral("Work")}));
        QVERIFY(!model.canFetchMore(docs));
        const QModelIndex work = model.indexForUrl(at(QStringLiteral("Documents/Work")));
        QCOMPARE(work.data().toString(), QStringLiteral("Work"));
        QCOMPARE(work.data(NavigationModel::KindRole).toInt(), int(NavigationModel::Folder));
        QCOMPARE(work.data(NavigationModel::DepthRole).toInt(), 1);
        // An empty folder loses its chevron once listed.
        QVERIFY(model.hasChildren(work));
        model.fetchMore(work);
        QVERIFY(finished.wait(5000));
        QVERIFY(!model.hasChildren(work));

        // A folder made later shows up.
        QDir().mkpath(home() + QStringLiteral("/Documents/Zeta"));
        QTRY_COMPARE_WITH_TIMEOUT(childTexts(docs).size(), 4, 10000);
        QCOMPARE(childTexts(docs).last(), QStringLiteral("Zeta"));
        QDir(home() + QStringLiteral("/Documents/Zeta")).removeRecursively();
    }

    void driveFolders()
    {
        NavigationModel model(m_places, m_drives);
        const QModelIndex pc = model.indexForUrl(NavigationModel::thisPcUrl());
        const QModelIndex windows = model.index(1, 0, pc);
        QSignalSpy finished(&model, &NavigationModel::loadingFinished);
        model.fetchMore(windows);
        QVERIFY(finished.wait(5000));
        QCOMPARE(childTexts(windows), QStringList({QStringLiteral("Users")}));
        QCOMPARE(model.indexForUrl(QUrl::fromLocalFile(sandbox()->path() + QStringLiteral("/windows/Users/ana"))).data().toString(), QStringLiteral("Users"));
    }

    void pins()
    {
        NavigationModel model(m_places, m_drives);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        const QUrl work = at(QStringLiteral("Documents/Work"));
        QVERIFY(!model.isPinned(work));
        QVERIFY(model.pin(work));
        QVERIFY(model.isPinned(work));
        QVERIFY(topTexts(model).contains(QStringLiteral("Work")));
        QVERIFY(!model.pin(work)); // twice: nothing
        // Every KDE dialog sees the same pin.
        KFilePlacesModel other;
        bool seen = false;
        for (int row = 0; row < other.rowCount(); ++row) {
            seen = seen || other.url(other.index(row, 0)).adjusted(QUrl::StripTrailingSlash) == work;
        }
        QVERIFY(seen);
        QVERIFY(model.unpin(work));
        QVERIFY(!model.isPinned(work));
        QVERIFY(!topTexts(model).contains(QStringLiteral("Work")));

        const QUrl desktop = at(QStringLiteral("Desktop"));
        QVERIFY(model.unpin(desktop));
        QVERIFY(!topTexts(model).contains(QStringLiteral("Desktop")));
        QVERIFY(model.pin(desktop, QStringLiteral("Desktop")));
        QCOMPARE(topTexts(model).count(QStringLiteral("Desktop")), 1);
    }

    void dropRules_data()
    {
        QTest::addColumn<int>("modifiers");
        QTest::addColumn<bool>("sameDrive");
        QTest::addColumn<bool>("toBin");
        QTest::addColumn<int>("action");
        QTest::newRow("same drive") << 0 << true << false << int(Qt::MoveAction);
        QTest::newRow("other drive") << 0 << false << false << int(Qt::CopyAction);
        QTest::newRow("shift other drive") << int(Qt::ShiftModifier) << false << false << int(Qt::MoveAction);
        QTest::newRow("ctrl same drive") << int(Qt::ControlModifier) << true << false << int(Qt::CopyAction);
        QTest::newRow("alt") << int(Qt::AltModifier) << true << false << int(Qt::LinkAction);
        QTest::newRow("ctrl shift") << int(Qt::ControlModifier | Qt::ShiftModifier) << false << false << int(Qt::LinkAction);
        QTest::newRow("bin, ctrl") << int(Qt::ControlModifier) << false << true << int(Qt::MoveAction);
    }

    void dropRules()
    {
        QFETCH(int, modifiers);
        QFETCH(bool, sameDrive);
        QFETCH(bool, toBin);
        QFETCH(int, action);
        QCOMPARE(int(dropAction(Qt::KeyboardModifiers(modifiers), sameDrive, toBin)), action);
    }

    void driveDetection()
    {
        const QList<QUrl> files = {at(QStringLiteral("Documents/notes.txt"))};
        QVERIFY(sameDrive(files, at(QStringLiteral("Desktop"))));
        // A target that does not exist yet is judged by the folder above it.
        QVERIFY(sameDrive(files, at(QStringLiteral("Desktop/new/deeper"))));
        QVERIFY(!sameDrive(files, QUrl(QStringLiteral("smb://nas/share"))));
        QVERIFY(sameDrive({QUrl(QStringLiteral("smb://nas/share/a"))}, QUrl(QStringLiteral("smb://nas/share/b"))));
        QVERIFY(!sameDrive({QUrl(QStringLiteral("smb://nas/share/a"))}, QUrl(QStringLiteral("smb://other/share"))));
        QVERIFY(!sameDrive({}, at(QStringLiteral("Desktop"))));
        if (QFileInfo::exists(QStringLiteral("/proc/self"))) {
            QVERIFY(!sameDrive(files, QUrl::fromLocalFile(QStringLiteral("/proc"))));
        }
    }

    void pointless()
    {
        const QUrl docs = at(QStringLiteral("Documents"));
        QVERIFY(isPointlessDrop({docs}, at(QStringLiteral("Documents/Work")), Qt::MoveAction));
        QVERIFY(isPointlessDrop({docs}, docs, Qt::CopyAction));
        QVERIFY(isPointlessDrop({at(QStringLiteral("Documents/notes.txt"))}, docs, Qt::MoveAction));
        QVERIFY(!isPointlessDrop({at(QStringLiteral("Documents/notes.txt"))}, docs, Qt::CopyAction));
        QVERIFY(!isPointlessDrop({at(QStringLiteral("Documents/notes.txt"))}, at(QStringLiteral("Desktop")), Qt::MoveAction));
    }

    void pane()
    {
        NavigationPane pane(m_places, m_drives);
        pane.resize(240, 600);
        pane.show();
        QVERIFY(QTest::qWaitForWindowExposed(&pane));
        NavigationModel *model = pane.navigationModel();
        QCOMPARE(pane.visualRect(model->index(0, 0)).height(), 32);
        QCOMPARE(pane.visualRect(model->index(2, 0)).height(), 9);
        const QModelIndex pc = model->indexForUrl(NavigationModel::thisPcUrl());
        QVERIFY(pane.isExpanded(pc)); // This PC starts open
        QCOMPARE(pane.chevronRect(model->index(0, 0, pc)).left(), 8 + 16);
        pane.setCompact(true);
        QTRY_COMPARE(pane.visualRect(model->index(0, 0)).height(), 24);
        pane.setCompact(false);

        // A folder below a pin selects the pin; with "Expand to open folder" the folder.
        const QUrl work = at(QStringLiteral("Documents/Work"));
        pane.setCurrentUrl(work);
        QCOMPARE(pane.currentIndex().data().toString(), QStringLiteral("Documents"));
        pane.setExpandToOpenFolder(true);
        QTRY_COMPARE_WITH_TIMEOUT(pane.currentIndex().data().toString(), QStringLiteral("Work"), 5000);
        pane.setExpandToOpenFolder(false);

        // A click opens the row.
        QSignalSpy activated(&pane, &NavigationPane::urlActivated);
        const QRect home = pane.visualRect(model->index(0, 0));
        QTest::mouseClick(pane.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(120, home.center().y()));
        QCOMPARE(activated.count(), 1);
        QCOMPARE(activated.at(0).at(0).toUrl(), NavigationModel::homeUrl());
        // Separators do nothing.
        QTest::mouseClick(pane.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(120, pane.visualRect(model->index(2, 0)).center().y()));
        QCOMPARE(activated.count(), 1);
        // The chevron opens and closes without navigating.
        const QModelIndex raye = model->index(0, 0, pc);
        QVERIFY(!pane.isExpanded(raye));
        QTest::mouseClick(pane.viewport(), Qt::LeftButton, Qt::NoModifier, pane.chevronRect(raye).center());
        QVERIFY(pane.isExpanded(raye));
        QCOMPARE(activated.count(), 1);
    }

private:
    KFilePlacesModel *m_places = nullptr;
    StaticDriveSource *m_drives = nullptr;
};

QTEST_MAIN(NavModelTest)

#include "test_navmodel.moc"
