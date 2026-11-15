/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The controls of the Files widget kit: sizes (32 high, 40 wide icon
// buttons), the colours of each state, keyboard behaviour and the focus
// ring, flyout placement and shadow, the card dialog, motion and the
// showcase list the render harness reads.

#include "ui/carddialog.h"
#include "ui/commandbutton.h"
#include "ui/flyout.h"
#include "ui/focusring.h"
#include "ui/motion.h"
#include "ui/pivotbar.h"
#include "ui/separator.h"
#include "ui/shadow.h"
#include "ui/showcase.h"
#include "ui/splitbutton.h"
#include "ui/tokens.h"
#include "ui/usagebar.h"

#include <KLocalizedString>

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

using namespace Raye::Ui;

namespace
{
QIcon someIcon()
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::red);
    return QIcon(pixmap);
}

int textWidth(const QWidget *w, const QString &text)
{
    return QFontMetrics(w->font()).horizontalAdvance(text);
}
}

class RayeUiControlsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();

    void iconButtonIs40By32();
    void labelledButtonWidth();
    void chevronAddsTwentyPixels();
    void compactButtonIs24High();
    void listRowsAlignLeft();
    void subtleLooks();
    void standardAndAccentLooks();
    void tooltipSaysTheShortcut();
    void buttonFollowsItsAction();
    void focusPolicyPerKind();
    void ringOnlyForKeyboardFocus();
    void enterPressesAButton();
    void downOpensTheMenu();

    void splitButtonIsOneTabStop();
    void splitButtonF4OpensTheMenu();

    void separatorIsOneBy24();

    void pivotBarKeys();
    void pivotPillSitsUnderTheCurrentItem();

    void usageBarValueAndCaution();

    void flyoutOpensBelowWithAGap();
    void flyoutFlipsAboveWhenNoRoom();
    void flyoutStaysOnScreen();
    void flyoutRightAligned();
    void flyoutEscCloses();
    void shadowCutsOutTheSurface();

    void cardDialogWidthAndButtons();
    void cardDialogEnterPressesTheDefault();
    void cardDialogEscRejects();
    void cardDialogCanStayOpen();

    void motionDurations();
    void motionOffRunsAtOnce();

    void showcaseRegistry();
};

void RayeUiControlsTest::initTestCase()
{
    KLocalizedString::setApplicationDomain("raye-files");
    QApplication::setPalette(TokenSet::table(true).palette());
}

void RayeUiControlsTest::init()
{
    // Instant motion: states are read right after they change.
    Motion::setSpeedFactor(0);
}

void RayeUiControlsTest::iconButtonIs40By32()
{
    CommandButton button(someIcon(), QStringLiteral("Copy"));
    button.setIconOnly(true);
    QCOMPARE(button.sizeHint(), QSize(40, 32));
    // The label stays the name screen readers and tooltips use.
    QCOMPARE(button.text(), QStringLiteral("Copy"));

    CommandButton noText;
    noText.setIcon(someIcon());
    QCOMPARE(noText.sizeHint(), QSize(Metrics::iconButtonWidth, Metrics::controlHeight));
}

void RayeUiControlsTest::labelledButtonWidth()
{
    CommandButton button(someIcon(), QStringLiteral("Sort"));
    QCOMPARE(button.sizeHint().height(), 32);
    QCOMPARE(button.sizeHint().width(), 12 + 16 + 8 + textWidth(&button, QStringLiteral("Sort")) + 12);

    CommandButton plain(QIcon(), QStringLiteral("Cancel"));
    QCOMPARE(plain.sizeHint().width(), 12 + textWidth(&plain, QStringLiteral("Cancel")) + 12);

    // A mnemonic marker is not drawn and takes no room.
    CommandButton marked(QIcon(), QStringLiteral("&Cancel"));
    QCOMPARE(marked.sizeHint().width(), plain.sizeHint().width());
}

void RayeUiControlsTest::chevronAddsTwentyPixels()
{
    CommandButton button(someIcon(), QStringLiteral("New"));
    const int without = button.sizeHint().width();
    QMenu menu;
    button.setMenu(&menu);
    QVERIFY(button.showsChevron());
    QCOMPARE(button.sizeHint().width(), without + 8 + 12);

    // The chevron half of a split button: 28 wide.
    CommandButton chevronOnly;
    chevronOnly.setShowChevron(true);
    QCOMPARE(chevronOnly.sizeHint(), QSize(28, 32));
}

void RayeUiControlsTest::listRowsAlignLeft()
{
    CommandButton row(someIcon(), QStringLiteral("Copy link"));
    QCOMPARE(row.contentAlignment(), Qt::AlignHCenter);
    row.setContentAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QCOMPARE(row.contentAlignment(), Qt::AlignLeft);
    // The icon is drawn 12 px from the left edge, not in the middle.
    row.resize(220, 32);
    QImage image(row.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    row.render(&image);
    QCOMPARE(QColor(image.pixel(12 + 8, 16)).rgb(), QColor(Qt::red).rgb());
}

void RayeUiControlsTest::compactButtonIs24High()
{
    CommandButton button(someIcon(), QStringLiteral("Copy"));
    button.setIconOnly(true);
    button.setCompact(true);
    QCOMPARE(button.sizeHint(), QSize(32, 24));
}

void RayeUiControlsTest::subtleLooks()
{
    const TokenSet t = TokenSet::table(true);
    using L = CommandButton::Look;
    const L rest = CommandButton::look(CommandButton::Subtle, true, false, false, false, t);
    QCOMPARE(rest.fill.alpha(), 0);
    QCOMPARE(rest.ink, t.color(TokenSet::textPrimary));
    QVERIFY(!rest.border.isValid());
    QCOMPARE(CommandButton::look(CommandButton::Subtle, true, true, false, false, t).fill, t.color(TokenSet::fillSubtleHover));
    QCOMPARE(CommandButton::look(CommandButton::Subtle, true, true, true, false, t).fill, t.color(TokenSet::fillSubtlePressed));
    QCOMPARE(CommandButton::look(CommandButton::Subtle, true, false, false, true, t).fill, t.color(TokenSet::fillSubtleSelected));
    const L disabled = CommandButton::look(CommandButton::Subtle, false, true, false, false, t);
    QCOMPARE(disabled.ink, t.color(TokenSet::textDisabled));
    QCOMPARE(disabled.fill.alpha(), 0);
}

void RayeUiControlsTest::standardAndAccentLooks()
{
    for (bool night : {true, false}) {
        const TokenSet t = TokenSet::table(night);
        const auto standard = CommandButton::look(CommandButton::Standard, true, false, false, false, t);
        QCOMPARE(standard.fill, t.color(TokenSet::fillControl));
        QCOMPARE(standard.border, t.color(TokenSet::strokeControl));
        QCOMPARE(CommandButton::look(CommandButton::Standard, true, true, false, false, t).fill, t.color(TokenSet::fillControlHover));
        QCOMPARE(CommandButton::look(CommandButton::Standard, true, false, true, false, t).ink, t.color(TokenSet::textSecondary));
        QCOMPARE(CommandButton::look(CommandButton::Standard, false, false, false, false, t).ink, t.color(TokenSet::textDisabled));

        const auto accent = CommandButton::look(CommandButton::Accent, true, false, false, false, t);
        QCOMPARE(accent.fill, t.color(TokenSet::accent));
        QCOMPARE(accent.ink, t.color(TokenSet::textOnAccent));
        QCOMPARE(CommandButton::look(CommandButton::Accent, true, true, false, false, t).fill, t.color(TokenSet::accentHover));
        QCOMPARE(CommandButton::look(CommandButton::Accent, true, true, true, false, t).fill, t.color(TokenSet::accentPressed));
        QCOMPARE(CommandButton::look(CommandButton::Accent, false, false, false, false, t).fill, t.color(TokenSet::accentDisabled));

        // A toggled standard button is drawn like the accent one.
        QCOMPARE(CommandButton::look(CommandButton::Standard, true, false, false, true, t).fill, t.color(TokenSet::accent));
    }
}

void RayeUiControlsTest::tooltipSaysTheShortcut()
{
    QCOMPARE(CommandButton::toolTipFor(QStringLiteral("Copy"), QKeySequence(QStringLiteral("Ctrl+C"))), QStringLiteral("Copy (Ctrl+C)"));
    QCOMPARE(CommandButton::toolTipFor(QStringLiteral("Share"), QKeySequence()), QStringLiteral("Share"));
}

void RayeUiControlsTest::buttonFollowsItsAction()
{
    QAction action(someIcon(), QStringLiteral("&Paste"));
    action.setShortcut(QKeySequence(QStringLiteral("Ctrl+V")));
    CommandButton button;
    button.setAction(&action);
    QCOMPARE(button.text(), QStringLiteral("Paste"));
    QCOMPARE(button.shortcutHint(), QKeySequence(QStringLiteral("Ctrl+V")));
    QVERIFY(button.isEnabled());

    // Disabled, not hidden, when it cannot act.
    action.setEnabled(false);
    QVERIFY(!button.isEnabled());
    action.setEnabled(true);

    QSignalSpy triggered(&action, &QAction::triggered);
    button.click();
    QCOMPARE(triggered.count(), 1);

    QAction toggle(QStringLiteral("Details"));
    toggle.setCheckable(true);
    CommandButton details;
    details.setAction(&toggle);
    QVERIFY(details.isCheckable());
    details.click();
    QVERIFY(toggle.isChecked());
    QVERIFY(details.isChecked());
    toggle.setChecked(false);
    QVERIFY(!details.isChecked());
}

void RayeUiControlsTest::focusPolicyPerKind()
{
    CommandButton button;
    // A command bar button: Tab reaches it, a click leaves the focus where it was.
    QCOMPARE(button.focusPolicy(), Qt::TabFocus);
    button.setKind(CommandButton::Standard);
    QCOMPARE(button.focusPolicy(), Qt::StrongFocus);
    button.setKind(CommandButton::Accent);
    QCOMPARE(button.focusPolicy(), Qt::StrongFocus);
}

void RayeUiControlsTest::ringOnlyForKeyboardFocus()
{
    QWidget window;
    auto *layout = new QHBoxLayout(&window);
    auto *edit = new QLineEdit(&window);
    auto *button = new CommandButton(someIcon(), QStringLiteral("Copy"), &window);
    layout->addWidget(edit);
    layout->addWidget(button);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    FocusRing *ring = FocusRing::of(button);
    QVERIFY(ring);
    QCOMPARE(ring->parentWidget(), &window);

    button->setFocus(Qt::MouseFocusReason);
    QVERIFY(button->hasFocus());
    QVERIFY(!ring->isVisible());

    edit->setFocus(Qt::OtherFocusReason);
    button->setFocus(Qt::TabFocusReason);
    QVERIFY(ring->isVisible());
    // 3 px outside the control, plus the 2 px glow.
    QCOMPARE(ring->geometry(), button->geometry().adjusted(-5, -5, 5, 5));

    edit->setFocus(Qt::TabFocusReason);
    QVERIFY(!ring->isVisible());

    QVERIFY(FocusRing::isKeyboardReason(Qt::BacktabFocusReason));
    QVERIFY(!FocusRing::isKeyboardReason(Qt::PopupFocusReason));
}

void RayeUiControlsTest::enterPressesAButton()
{
    CommandButton button(QIcon(), QStringLiteral("Open"));
    QSignalSpy clicked(&button, &QAbstractButton::clicked);
    QTest::keyClick(&button, Qt::Key_Return);
    QTRY_COMPARE(clicked.count(), 1);
}

void RayeUiControlsTest::downOpensTheMenu()
{
    QWidget window;
    auto *button = new CommandButton(someIcon(), QStringLiteral("View"), &window);
    QMenu menu;
    menu.addAction(QStringLiteral("Details"));
    menu.addAction(QStringLiteral("List"));
    button->setMenu(&menu);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QSignalSpy shown(&menu, &QMenu::aboutToShow);
    QTest::keyClick(button, Qt::Key_Down);
    QCOMPARE(shown.count(), 1);
    QCOMPARE(menu.activeAction(), menu.actions().constFirst());
    menu.close();
}

void RayeUiControlsTest::splitButtonIsOneTabStop()
{
    SplitButton split(someIcon(), QStringLiteral("Extract all"));
    QVERIFY(split.mainButton()->focusPolicy() != Qt::NoFocus);
    QCOMPARE(split.menuButton()->focusPolicy(), Qt::NoFocus);
    QCOMPARE(split.focusProxy(), split.mainButton());
    QCOMPARE(split.menuButton()->sizeHint(), QSize(28, 32));
    QCOMPARE(split.sizeHint().width(), split.mainButton()->sizeHint().width() + 28);
    QCOMPARE(split.sizeHint().height(), 32);
    QVERIFY(!split.menuButton()->accessibleName().isEmpty());
    // One ring for both parts.
    QVERIFY(FocusRing::of(&split));
    QVERIFY(!FocusRing::of(split.mainButton()));
    split.setKind(CommandButton::Standard);
    QCOMPARE(split.menuButton()->focusPolicy(), Qt::NoFocus);
    QCOMPARE(split.menuButton()->kind(), CommandButton::Standard);
}

void RayeUiControlsTest::splitButtonF4OpensTheMenu()
{
    QWidget window;
    auto *split = new SplitButton(someIcon(), QStringLiteral("Extract all"), &window);
    QMenu menu;
    menu.addAction(QStringLiteral("Extract here"));
    split->setMenu(&menu);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QSignalSpy clicked(split, &SplitButton::clicked);
    QSignalSpy shown(&menu, &QMenu::aboutToShow);
    QTest::keyClick(split->mainButton(), Qt::Key_F4);
    QCOMPARE(shown.count(), 1);
    QCOMPARE(clicked.count(), 0);
    menu.close();
    QTest::keyClick(split->mainButton(), Qt::Key_Down, Qt::AltModifier);
    QCOMPARE(shown.count(), 2);
    menu.close();
    split->mainButton()->click();
    QCOMPARE(clicked.count(), 1);
}

void RayeUiControlsTest::separatorIsOneBy24()
{
    Separator vertical;
    QCOMPARE(vertical.sizeHint(), QSize(1, 24));
    QCOMPARE(vertical.focusPolicy(), Qt::NoFocus);
    Separator horizontal(Qt::Horizontal);
    QCOMPARE(horizontal.sizeHint().height(), 1);
    QCOMPARE(horizontal.sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
}

void RayeUiControlsTest::pivotBarKeys()
{
    PivotBar bar;
    bar.addItem(QStringLiteral("Recent"));
    bar.addItem(QStringLiteral("Favorites"));
    bar.addItem(QStringLiteral("Shared"));
    QCOMPARE(bar.height(), 40);
    QCOMPARE(bar.currentIndex(), 0);
    // Only the selected item is a Tab stop.
    QCOMPARE(bar.item(0)->focusPolicy(), Qt::TabFocus);
    QCOMPARE(bar.item(1)->focusPolicy(), Qt::NoFocus);

    QSignalSpy changed(&bar, &PivotBar::currentChanged);
    bar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bar));
    QTest::keyClick(bar.item(0), Qt::Key_Right);
    QCOMPARE(bar.currentIndex(), 1);
    QCOMPARE(bar.item(1)->focusPolicy(), Qt::TabFocus);
    QCOMPARE(bar.item(0)->focusPolicy(), Qt::NoFocus);
    QTest::keyClick(bar.item(1), Qt::Key_End);
    QCOMPARE(bar.currentIndex(), 2);
    QTest::keyClick(bar.item(2), Qt::Key_Right); // stays at the end
    QCOMPARE(bar.currentIndex(), 2);
    QTest::keyClick(bar.item(2), Qt::Key_Home);
    QCOMPARE(bar.currentIndex(), 0);
    QCOMPARE(changed.count(), 3);
    QVERIFY(bar.item(0)->isChecked());
    QVERIFY(!bar.item(2)->isChecked());

    bar.item(1)->click();
    QCOMPARE(bar.currentIndex(), 1);
}

void RayeUiControlsTest::pivotPillSitsUnderTheCurrentItem()
{
    PivotBar bar;
    bar.addItem(QStringLiteral("Recent"));
    bar.addItem(QStringLiteral("Favorites"));
    bar.resize(400, 40);
    bar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bar));
    bar.setCurrentIndex(1);
    QTRY_COMPARE(qRound(bar.pillRect().center().x()), bar.item(1)->geometry().center().x() + 1);
    QCOMPARE(bar.pillRect().size(), QSizeF(16, 3));
    QCOMPARE(qRound(bar.pillRect().bottom()), 40 - 4);
}

void RayeUiControlsTest::usageBarValueAndCaution()
{
    UsageBar bar;
    bar.setAnimateOnShow(false);
    QCOMPARE(bar.sizeHint().height(), 4);
    bar.setUsage(45, 100);
    QCOMPARE(bar.value(), 0.45);
    QVERIFY(!bar.caution());
    bar.setUsage(95, 100);
    QVERIFY(bar.caution());
    bar.setUsage(5, 0); // no total: nothing used
    QCOMPARE(bar.value(), 0.0);
    bar.setValue(2.0);
    QCOMPARE(bar.value(), 1.0);
    QVERIFY(bar.accessibleName().contains(QStringLiteral("100")));
}

void RayeUiControlsTest::flyoutOpensBelowWithAGap()
{
    const QRect screen(0, 0, 1280, 800);
    const auto p = Flyout::place(QRect(100, 100, 80, 32), QSize(200, 150), screen, Qt::BottomEdge, Qt::AlignLeft);
    QCOMPARE(p.geometry, QRect(100, 100 + 32 + 4, 200, 150));
    QCOMPARE(p.openedFrom, Qt::BottomEdge);
}

void RayeUiControlsTest::flyoutFlipsAboveWhenNoRoom()
{
    const QRect screen(0, 0, 1280, 800);
    const auto p = Flyout::place(QRect(100, 700, 80, 32), QSize(200, 150), screen, Qt::BottomEdge, Qt::AlignLeft);
    QCOMPARE(p.openedFrom, Qt::TopEdge);
    QCOMPARE(p.geometry.bottom(), 700 - 4 - 1);
    // Asked above but no room there: below.
    const auto q = Flyout::place(QRect(100, 20, 80, 32), QSize(200, 150), screen, Qt::TopEdge, Qt::AlignLeft);
    QCOMPARE(q.openedFrom, Qt::BottomEdge);
}

void RayeUiControlsTest::flyoutStaysOnScreen()
{
    const QRect screen(0, 0, 1280, 800);
    const auto p = Flyout::place(QRect(1200, 100, 60, 32), QSize(300, 150), screen, Qt::BottomEdge, Qt::AlignLeft);
    QCOMPARE(p.geometry.right(), screen.right());
    QVERIFY(screen.contains(p.geometry));
}

void RayeUiControlsTest::flyoutRightAligned()
{
    const QRect screen(0, 0, 1280, 800);
    const auto p = Flyout::place(QRect(600, 100, 80, 32), QSize(200, 100), screen, Qt::BottomEdge, Qt::AlignRight);
    QCOMPARE(p.geometry.right(), 679);
    const auto c = Flyout::place(QRect(600, 100, 80, 32), QSize(200, 100), screen, Qt::BottomEdge, Qt::AlignHCenter);
    QVERIFY(qAbs(c.geometry.center().x() - QRect(600, 100, 80, 32).center().x()) <= 1);
}

void RayeUiControlsTest::flyoutEscCloses()
{
    QWidget window;
    window.resize(300, 200);
    auto *anchor = new CommandButton(someIcon(), QStringLiteral("Share"), &window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    Flyout flyout(&window);
    auto *content = new QLineEdit;
    flyout.setContentWidget(content);
    QCOMPARE(flyout.contentWidget(), content);
    QSignalSpy closed(&flyout, &Flyout::closed);
    flyout.showAt(anchor);
    QVERIFY(flyout.isVisible());
    QCOMPARE(flyout.layout()->contentsMargins(), QMargins(4, 4, 4, 4));
    QTest::keyClick(&flyout, Qt::Key_Escape);
    QTRY_VERIFY(!flyout.isVisible());
    QCOMPARE(closed.count(), 1);
}

void RayeUiControlsTest::shadowCutsOutTheSurface()
{
    const QSize surface(100, 60);
    for (bool night : {true, false}) {
        const int margin = Shadow::margin(Elevation::Flyout, night);
        QCOMPARE(margin, night ? 38 : 28);
        const QImage image = Shadow::image(surface, 8, Elevation::Flyout, night);
        QCOMPARE(image.size(), surface + QSize(2 * margin, 2 * margin));
        // No shadow under the surface itself, some below it.
        QCOMPARE(qAlpha(image.pixel(margin + 50, margin + 30)), 0);
        QVERIFY(qAlpha(image.pixel(margin + 50, margin + 60 + 6)) > 0);
        // It falls downwards: darker below than above.
        QVERIFY(qAlpha(image.pixel(margin + 50, margin + 60 + 4)) > qAlpha(image.pixel(margin + 50, margin - 4)));
    }
    QCOMPARE(Shadow::margin(Elevation::Dialog, true), 56 + 18);
    QCOMPARE(Flyout::shadowImage(surface, true).size(), surface + QSize(76, 76));
}

void RayeUiControlsTest::cardDialogWidthAndButtons()
{
    CardDialog dialog;
    dialog.setTranslucent(false);
    dialog.setTitle(QStringLiteral("Delete file"));
    dialog.setMessage(QStringLiteral("Are you sure you want to permanently delete this file?"));
    CommandButton *yes = dialog.setButtonText(CardDialog::Primary, QStringLiteral("Yes"));
    CommandButton *no = dialog.setButtonText(CardDialog::Close, QStringLiteral("No"));
    QVERIFY(yes && no);
    QCOMPARE(yes->kind(), CommandButton::Accent);
    QCOMPARE(no->kind(), CommandButton::Standard);
    QCOMPARE(dialog.windowTitle(), QStringLiteral("Delete file"));

    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QVERIFY(dialog.width() >= CardDialog::minimumWidth);
    QVERIFY(dialog.width() <= CardDialog::maximumWidth);
    // Same width, 8 px apart, 24 px from the sides, in an 80 px row.
    QVERIFY(qAbs(yes->width() - no->width()) <= 1);
    QCOMPARE(no->x() - yes->geometry().right() - 1, 8);
    QCOMPARE(dialog.width() - (no->mapTo(&dialog, QPoint(0, 0)).x() + no->width()), 24);
    QCOMPARE(dialog.height() - dialog.buttonRowTop(), 80);
    QCOMPARE(yes->mapTo(&dialog, QPoint(0, 0)).x(), 24);
    QCOMPARE(yes->height(), 32);
    // The default button has the focus, without the ring of a Tab press.
    QVERIFY(yes->hasFocus());
    QVERIFY(!FocusRing::of(yes)->isVisible());

    // Secondary goes between them; Close makes nothing accent.
    CommandButton *later = dialog.setButtonText(CardDialog::Secondary, QStringLiteral("Later"));
    QVERIFY(later);
    QTRY_VERIFY(later->x() > yes->x() && later->x() < no->x());
    dialog.setDefaultButton(CardDialog::Close);
    QCOMPARE(yes->kind(), CommandButton::Standard);
    dialog.setDefaultButton(CardDialog::Secondary);
    QCOMPARE(later->kind(), CommandButton::Accent);

    QVERIFY(!dialog.setButtonText(CardDialog::Secondary, QString()));
    QVERIFY(!dialog.button(CardDialog::Secondary));
    dialog.close();
}

void RayeUiControlsTest::cardDialogEnterPressesTheDefault()
{
    CardDialog dialog;
    // Never hang the suite: a dialog still open after 5 s fails the test.
    QTimer::singleShot(5000, &dialog, &QDialog::reject);
    dialog.setTranslucent(false);
    dialog.setTitle(QStringLiteral("Replace or skip files"));
    dialog.setButtonText(CardDialog::Primary, QStringLiteral("Replace"));
    dialog.setButtonText(CardDialog::Close, QStringLiteral("Cancel"));
    QSignalSpy pressed(&dialog, &CardDialog::buttonClicked);
    QTimer::singleShot(0, &dialog, [&dialog]() {
        QTest::keyClick(&dialog, Qt::Key_Return);
    });
    QCOMPARE(dialog.exec(), int(CardDialog::PrimaryChosen));
    QCOMPARE(pressed.count(), 1);
    QCOMPARE(pressed.constFirst().constFirst().value<CardDialog::ButtonRole>(), CardDialog::Primary);

    // Secondary has its own result.
    dialog.setButtonText(CardDialog::Secondary, QStringLiteral("Skip"));
    QTimer::singleShot(0, &dialog, [&dialog]() {
        dialog.button(CardDialog::Secondary)->click();
    });
    QCOMPARE(dialog.exec(), int(CardDialog::SecondaryChosen));
}

void RayeUiControlsTest::cardDialogEscRejects()
{
    CardDialog dialog;
    QSignalSpy pressed(&dialog, &CardDialog::buttonClicked);
    dialog.setTranslucent(false);
    dialog.setButtonText(CardDialog::Primary, QStringLiteral("Delete"));
    QTimer::singleShot(0, &dialog, [&dialog]() {
        QTest::keyClick(&dialog, Qt::Key_Escape);
    });
    QCOMPARE(dialog.exec(), int(QDialog::Rejected));
    QCOMPARE(pressed.count(), 1);
    QCOMPARE(pressed.constFirst().constFirst().value<CardDialog::ButtonRole>(), CardDialog::Close);
}

void RayeUiControlsTest::cardDialogCanStayOpen()
{
    CardDialog dialog;
    dialog.setTranslucent(false);
    dialog.setClosesOnButton(false);
    CommandButton *retry = dialog.setButtonText(CardDialog::Primary, QStringLiteral("Try again"));
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QSignalSpy pressed(&dialog, &CardDialog::buttonClicked);
    retry->click();
    QCOMPARE(pressed.count(), 1);
    QVERIFY(dialog.isVisible());
    dialog.close();
}

void RayeUiControlsTest::motionDurations()
{
    Motion::setSpeedFactor(1.0);
    QCOMPARE(Motion::duration(Motion::Faster), 83);
    QCOMPARE(Motion::duration(Motion::Fast), 167);
    QCOMPARE(Motion::duration(Motion::Normal), 250);
    QCOMPARE(Motion::duration(Motion::Slow), 333);
    QCOMPARE(Motion::easing(Motion::Normal).type(), QEasingCurve::OutQuint);
    QCOMPARE(Motion::easing(Motion::Exit).type(), QEasingCurve::InCubic);
    Motion::setSpeedFactor(2.0);
    QCOMPARE(Motion::duration(Motion::Normal), 500);
    QCOMPARE(Tokens::instance()->durationNormal(), 500);
    Motion::setSpeedFactor(0);
    QVERIFY(!Motion::enabled());
    QCOMPARE(Motion::duration(Motion::Slow), 0);
}

void RayeUiControlsTest::motionOffRunsAtOnce()
{
    QObject owner;
    qreal last = -1;
    bool done = false;
    QVERIFY(!Motion::run(&owner, Motion::Normal, [&last](qreal t) {
        last = t;
    }, [&done]() {
        done = true;
    }));
    QCOMPARE(last, 1.0);
    QVERIFY(done);

    Motion::setSpeedFactor(1.0);
    last = -1;
    done = false;
    QVERIFY(Motion::run(&owner, Motion::Faster, [&last](qreal t) {
        last = t;
    }, [&done]() {
        done = true;
    }));
    QTRY_VERIFY(done);
    QCOMPARE(last, 1.0);

    ColorFade fade(&owner, {});
    fade.setTarget(Qt::red);
    QCOMPARE(fade.value(), QColor(Qt::red)); // no colour yet: jumps
    fade.setTarget(Qt::blue);
    QTRY_COMPARE(fade.value(), QColor(Qt::blue));
}

void RayeUiControlsTest::showcaseRegistry()
{
    int built = 0;
    QVERIFY(Showcase::add(QStringLiteral("TestThing"), QStringLiteral("a test"), [&built](QWidget *parent) {
        ++built;
        return new QWidget(parent);
    }));
    QVERIFY(!Showcase::add(QString(), QString(), [](QWidget *) {
        return nullptr;
    }));
    const Showcase::Entry *entry = Showcase::find(QStringLiteral("testthing"));
    QVERIFY(entry);
    QCOMPARE(entry->name, QStringLiteral("TestThing"));
    std::unique_ptr<QWidget> w(entry->build(nullptr));
    QVERIFY(w);
    QCOMPARE(built, 1);
    QVERIFY(!Showcase::find(QStringLiteral("NoSuchThing")));
    bool found = false;
    for (const auto &e : Showcase::entries()) {
        found = found || e.name == QLatin1String("TestThing");
    }
    QVERIFY(found);
}

QTEST_MAIN(RayeUiControlsTest)

#include "test_ui_controls.moc"
