/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The address row (explorer spec 3.3, without the search box): Back,
// Forward, Up and Refresh with their history, the crumbs of the location,
// the chevron listings, drops on crumbs, edit mode (Alt+D, Ctrl+L, F4) and
// the loading line.

#include "addressbar/addressbar.h"
#include "addressbar/addressbox.h"
#include "addressbar/subfolders.h"
#include "locations/locationnames.h"
#include "ui/commandbutton.h"
#include "ui/motion.h"

#include <KLineEdit>
#include <KUrlNavigator>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Raye::Address;
using namespace Raye::Locations;

namespace
{
Context annContext()
{
    Context c;
    c.homePath = QStringLiteral("/home/ann");
    c.userName = QStringLiteral("Ann");
    c.knownFolders = {{KnownFolder::Documents, QStringLiteral("/home/ann/Documents")}, {KnownFolder::Pictures, QStringLiteral("/home/ann/Pictures")}};
    c.drives = {{QStringLiteral("/"), QStringLiteral("Raye"), QStringLiteral("drive-harddisk-system"), false}};
    return c;
}

QUrl local(const char *path)
{
    return QUrl::fromLocalFile(QString::fromUtf8(path));
}
}

class RayeAddressBarTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void showsTheCrumbs();
    void rowSizes();
    void buttonsFollowTheHistory();
    void backAndForward();
    void historyMenusJump();
    void historyMenusKeepTen();
    void upGoesToTheCrumbBefore();
    void refreshAsks();
    void tooltipsSayTheKeys();
    void editModeShowsThePath();
    void editModeGoesOnEnter();
    void editModeRestoresOnEscape();
    void editModeIgnoresWhatLeadsNowhere();
    void editShortcuts();
    void dropsOnFolderCrumbs();
    void noDropsOnPages();
    void longPathsFoldFromTheLeft();
    void loadingLine();
    void trailingWidthIsClamped();
    void subfoldersAreListedSorted();
    void thisPcListsItsPages();
    void ownNavigatorWhenNoneGiven();

private:
    AddressBar *m_bar = nullptr;
    KUrlNavigator *m_nav = nullptr;
};

void RayeAddressBarTest::initTestCase()
{
    Raye::Ui::Motion::setSpeedFactor(0);
    qRegisterMetaType<QDropEvent *>();
}

void RayeAddressBarTest::init()
{
    m_bar = new AddressBar;
    m_bar->setLocationNames(LocationNames(annContext()));
    m_nav = new KUrlNavigator(nullptr, local("/home/ann/Documents"), nullptr);
    m_bar->setNavigator(m_nav);
    m_bar->resize(1000, AddressBar::rowHeight);
    m_bar->show();
}

void RayeAddressBarTest::cleanup()
{
    delete m_bar;
    m_bar = nullptr;
    delete m_nav;
    m_nav = nullptr;
}

void RayeAddressBarTest::showsTheCrumbs()
{
    QCOMPARE(m_bar->addressBox()->crumbTexts(), (QStringList{"This PC", "Documents"}));
    m_bar->setUrl(local("/home/ann/Documents/Work"));
    QCOMPARE(m_bar->url(), local("/home/ann/Documents/Work"));
    QCOMPARE(m_bar->addressBox()->crumbTexts(), (QStringList{"This PC", "Documents", "Work"}));
    QCOMPARE(m_bar->addressBox()->locationIconName(), QStringLiteral("folder"));
    // a change made elsewhere (Dolphin's own Alt+Left) shows too
    m_nav->goBack();
    QCOMPARE(m_bar->addressBox()->crumbTexts(), (QStringList{"This PC", "Documents"}));
}

void RayeAddressBarTest::rowSizes()
{
    QCOMPARE(AddressBar::rowHeight, 48);
    QCOMPARE(m_bar->sizeHint().height(), 48);
    QCOMPARE(m_bar->backButton()->size(), QSize(32, 32));
    QCOMPARE(m_bar->addressBox()->boxRect().height(), 32);
    QVERIFY(m_bar->addressBox()->width() > 600); // the box takes what is left
}

void RayeAddressBarTest::buttonsFollowTheHistory()
{
    QVERIFY(!m_bar->backButton()->isEnabled());
    QVERIFY(!m_bar->forwardButton()->isEnabled());
    QVERIFY(m_bar->upButton()->isEnabled());
    m_bar->setUrl(local("/home/ann/Documents/Work"));
    QVERIFY(m_bar->backButton()->isEnabled());
    QVERIFY(!m_bar->forwardButton()->isEnabled());
    m_bar->setUrl(thisPcUrl());
    QVERIFY(!m_bar->upButton()->isEnabled()); // nothing above This PC
    m_bar->setUrl(homePageUrl());
    QVERIFY(!m_bar->upButton()->isEnabled());
}

void RayeAddressBarTest::backAndForward()
{
    m_bar->setUrl(local("/home/ann/Documents/Work"));
    m_bar->backButton()->click();
    QCOMPARE(m_nav->locationUrl(), local("/home/ann/Documents"));
    QVERIFY(m_bar->forwardButton()->isEnabled());
    m_bar->forwardButton()->click();
    QCOMPARE(m_nav->locationUrl(), local("/home/ann/Documents/Work"));
}

void RayeAddressBarTest::historyMenusJump()
{
    m_bar->setUrl(local("/home/ann/Documents/a"));
    m_bar->setUrl(local("/home/ann/Documents/b"));
    m_bar->setUrl(local("/home/ann/Documents/c"));
    QMenu *menu = m_bar->historyMenu(AddressBar::Back);
    const QList<QAction *> back = menu->actions();
    QCOMPARE(back.size(), 3);
    QCOMPARE(back.at(0)->text(), QStringLiteral("b")); // nearest first
    QCOMPARE(back.at(2)->text(), QStringLiteral("Documents"));
    back.at(1)->trigger();
    QCOMPARE(m_nav->locationUrl(), local("/home/ann/Documents/a"));
    menu->deleteLater();

    menu = m_bar->historyMenu(AddressBar::Forward);
    const QList<QAction *> fwd = menu->actions();
    QCOMPARE(fwd.size(), 2);
    QCOMPARE(fwd.at(0)->text(), QStringLiteral("b"));
    fwd.at(1)->trigger();
    QCOMPARE(m_nav->locationUrl(), local("/home/ann/Documents/c"));
    menu->deleteLater();
}

void RayeAddressBarTest::historyMenusKeepTen()
{
    for (int i = 0; i < 15; ++i) {
        m_bar->setUrl(local(qPrintable(QStringLiteral("/home/ann/Documents/%1").arg(i))));
    }
    QMenu *menu = m_bar->historyMenu(AddressBar::Back);
    QCOMPARE(menu->actions().size(), 10);
    delete menu;
}

void RayeAddressBarTest::upGoesToTheCrumbBefore()
{
    m_bar->upButton()->click();
    QCOMPARE(m_nav->locationUrl(), thisPcUrl());
    m_bar->setUrl(local("/usr/share"));
    m_bar->upButton()->click();
    QCOMPARE(m_nav->locationUrl(), local("/usr"));
    QVERIFY(m_bar->backButton()->isEnabled()); // Up is a step of the history
}

void RayeAddressBarTest::refreshAsks()
{
    QSignalSpy spy(m_bar, &AddressBar::refreshRequested);
    m_bar->refreshButton()->click();
    QCOMPARE(spy.count(), 1);
}

void RayeAddressBarTest::tooltipsSayTheKeys()
{
    QCOMPARE(m_bar->backButton()->toolTip(), Raye::Ui::CommandButton::toolTipFor(QStringLiteral("Back"), QKeySequence(Qt::ALT | Qt::Key_Left)));
    QVERIFY(m_bar->backButton()->toolTip().contains(QLatin1String("Alt")));
    QVERIFY(m_bar->upButton()->toolTip().contains(QLatin1String("Up")));
    QVERIFY(m_bar->refreshButton()->toolTip().contains(QLatin1String("F5")));
    QCOMPARE(m_bar->backButton()->accessibleName(), QStringLiteral("Back"));
}

void RayeAddressBarTest::editModeShowsThePath()
{
    m_bar->editLocation();
    AddressBox *box = m_bar->addressBox();
    QVERIFY(box->isEditing());
    QCOMPARE(box->lineEdit()->text(), QStringLiteral("/home/ann/Documents"));
    QCOMPARE(box->lineEdit()->selectedText(), QStringLiteral("/home/ann/Documents"));
    QVERIFY(box->lineEdit()->isVisible());
    m_bar->setUrl(thisPcUrl());
    QVERIFY(!box->isEditing()); // a new location ends editing
    m_bar->editLocation();
    QCOMPARE(box->lineEdit()->text(), QStringLiteral("This PC"));
}

void RayeAddressBarTest::editModeGoesOnEnter()
{
    m_bar->editLocation();
    AddressBox *box = m_bar->addressBox();
    box->lineEdit()->setText(QStringLiteral("Work"));
    QTest::keyClick(box->lineEdit(), Qt::Key_Return);
    QCOMPARE(m_nav->locationUrl(), local("/home/ann/Documents/Work"));
    QVERIFY(!box->isEditing());

    m_bar->editLocation();
    box->lineEdit()->setText(QStringLiteral("\\\\nas\\music"));
    QTest::keyClick(box->lineEdit(), Qt::Key_Enter);
    QCOMPARE(m_nav->locationUrl(), QUrl(QStringLiteral("smb://nas/music")));
}

void RayeAddressBarTest::editModeRestoresOnEscape()
{
    m_bar->editLocation();
    AddressBox *box = m_bar->addressBox();
    box->lineEdit()->setText(QStringLiteral("/tmp"));
    QTest::keyClick(box->lineEdit(), Qt::Key_Escape);
    QVERIFY(!box->isEditing());
    QCOMPARE(m_nav->locationUrl(), local("/home/ann/Documents"));
    QCOMPARE(box->crumbTexts(), (QStringList{"This PC", "Documents"}));
}

void RayeAddressBarTest::editModeIgnoresWhatLeadsNowhere()
{
    m_bar->setUrl(thisPcUrl());
    m_bar->editLocation();
    AddressBox *box = m_bar->addressBox();
    box->lineEdit()->setText(QStringLiteral("Work")); // relative to a page
    QTest::keyClick(box->lineEdit(), Qt::Key_Return);
    QVERIFY(box->isEditing());
    QCOMPARE(m_nav->locationUrl(), thisPcUrl());
}

void RayeAddressBarTest::editShortcuts()
{
    const QList<QKeySequence> keys = m_bar->editLocationAction()->shortcuts();
    QVERIFY(keys.contains(QKeySequence(Qt::ALT | Qt::Key_D)));
    QVERIFY(keys.contains(QKeySequence(Qt::CTRL | Qt::Key_L)));
    QVERIFY(keys.contains(QKeySequence(Qt::Key_F4)));
    m_bar->editLocationAction()->trigger();
    QVERIFY(m_bar->addressBox()->isEditing());
}

void RayeAddressBarTest::dropsOnFolderCrumbs()
{
    QSignalSpy spy(m_bar, &AddressBar::urlsDropped);
    QWidget *documents = m_bar->addressBox()->crumbWidget(1);
    QVERIFY(documents);
    QVERIFY(documents->acceptDrops());
    QMimeData mime;
    mime.setUrls({local("/tmp/a.txt")});
    QDragEnterEvent enter(QPoint(4, 4), Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(documents, &enter);
    QVERIFY(enter.isAccepted());
    QDropEvent drop(QPointF(4, 4), Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(documents, &drop);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toUrl(), local("/home/ann/Documents"));
}

void RayeAddressBarTest::noDropsOnPages()
{
    QWidget *thisPc = m_bar->addressBox()->crumbWidget(0);
    QVERIFY(thisPc);
    QVERIFY(!thisPc->acceptDrops());
    // Text is not files: refused even on a folder.
    QWidget *documents = m_bar->addressBox()->crumbWidget(1);
    QMimeData mime;
    mime.setText(QStringLiteral("hello"));
    QDragEnterEvent enter(QPoint(4, 4), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    enter.setAccepted(false);
    QApplication::sendEvent(documents, &enter);
    QVERIFY(!enter.isAccepted());
}

void RayeAddressBarTest::longPathsFoldFromTheLeft()
{
    m_bar->setUrl(local("/home/ann/Documents/a very long folder name/another long folder name/yet another one/and the last one here"));
    m_bar->resize(1800, AddressBar::rowHeight);
    QCoreApplication::processEvents();
    QCOMPARE(m_bar->addressBox()->hiddenCrumbCount(), 0);
    m_bar->resize(420, AddressBar::rowHeight);
    QCoreApplication::processEvents();
    AddressBox *box = m_bar->addressBox();
    QVERIFY(box->hiddenCrumbCount() > 0);
    QVERIFY(box->overflowButton()->isVisible());
    // the location itself always stays
    QVERIFY(box->crumbWidget(box->crumbTexts().size() - 1)->isVisible());
    QVERIFY(!box->crumbWidget(0)->isVisible());
    QMenu *menu = box->overflowMenu();
    QCOMPARE(menu->actions().size(), box->hiddenCrumbCount());
    QCOMPARE(menu->actions().first()->text(), box->crumbTexts().at(box->hiddenCrumbCount() - 1)); // deepest first
    delete menu;
}

void RayeAddressBarTest::loadingLine()
{
    QVERIFY(!m_bar->addressBox()->isLoading());
    m_bar->setLoading(true);
    QVERIFY(m_bar->addressBox()->isLoading());
    QCOMPARE(m_bar->addressBox()->loadingProgress(), -1);
    m_bar->setLoadingProgress(40);
    QCOMPARE(m_bar->addressBox()->loadingProgress(), 40);
    m_bar->setLoadingProgress(140);
    QCOMPARE(m_bar->addressBox()->loadingProgress(), 100);
    m_bar->setLoading(false);
    QVERIFY(!m_bar->addressBox()->isLoading());
}

void RayeAddressBarTest::trailingWidthIsClamped()
{
    auto *search = new QWidget;
    m_bar->setTrailingWidget(search);
    QCOMPARE(m_bar->trailingWidth(), 240);
    QSignalSpy spy(m_bar, &AddressBar::trailingWidthChanged);
    m_bar->setTrailingWidth(100);
    QCOMPARE(m_bar->trailingWidth(), 160);
    m_bar->setTrailingWidth(900);
    QCOMPARE(m_bar->trailingWidth(), 480);
    QCOMPARE(spy.count(), 2);
    QCoreApplication::processEvents();
    QCOMPARE(search->width(), 480);
}

void RayeAddressBarTest::subfoldersAreListedSorted()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir d(dir.path());
    for (const char *name : {"a10", "a2", "B", ".hidden"}) {
        QVERIFY(d.mkdir(QString::fromLatin1(name)));
    }
    QFile file(d.filePath(QStringLiteral("notes.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    Subfolders lister;
    QSignalSpy spy(&lister, &Subfolders::finished);
    lister.list(QUrl::fromLocalFile(dir.path()));
    QVERIFY(spy.wait(10000));
    QStringList names;
    for (const Crumb &c : lister.folders()) {
        names << c.text;
        QVERIFY(c.url.isLocalFile());
    }
    QCOMPARE(names, (QStringList{"a2", "a10", "B"}));
    QCOMPARE(lister.folders().at(0).url, QUrl::fromLocalFile(d.filePath(QStringLiteral("a2"))));
}

void RayeAddressBarTest::thisPcListsItsPages()
{
    Subfolders lister;
    lister.setLocationNames(LocationNames(annContext()));
    QSignalSpy spy(&lister, &Subfolders::finished);
    lister.list(thisPcUrl());
    QCOMPARE(spy.count(), 1); // no listing needed: at once
    QStringList names;
    for (const Crumb &c : lister.folders()) {
        names << c.text;
    }
    QCOMPARE(names, (QStringList{"Documents", "Pictures", "Raye"}));
}

void RayeAddressBarTest::ownNavigatorWhenNoneGiven()
{
    AddressBar bar;
    bar.setLocationNames(LocationNames(annContext()));
    QVERIFY(bar.navigator());
    bar.setUrl(local("/home/ann/Pictures"));
    bar.setUrl(local("/home/ann/Pictures/2024"));
    QVERIFY(bar.backButton()->isEnabled());
    bar.backButton()->click();
    QCOMPARE(bar.url(), local("/home/ann/Pictures"));
}

QTEST_MAIN(RayeAddressBarTest)

#include "test_addressbar.moc"
