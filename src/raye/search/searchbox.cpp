/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "searchbox.h"

#include "commandbar/commandbar.h"
#include "ui/flyout.h"
#include "ui/tokens.h"

#include <KConfigGroup>
#include <KIconEngine>
#include <KIconLoader>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

namespace Raye::Search
{
using Ui::TokenSet;
using Ui::Tokens;

namespace
{
constexpr int glyphSize = 16;

QIcon themeIcon(const char *name)
{
    return QIcon(new KIconEngine(QString::fromLatin1(name), KIconLoader::global()));
}

// The drop-down of recent searches: one row each, the pointer or the arrow
// keys pick, any other key goes back to the box so typing never stops.
class RecentList : public QWidget
{
public:
    static constexpr int rowHeight = 32;
    static constexpr int headerHeight = 28;

    RecentList(const QStringList &terms, QLineEdit *edit, std::function<void(const QString &)> pick, std::function<void()> clear, QWidget *parent)
        : QWidget(parent)
        , m_terms(terms)
        , m_edit(edit)
        , m_pick(std::move(pick))
        , m_clear(std::move(clear))
    {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(i18nc("@label", "Recent searches"));
    }

    void setWidth(int width)
    {
        setFixedSize(width, headerHeight + rowHeight * (m_terms.size() + 1) + 5);
    }

protected:
    // Rows 0 to n-1 are the searches; row n, after a 5 px gap, clears them.
    int rowAt(const QPoint &pos) const
    {
        const int y = pos.y() - headerHeight;
        const int n = int(m_terms.size());
        if (y < 0) {
            return -1;
        }
        if (y < n * rowHeight) {
            return y / rowHeight;
        }
        const int clearTop = n * rowHeight + 5;
        return y >= clearTop && y < clearTop + rowHeight ? n : -1;
    }

    void paintEvent(QPaintEvent *) override
    {
        const TokenSet &t = Tokens::current();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        p.setFont(Ui::textFont(Ui::TextStyle::Caption));
        p.setPen(t.color(TokenSet::textSecondary));
        p.drawText(QRect(12, 0, width() - 24, headerHeight), Qt::AlignVCenter | Qt::AlignLeft, i18nc("@title", "Recent searches"));

        p.setFont(Ui::textFont(Ui::TextStyle::Body));
        const QFontMetrics fm(p.font());
        for (int i = 0; i <= m_terms.size(); ++i) {
            const bool clearRow = i == m_terms.size();
            int top = headerHeight + i * rowHeight;
            if (clearRow) {
                top += 5;
                p.setPen(QPen(t.color(TokenSet::strokeDivider), 1));
                p.drawLine(QPointF(4, top - 3.5), QPointF(width() - 4, top - 3.5));
            }
            const QRect row(0, top, width(), rowHeight);
            if (i == m_current) {
                p.setPen(Qt::NoPen);
                p.setBrush(t.color(TokenSet::fillSubtleHover));
                p.drawRoundedRect(QRectF(row).adjusted(0, 1, 0, -1), Ui::Metrics::radiusControl, Ui::Metrics::radiusControl);
            }
            const QRect icon(12, row.center().y() - 7, 16, 16);
            themeIcon(clearRow ? "edit-clear-history" : "document-open-recent").paint(&p, icon);
            p.setPen(t.color(clearRow ? TokenSet::textSecondary : TokenSet::textPrimary));
            const QString text = clearRow ? i18nc("@action:inmenu", "Clear search history") : m_terms.at(i);
            p.drawText(QRect(40, top, width() - 52, rowHeight), Qt::AlignVCenter | Qt::AlignLeft, fm.elidedText(text, Qt::ElideRight, width() - 52));
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        setCurrent(rowAt(event->position().toPoint()));
    }

    void leaveEvent(QEvent *) override
    {
        setCurrent(-1);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            activate(rowAt(event->position().toPoint()));
        }
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        switch (event->key()) {
        case Qt::Key_Down:
            setCurrent(m_current >= m_terms.size() ? 0 : m_current + 1);
            return;
        case Qt::Key_Up:
            setCurrent(m_current <= 0 ? m_terms.size() : m_current - 1);
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            activate(m_current);
            return;
        case Qt::Key_Escape:
            close();
            return;
        default:
            break;
        }
        // Typing goes on in the box.
        if (!event->text().isEmpty() || event->key() == Qt::Key_Backspace) {
            QPointer<QLineEdit> edit = m_edit;
            QKeyEvent copy(event->type(), event->key(), event->modifiers(), event->text(), event->isAutoRepeat(), event->count());
            close();
            if (edit) {
                edit->setFocus(Qt::OtherFocusReason);
                QApplication::sendEvent(edit, &copy);
            }
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    void setCurrent(int row)
    {
        if (m_current != row) {
            m_current = row;
            update();
        }
    }

    void close()
    {
        if (auto *flyout = qobject_cast<Ui::Flyout *>(window())) {
            flyout->dismiss();
        }
    }

    void activate(int row)
    {
        if (row < 0 || row > m_terms.size()) {
            return;
        }
        if (row == m_terms.size()) {
            m_clear();
        } else {
            m_pick(m_terms.at(row));
        }
        close();
    }

    QStringList m_terms;
    QPointer<QLineEdit> m_edit;
    std::function<void(const QString &)> m_pick;
    std::function<void()> m_clear;
    int m_current = -1;
};
}

SearchBox::SearchBox(QWidget *parent)
    : QWidget(parent)
    , m_edit(new QLineEdit(this))
    , m_debounce(new QTimer(this))
    , m_optionsMenu(new QMenu(this))
    , m_optionsAction(new QAction(this))
    , m_closeAction(new QAction(this))
{
    setAttribute(Qt::WA_Hover);
    setFocusProxy(m_edit);
    setCursor(Qt::IBeamCursor);

    m_edit->setFrame(false);
    m_edit->setFont(Ui::textFont(Ui::TextStyle::Body));
    m_edit->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_edit->setAccessibleName(i18nc("@label the search box", "Search"));
    m_edit->installEventFilter(this);
    connect(m_edit, &QLineEdit::textEdited, this, &SearchBox::textEdited);
    connect(m_edit, &QLineEdit::textChanged, this, qOverload<>(&QWidget::update));

    m_debounce->setSingleShot(true);
    m_debounce->setInterval(debounceMs);
    connect(m_debounce, &QTimer::timeout, this, [this] {
        run(false);
    });

    m_optionsAction->setObjectName(QStringLiteral("raye_search_options"));
    m_optionsAction->setText(i18nc("@action:button the search options menu", "Search options"));
    m_optionsAction->setIcon(themeIcon("view-filter"));
    m_optionsAction->setMenu(m_optionsMenu);
    m_closeAction->setObjectName(QStringLiteral("raye_close_search"));
    m_closeAction->setText(i18nc("@action:button", "Close search"));
    m_closeAction->setIcon(themeIcon("window-close"));
    connect(m_closeAction, &QAction::triggered, this, &SearchBox::closeSearch);
    buildOptionsMenu();

    connect(Tokens::instance(), &Tokens::changed, this, [this] {
        update();
        changeEvent(nullptr);
    });
    changeEvent(nullptr);
    setFolder(QUrl(), QString());
}

SearchBox::~SearchBox() = default;

void SearchBox::setUrlBuilder(const UrlBuilder &builder)
{
    m_builder = builder;
}

QString SearchBox::placeholder() const
{
    return placeholderText(m_folderName);
}

void SearchBox::setFolder(const QUrl &folder, const QString &displayName)
{
    // The results themselves: the folder searched stays the same.
    if (isSearchUrl(folder)) {
        return;
    }
    const bool moved = !folder.matches(m_folder, QUrl::StripTrailingSlash);
    m_folder = folder;
    m_folderName = displayName;
    m_edit->setPlaceholderText(placeholder());
    if (moved && (m_searching || !m_edit->text().isEmpty())) {
        // The window went somewhere else: the search is over, quietly.
        m_debounce->stop();
        m_searching = false;
        m_edit->clear();
        m_options.term.clear();
    }
}

void SearchBox::setOptions(const Options &options)
{
    m_options = options;
    if (m_edit->text() != options.term) {
        m_edit->setText(options.term);
    }
    syncOptionsMenu();
}

void SearchBox::setConfig(const KSharedConfig::Ptr &config)
{
    m_config = config;
}

KConfigGroup SearchBox::rayeGroup() const
{
    return KConfigGroup(m_config ? m_config : KSharedConfig::openConfig(QStringLiteral("dolphinrc")), QStringLiteral("Raye"));
}

QStringList SearchBox::recentSearches() const
{
    return rayeGroup().readEntry("RecentSearches", QStringList());
}

void SearchBox::clearRecentSearches()
{
    KConfigGroup group = rayeGroup();
    group.deleteEntry("RecentSearches", KConfig::Notify);
    group.sync();
}

void SearchBox::remember(const QString &term)
{
    const QStringList before = recentSearches();
    const QStringList after = addRecent(before, term);
    if (after != before) {
        KConfigGroup group = rayeGroup();
        group.writeEntry("RecentSearches", after, KConfig::Notify);
        group.sync();
    }
}

void SearchBox::focusSearch()
{
    m_edit->setFocus(Qt::ShortcutFocusReason);
    m_edit->selectAll();
    if (m_edit->text().isEmpty()) {
        showRecent();
    }
}

void SearchBox::textEdited()
{
    m_options.term = m_edit->text();
    if (m_recentFlyout) {
        m_recentFlyout->dismiss();
    }
    if (!m_options.isActive()) {
        m_debounce->stop();
        if (m_searching) {
            closeSearch();
        }
        return;
    }
    m_debounce->start();
}

void SearchBox::searchNow()
{
    m_debounce->stop();
    m_options.term = m_edit->text();
    run(true);
}

void SearchBox::run(bool rememberTerm)
{
    if (!m_options.isActive()) {
        closeSearch();
        return;
    }
    if (m_options.scope == Scope::CurrentFolder) {
        m_searching = true;
        Q_EMIT filterRequested(m_options.trimmedTerm(), kindMimeTypes(m_options.kind));
    } else {
        if (m_folder.isEmpty()) {
            return;
        }
        const QUrl url = m_builder ? m_builder(m_options, m_folder) : fileNameSearchUrl(m_options, m_folder);
        if (!url.isValid()) {
            return;
        }
        m_searching = true;
        Q_EMIT searchRequested(url);
    }
    if (rememberTerm) {
        remember(m_options.trimmedTerm());
    }
    syncOptionsMenu();
}

void SearchBox::closeSearch()
{
    m_debounce->stop();
    if (m_recentFlyout) {
        m_recentFlyout->dismiss();
    }
    m_edit->clear();
    m_options.term.clear();
    // The filters go with the search; where to look and file contents stay.
    m_options.kind = Kind::Any;
    m_options.date = DateRange::Any;
    m_options.size = SizeRange::Any;
    const bool wasSearching = m_searching;
    m_searching = false;
    syncOptionsMenu();
    if (wasSearching) {
        Q_EMIT searchClosed();
    }
}

bool SearchBox::acceptsResult(const QMimeType &mime, bool isDir, const QDateTime &modified, qint64 size) const
{
    return Raye::Search::acceptsResult(m_options, mime, isDir, modified, size, QDate::currentDate(), QLocale().firstDayOfWeek());
}

void SearchBox::changeOptions(const std::function<void(Options &)> &change)
{
    Options next = m_options;
    next.term = m_edit->text();
    change(next);
    if (next == m_options) {
        return;
    }
    const bool scopeChanged = next.scope != m_options.scope;
    const bool wasFiltering = m_searching && m_options.scope == Scope::CurrentFolder;
    m_options = next;
    Q_EMIT optionsChanged();
    if (scopeChanged && wasFiltering) {
        // Leaving "Current folder": take the filter off before searching.
        Q_EMIT filterRequested(QString(), {});
    }
    if (m_searching || m_options.isActive()) {
        run(false);
    }
    syncOptionsMenu();
}

void SearchBox::buildOptionsMenu()
{
    QMenu *menu = m_optionsMenu;
    menu->setTitle(i18nc("@title:menu", "Search options"));

    auto radio = [this](QMenu *into, QActionGroup *group, const QString &text, int value, const char *name) {
        QAction *a = into->addAction(text);
        a->setObjectName(QString::fromLatin1(name));
        a->setCheckable(true);
        a->setData(value);
        a->setActionGroup(group);
        return a;
    };

    QMenu *date = menu->addMenu(themeIcon("view-calendar"), i18nc("@title:menu", "Date modified"));
    date->menuAction()->setObjectName(QStringLiteral("raye_search_date"));
    m_dateGroup = new QActionGroup(this);
    radio(date, m_dateGroup, i18nc("@action:inmenu date modified", "Any time"), int(DateRange::Any), "date_any");
    date->addSeparator();
    radio(date, m_dateGroup, i18nc("@action:inmenu date modified", "Today"), int(DateRange::Today), "date_today");
    radio(date, m_dateGroup, i18nc("@action:inmenu date modified", "Yesterday"), int(DateRange::Yesterday), "date_yesterday");
    radio(date, m_dateGroup, i18nc("@action:inmenu date modified", "This week"), int(DateRange::ThisWeek), "date_this_week");
    radio(date, m_dateGroup, i18nc("@action:inmenu date modified", "Last week"), int(DateRange::LastWeek), "date_last_week");
    radio(date, m_dateGroup, i18nc("@action:inmenu date modified", "This month"), int(DateRange::ThisMonth), "date_this_month");
    radio(date, m_dateGroup, i18nc("@action:inmenu date modified", "Last month"), int(DateRange::LastMonth), "date_last_month");
    radio(date, m_dateGroup, i18nc("@action:inmenu date modified", "This year"), int(DateRange::ThisYear), "date_this_year");
    radio(date, m_dateGroup, i18nc("@action:inmenu date modified", "Last year"), int(DateRange::LastYear), "date_last_year");
    connect(m_dateGroup, &QActionGroup::triggered, this, [this](QAction *a) {
        changeOptions([a](Options &o) {
            o.date = DateRange(a->data().toInt());
        });
    });

    QMenu *kind = menu->addMenu(themeIcon("folder-documents"), i18nc("@title:menu file kind", "Kind"));
    kind->menuAction()->setObjectName(QStringLiteral("raye_search_kind"));
    m_kindGroup = new QActionGroup(this);
    radio(kind, m_kindGroup, i18nc("@action:inmenu file kind", "Any kind"), int(Kind::Any), "kind_any");
    kind->addSeparator();
    radio(kind, m_kindGroup, i18nc("@action:inmenu file kind", "Folder"), int(Kind::Folder), "kind_folder");
    radio(kind, m_kindGroup, i18nc("@action:inmenu file kind", "Document"), int(Kind::Document), "kind_document");
    radio(kind, m_kindGroup, i18nc("@action:inmenu file kind", "Picture"), int(Kind::Picture), "kind_picture");
    radio(kind, m_kindGroup, i18nc("@action:inmenu file kind", "Music"), int(Kind::Music), "kind_music");
    radio(kind, m_kindGroup, i18nc("@action:inmenu file kind", "Video"), int(Kind::Video), "kind_video");
    radio(kind, m_kindGroup, i18nc("@action:inmenu file kind", "Compressed folder"), int(Kind::Archive), "kind_archive");
    connect(m_kindGroup, &QActionGroup::triggered, this, [this](QAction *a) {
        changeOptions([a](Options &o) {
            o.kind = Kind(a->data().toInt());
        });
    });

    QMenu *size = menu->addMenu(themeIcon("drive-harddisk"), i18nc("@title:menu file size", "Size"));
    size->menuAction()->setObjectName(QStringLiteral("raye_search_size"));
    m_sizeGroup = new QActionGroup(this);
    radio(size, m_sizeGroup, i18nc("@action:inmenu file size", "Any size"), int(SizeRange::Any), "size_any");
    size->addSeparator();
    radio(size, m_sizeGroup, i18nc("@action:inmenu file size", "Empty (0 KB)"), int(SizeRange::Empty), "size_empty");
    radio(size, m_sizeGroup, i18nc("@action:inmenu file size", "Tiny (0 – 16 KB)"), int(SizeRange::Tiny), "size_tiny");
    radio(size, m_sizeGroup, i18nc("@action:inmenu file size", "Small (16 KB – 1 MB)"), int(SizeRange::Small), "size_small");
    radio(size, m_sizeGroup, i18nc("@action:inmenu file size", "Medium (1 – 128 MB)"), int(SizeRange::Medium), "size_medium");
    radio(size, m_sizeGroup, i18nc("@action:inmenu file size", "Large (128 MB – 1 GB)"), int(SizeRange::Large), "size_large");
    radio(size, m_sizeGroup, i18nc("@action:inmenu file size", "Huge (1 – 4 GB)"), int(SizeRange::Huge), "size_huge");
    radio(size, m_sizeGroup, i18nc("@action:inmenu file size", "Gigantic (> 4 GB)"), int(SizeRange::Gigantic), "size_gigantic");
    connect(m_sizeGroup, &QActionGroup::triggered, this, [this](QAction *a) {
        changeOptions([a](Options &o) {
            o.size = SizeRange(a->data().toInt());
        });
    });

    menu->addSeparator();
    m_contentsAction = menu->addAction(i18nc("@action:inmenu", "Search file contents"));
    m_contentsAction->setObjectName(QStringLiteral("raye_search_contents"));
    m_contentsAction->setCheckable(true);
    connect(m_contentsAction, &QAction::toggled, this, [this](bool on) {
        changeOptions([on](Options &o) {
            o.fileContents = on;
        });
    });

    menu->addSeparator();
    m_scopeGroup = new QActionGroup(this);
    radio(menu, m_scopeGroup, i18nc("@action:inmenu where to search", "Current folder"), int(Scope::CurrentFolder), "scope_current");
    radio(menu, m_scopeGroup, i18nc("@action:inmenu where to search", "All subfolders"), int(Scope::AllSubfolders), "scope_all");
    connect(m_scopeGroup, &QActionGroup::triggered, this, [this](QAction *a) {
        changeOptions([a](Options &o) {
            o.scope = Scope(a->data().toInt());
        });
    });

    menu->addSeparator();
    menu->addAction(m_closeAction);
    syncOptionsMenu();
}

void SearchBox::syncOptionsMenu()
{
    auto check = [](QActionGroup *group, int value) {
        for (QAction *a : group->actions()) {
            if (a->data().toInt() == value && !a->isChecked()) {
                a->setChecked(true);
            }
        }
    };
    check(m_dateGroup, int(m_options.date));
    check(m_kindGroup, int(m_options.kind));
    check(m_sizeGroup, int(m_options.size));
    check(m_scopeGroup, int(m_options.scope));
    {
        const QSignalBlocker blocker(m_contentsAction);
        m_contentsAction->setChecked(m_options.fileContents);
    }
    m_closeAction->setEnabled(m_searching || !m_edit->text().isEmpty());
}

void SearchBox::registerWith(Commands::CommandBar *bar)
{
    if (!bar) {
        return;
    }
    bar->registerSet(QStringLiteral("search"), {m_optionsAction, m_closeAction}, [](const Commands::CommandContext &context) {
        return context.searching;
    });
}

void SearchBox::showRecent()
{
    const QStringList terms = recentSearches();
    if (terms.isEmpty() || m_recentFlyout || !isVisible()) {
        return;
    }
    auto *flyout = new Ui::Flyout(this);
    flyout->setAttribute(Qt::WA_DeleteOnClose);
    auto *list = new RecentList(
        terms,
        m_edit,
        [this](const QString &term) {
            pickRecent(term);
        },
        [this] {
            clearRecentSearches();
        },
        flyout);
    list->setWidth(boxRect().width() - 2 * flyout->padding());
    flyout->setContentWidget(list);
    m_recentFlyout = flyout;
    flyout->showAt(QRect(mapToGlobal(boxRect().topLeft()), boxRect().size()), Qt::BottomEdge, Qt::AlignLeft);
    list->setFocus(Qt::PopupFocusReason);
}

void SearchBox::pickRecent(const QString &term)
{
    m_edit->setText(term);
    m_options.term = term;
    m_edit->setFocus(Qt::OtherFocusReason);
    searchNow();
}

bool SearchBox::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit) {
        switch (event->type()) {
        case QEvent::KeyPress: {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                searchNow();
                return true;
            }
            if (key->key() == Qt::Key_Escape) {
                if (m_searching || !m_edit->text().isEmpty()) {
                    closeSearch();
                } else {
                    m_edit->clearFocus();
                }
                return true;
            }
            if (key->key() == Qt::Key_Down && m_edit->text().isEmpty()) {
                showRecent();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonPress:
            if (m_edit->text().isEmpty() && !m_edit->hasFocus()) {
                QTimer::singleShot(0, this, &SearchBox::showRecent);
            }
            break;
        case QEvent::FocusIn:
        case QEvent::FocusOut:
            if (event->type() == QEvent::FocusOut && m_searching && !m_recentFlyout) {
                // Leaving the box keeps the words for next time.
                remember(m_options.trimmedTerm());
            }
            update();
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QRect SearchBox::boxRect() const
{
    return QRect(glowMargin, (height() - boxHeight) / 2, width() - 2 * glowMargin, boxHeight);
}

void SearchBox::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const QRect box = boxRect();
    const int right = innerPadding + glyphSize + 6;
    m_edit->setGeometry(box.left() + innerPadding, box.top() + 1, box.width() - innerPadding - right, box.height() - 2);
}

QSize SearchBox::sizeHint() const
{
    return QSize(240, boxHeight + 2 * glowMargin);
}

QSize SearchBox::minimumSizeHint() const
{
    return QSize(160, boxHeight + 2 * glowMargin);
}

void SearchBox::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void SearchBox::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void SearchBox::changeEvent(QEvent *event)
{
    if (event) {
        QWidget::changeEvent(event);
        if (event->type() != QEvent::PaletteChange && event->type() != QEvent::StyleChange) {
            return;
        }
    }
    // The field is drawn by the box: the line edit only shows the words.
    const TokenSet &t = Tokens::current();
    QPalette pal = m_edit->palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    pal.setColor(QPalette::Text, t.color(TokenSet::textPrimary));
    pal.setColor(QPalette::PlaceholderText, t.color(TokenSet::textSecondary));
    pal.setColor(QPalette::Highlight, t.color(TokenSet::accent));
    pal.setColor(QPalette::HighlightedText, t.color(TokenSet::textOnAccent));
    m_edit->setPalette(pal);
}

void SearchBox::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const TokenSet &t = Tokens::current();
    const QRectF box = QRectF(boxRect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const bool focused = m_edit->hasFocus();
    const int radius = Ui::Metrics::radiusControl;

    // The same field as the address box: soft glow when focused (night).
    if (focused && t.night()) {
        const QColor base = t.color(TokenSet::glow);
        for (int k = glowMargin; k >= 1; --k) {
            QColor glow = base;
            glow.setAlphaF(base.alphaF() * (1.0 - qreal(k) / (glowMargin + 1)));
            p.setPen(QPen(glow, 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(box.adjusted(-k, -k, k, k), radius + k, radius + k);
        }
    }
    p.setPen(QPen(t.color(TokenSet::strokeControl), 1));
    p.setBrush(t.color(focused ? TokenSet::fillInputActive : (m_hovered ? TokenSet::fillControlHover : TokenSet::fillControl)));
    p.drawRoundedRect(box, radius, radius);

    QPainterPath clip;
    clip.addRoundedRect(box, radius, radius);
    p.save();
    p.setClipPath(clip);
    if (focused) {
        p.fillRect(QRectF(box.left(), box.bottom() - focusStroke + 0.5, box.width(), focusStroke), t.color(TokenSet::accent));
    } else {
        p.fillRect(QRectF(box.left(), box.bottom(), box.width(), 1), t.color(TokenSet::strokeControlBottom));
    }
    p.restore();

    // The magnifier on the right.
    const QRectF glyph(box.right() - innerPadding - glyphSize, box.center().y() - glyphSize / 2.0, glyphSize, glyphSize);
    QPen pen(t.color(m_searching ? TokenSet::accentText : TokenSet::textSecondary), 1.3);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const QRectF lens(glyph.left() + 2, glyph.top() + 2, 9, 9);
    p.drawEllipse(lens);
    p.drawLine(QPointF(lens.right() - 1, lens.bottom() - 1), QPointF(glyph.right() - 2.5, glyph.bottom() - 2.5));
}

}
