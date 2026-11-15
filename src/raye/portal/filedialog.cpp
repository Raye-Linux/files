/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "filedialog.h"

#include "addressbar/addressbar.h"
#include "kitemviews/kfileitemmodel.h"
#include "navpane/navigationpane.h"
#include "ui/carddialog.h"
#include "ui/commandbutton.h"
#include "ui/showcase.h"
#include "ui/tokens.h"
#include "views/dolphinview.h"

#include <KConfigGroup>
#include <KFileItem>
#include <KIO/MkdirJob>
#include <KIO/StatJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KUrlNavigator>
#include <KWindowConfig>

#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QSplitter>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace Raye::Portal
{

using Raye::Ui::CardDialog;
using Raye::Ui::CommandButton;

namespace
{
constexpr QSize defaultSize(800, 560);
constexpr int acceptWidth = 120;
constexpr int paneWidth = 200;

const char *configGroup()
{
    return "RayeFileDialog";
}

QUrl startFolder(const FileRequest &request)
{
    if (request.folder.isValid() && (!request.folder.isLocalFile() || QFileInfo(request.folder.toLocalFile()).isDir())) {
        return request.folder;
    }
    const KConfigGroup group = KSharedConfig::openStateConfig()->group(QLatin1String(configGroup()));
    const QUrl last = group.readEntry("LastFolder", QUrl());
    if (last.isLocalFile() && QFileInfo(last.toLocalFile()).isDir()) {
        return last;
    }
    return QUrl::fromLocalFile(QDir::homePath());
}

bool askBeforeReplacing(QWidget *parent, const QStringList &existing)
{
    CardDialog card(parent);
    card.setTitle(i18nc("@title:window", "Confirm Save As"));
    card.setMessage(existing.size() == 1 ? i18nc("@info %1 is a file name", "%1 already exists. Do you want to replace it?", existing.first())
                                         : i18ncp("@info", "%1 file already exists. Do you want to replace it?", "%1 files already exist. Do you want to replace them?", existing.size()));
    card.setButtonText(CardDialog::Primary, i18nc("@action:button replace the file", "Yes"));
    card.setButtonText(CardDialog::Close, i18nc("@action:button keep the file", "No"));
    // The safe answer is the one Enter gives.
    card.setDefaultButton(CardDialog::Close);
    return card.exec() == CardDialog::PrimaryChosen;
}
}

FileDialog::FileDialog(const FileRequest &request, QWidget *parent)
    : QDialog(parent)
    , m_request(request)
{
    setObjectName(QStringLiteral("RayeFileDialog"));
    QString title = request.title;
    if (title.isEmpty()) {
        switch (request.mode) {
        case Mode::OpenFile:
            title = i18nc("@title:window", "Open");
            break;
        case Mode::OpenFolder:
            title = i18nc("@title:window", "Select folder");
            break;
        case Mode::SaveFile:
        case Mode::SaveFiles:
            title = i18nc("@title:window", "Save as");
            break;
        }
    }
    setWindowTitle(title);

    m_confirm = [this](const QStringList &existing) {
        return askBeforeReplacing(this, existing);
    };

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_view = new DolphinView(startFolder(request), this);
    buildTop();
    buildBottom();

    KConfigGroup group = KSharedConfig::openStateConfig()->group(QLatin1String(configGroup()));
    resize(defaultSize);
    create();
    KWindowConfig::restoreWindowSize(windowHandle(), group);
    resize(windowHandle()->size());

    applyType();
    updateAcceptButton();
    QTimer::singleShot(0, this, [this] {
        if (m_field && m_field->isVisible()) {
            m_field->setFocus(Qt::OtherFocusReason);
            // Save: the name without its extension is ready to be typed over.
            const int dot = m_field->text().lastIndexOf(QLatin1Char('.'));
            m_field->setSelection(0, dot > 0 ? dot : m_field->text().size());
        }
    });
}

FileDialog::~FileDialog() = default;

void FileDialog::buildTop()
{
    auto *outer = qobject_cast<QVBoxLayout *>(layout());
    const QUrl start = m_view->url();

    m_address = new Raye::Address::AddressBar(this);
    m_address->setUrl(start);
    outer->addWidget(m_address);
    connect(m_address, &Raye::Address::AddressBar::refreshRequested, m_view, &DolphinView::reload);

    // Narrows the folder on screen as one types.
    auto *search = new QLineEdit(this);
    search->setObjectName(QStringLiteral("RayeFileDialogSearch"));
    search->setPlaceholderText(i18nc("@info:placeholder", "Search this folder"));
    search->setAccessibleName(search->placeholderText());
    search->setClearButtonEnabled(true);
    connect(search, &QLineEdit::textChanged, m_view, &DolphinView::setNameFilter);
    connect(m_view, &DolphinView::urlChanged, search, &QLineEdit::clear);
    m_address->setTrailingWidget(search);

    auto *commands = new QHBoxLayout;
    commands->setContentsMargins(8, 0, 8, 8);
    commands->setSpacing(4);
    auto *newFolder = new CommandButton(QIcon::fromTheme(QStringLiteral("folder-new")), i18nc("@action:button", "New folder"), this);
    connect(newFolder, &QAbstractButton::clicked, this, &FileDialog::createFolder);
    commands->addWidget(newFolder);

    auto *viewButton = new CommandButton(QIcon::fromTheme(QStringLiteral("view-list-details")), i18nc("@action:button how the files are shown", "View"), this);
    auto *viewMenu = new QMenu(viewButton);
    auto *modes = new QActionGroup(viewMenu);
    const auto addMode = [this, viewMenu, modes](const QString &text, const QString &icon, DolphinView::Mode mode) {
        QAction *action = viewMenu->addAction(QIcon::fromTheme(icon), text);
        action->setCheckable(true);
        action->setChecked(m_view->viewMode() == mode);
        modes->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode] {
            m_view->setViewMode(mode);
        });
    };
    addMode(i18nc("@action:inmenu view mode", "Details"), QStringLiteral("view-list-details"), DolphinView::DetailsView);
    addMode(i18nc("@action:inmenu view mode", "Icons"), QStringLiteral("view-list-icons"), DolphinView::IconsView);
    addMode(i18nc("@action:inmenu view mode", "List"), QStringLiteral("view-list-text"), DolphinView::CompactView);
    viewMenu->addSeparator();
    QAction *hidden = viewMenu->addAction(i18nc("@action:inmenu", "Hidden items"));
    hidden->setCheckable(true);
    hidden->setChecked(m_view->hiddenFilesShown());
    connect(hidden, &QAction::toggled, m_view, &DolphinView::setHiddenFilesShown);
    viewButton->setMenu(viewMenu);
    commands->addWidget(viewButton);
    commands->addStretch(1);
    outer->addLayout(commands);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    m_pane = new Raye::NavPane::NavigationPane(this);
    m_pane->setCurrentUrl(start);
    splitter->addWidget(m_pane);
    splitter->addWidget(m_view);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({paneWidth, defaultSize.width() - paneWidth});
    outer->addWidget(splitter, 1);

    // The view is the location; the address and the pane follow and drive it.
    connect(m_view, &DolphinView::urlChanged, this, [this](const QUrl &url) {
        if (m_address->url() != url) {
            m_address->setUrl(url);
        }
        m_pane->setCurrentUrl(url);
    });
    connect(m_address->navigator(), &KUrlNavigator::urlChanged, this, [this] {
        m_view->setUrl(m_address->url());
    });
    connect(m_pane, &Raye::NavPane::NavigationPane::urlActivated, this, [this](const QUrl &url) {
        m_view->setUrl(url);
    });
    connect(m_view, &DolphinView::selectionChanged, this, &FileDialog::selectionChanged);
    connect(m_view, &DolphinView::itemActivated, this, &FileDialog::itemActivated);
    connect(m_view, &DolphinView::itemsActivated, this, [this] {
        tryAccept();
    });
    connect(m_view, &DolphinView::redirection, this, [this](const QUrl &, const QUrl &newUrl) {
        m_address->setUrl(newUrl);
    });
}

void FileDialog::buildBottom()
{
    auto *outer = qobject_cast<QVBoxLayout *>(layout());
    auto *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("RayeFileDialogBottom"));
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 12, 16, 16);
    panelLayout->setSpacing(10);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(1, 1);
    int row = 0;

    const bool saving = m_request.mode == Mode::SaveFile;
    if (m_request.mode != Mode::SaveFiles) {
        auto *label = new QLabel(m_request.mode == Mode::OpenFolder ? i18nc("@label:textbox", "Folder:") : i18nc("@label:textbox", "File name:"), panel);
        m_field = new QLineEdit(panel);
        m_field->setClearButtonEnabled(true);
        label->setBuddy(m_field);
        m_field->setAccessibleName(label->text());
        if (saving) {
            m_field->setText(m_request.name);
        }
        grid->addWidget(label, row, 0);
        grid->addWidget(m_field, row, 1);
        connect(m_field, &QLineEdit::returnPressed, this, [this] {
            tryAccept();
        });
        connect(m_field, &QLineEdit::textChanged, this, [this] {
            updateAcceptButton();
        });
    } else {
        auto *label = new QLabel(i18ncp("@info", "%1 file will be saved in the folder on screen.", "%1 files will be saved in the folder on screen.", m_request.names.size()), panel);
        grid->addWidget(label, row, 0, 1, 2);
    }

    if (!m_request.types.isEmpty()) {
        m_types = new QComboBox(panel);
        m_types->setAccessibleName(saving ? i18nc("@label:listbox", "Save as type:") : i18nc("@label:listbox", "Type of file:"));
        for (const FileType &type : std::as_const(m_request.types)) {
            m_types->addItem(type.displayName());
        }
        m_types->setCurrentIndex(qBound(0, m_request.currentType, int(m_request.types.size()) - 1));
        connect(m_types, &QComboBox::currentIndexChanged, this, [this] {
            applyType();
        });
        if (saving) {
            ++row;
            auto *label = new QLabel(i18nc("@label:listbox", "Save as type:"), panel);
            label->setBuddy(m_types);
            grid->addWidget(label, row, 0);
            grid->addWidget(m_types, row, 1);
        } else {
            m_types->setMinimumWidth(220);
            grid->addWidget(m_types, row, 2);
        }
    }
    ++row;

    for (const Choice &choice : std::as_const(m_request.choices)) {
        if (choice.options.isEmpty()) {
            auto *check = new QCheckBox(choice.label, panel);
            check->setChecked(choice.initial == QLatin1String("true"));
            grid->addWidget(check, row, 1);
            m_choiceChecks.insert(choice.id, check);
        } else {
            auto *label = new QLabel(choice.label, panel);
            auto *list = new QComboBox(panel);
            label->setBuddy(list);
            for (const ChoiceOption &option : choice.options) {
                list->addItem(option.label, option.id);
                if (option.id == choice.initial) {
                    list->setCurrentIndex(list->count() - 1);
                }
            }
            grid->addWidget(label, row, 0);
            grid->addWidget(list, row, 1);
            m_choiceLists.insert(choice.id, list);
        }
        ++row;
    }
    panelLayout->addLayout(grid);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addStretch(1);
    QString acceptText = cleanLabel(m_request.acceptLabel);
    if (acceptText.isEmpty()) {
        switch (m_request.mode) {
        case Mode::OpenFile:
            acceptText = i18nc("@action:button", "Open");
            break;
        case Mode::OpenFolder:
            acceptText = i18nc("@action:button", "Select folder");
            break;
        case Mode::SaveFile:
        case Mode::SaveFiles:
            acceptText = i18nc("@action:button", "Save");
            break;
        }
    }
    m_accept = new CommandButton(panel);
    m_accept->setKind(CommandButton::Accent);
    m_accept->setText(acceptText);
    m_accept->setMinimumWidth(acceptWidth);
    connect(m_accept, &QAbstractButton::clicked, this, [this] {
        tryAccept();
    });
    m_cancel = new CommandButton(panel);
    m_cancel->setKind(CommandButton::Standard);
    m_cancel->setText(i18nc("@action:button", "Cancel"));
    m_cancel->setMinimumWidth(acceptWidth);
    connect(m_cancel, &QAbstractButton::clicked, this, &QDialog::reject);
    buttons->addWidget(m_accept);
    buttons->addWidget(m_cancel);
    panelLayout->addLayout(buttons);

    outer->addWidget(panel);
}

FileType FileDialog::type() const
{
    if (m_types && m_types->currentIndex() >= 0 && m_types->currentIndex() < m_request.types.size()) {
        return m_request.types.at(m_types->currentIndex());
    }
    return {};
}

int FileDialog::currentType() const
{
    return m_types ? m_types->currentIndex() : -1;
}

QHash<QString, QString> FileDialog::choiceValues() const
{
    QHash<QString, QString> values;
    for (auto it = m_choiceLists.cbegin(); it != m_choiceLists.cend(); ++it) {
        values.insert(it.key(), it.value()->currentData().toString());
    }
    for (auto it = m_choiceChecks.cbegin(); it != m_choiceChecks.cend(); ++it) {
        values.insert(it.key(), it.value()->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
    }
    return values;
}

QStringList FileDialog::shownNames() const
{
    QStringList names;
    if (const auto *model = m_view->findChild<KFileItemModel *>()) {
        for (int i = 0; i < model->count(); ++i) {
            names.append(model->fileItem(i).name());
        }
    }
    names.sort();
    return names;
}

void FileDialog::setCurrentType(int index)
{
    if (m_types) {
        m_types->setCurrentIndex(index);
    }
}

QUrl FileDialog::folder() const
{
    return m_view->url();
}

void FileDialog::setFolder(const QUrl &url)
{
    m_view->setUrl(url);
}

QString FileDialog::fieldText() const
{
    return m_field ? m_field->text() : QString();
}

void FileDialog::setFieldText(const QString &text)
{
    if (m_field) {
        m_field->setText(text);
    }
}

void FileDialog::applyType()
{
    auto *model = m_view->findChild<KFileItemModel *>();
    if (!model) {
        return;
    }
    if (m_request.mode == Mode::OpenFolder || m_request.mode == Mode::SaveFiles) {
        // Only folders to choose from.
        model->setFileTypeFilter({}, {QStringLiteral("inode/directory")});
        return;
    }
    const FileType current = type();
    if (current.showsEverything()) {
        model->setFileTypeFilter({}, {});
    } else {
        QStringList globs;
        globs.reserve(current.globs.size());
        for (const QString &glob : current.globs) {
            globs.append(simplifyGlob(glob));
        }
        model->setFileTypeFilter(globs, current.mimeTypes);
    }
}

void FileDialog::selectionChanged(const KFileItemList &items)
{
    if (!m_field) {
        updateAcceptButton();
        return;
    }
    QStringList names;
    for (const KFileItem &item : items) {
        // In Open and Save a folder is somewhere to go, not a name to keep.
        if (m_request.mode == Mode::OpenFolder || !item.isDir()) {
            names.append(item.name());
        }
    }
    if (!names.isEmpty()) {
        m_field->setText(fieldFromNames(names));
    } else if (m_request.mode != Mode::SaveFile) {
        m_field->clear();
    }
    updateAcceptButton();
}

void FileDialog::itemActivated(const KFileItem &item)
{
    if (item.isDir()) {
        m_view->setUrl(item.url());
        return;
    }
    if (m_field) {
        m_field->setText(item.name());
    }
    tryAccept();
}

void FileDialog::updateAcceptButton()
{
    if (!m_accept) {
        return;
    }
    switch (m_request.mode) {
    case Mode::OpenFile:
    case Mode::SaveFile:
        m_accept->setEnabled(!m_field->text().trimmed().isEmpty());
        break;
    case Mode::OpenFolder:
    case Mode::SaveFiles:
        m_accept->setEnabled(true);
        break;
    }
}

QUrl FileDialog::localUrl(const QUrl &url) const
{
    if (url.isLocalFile()) {
        return url;
    }
    // Places like desktop:/ are still files of this PC.
    auto *job = KIO::mostLocalUrl(url, KIO::HideProgressInfo);
    KJobWidgets::setWindow(job, const_cast<FileDialog *>(this));
    job->exec();
    return job->mostLocalUrl();
}

void FileDialog::refuse(const QString &message)
{
    m_lastMessage = message;
    if (qEnvironmentVariableIsSet("RAYE_FILE_DIALOG_QUIET")) {
        return;
    }
    CardDialog card(this);
    card.setTitle(windowTitle());
    card.setMessage(message);
    card.setButtonText(CardDialog::Primary, i18nc("@action:button", "OK"));
    card.exec();
}

bool FileDialog::acceptOpen()
{
    const QStringList names = namesFromField(m_field->text());
    if (names.isEmpty()) {
        return false;
    }
    QList<QUrl> urls;
    for (const QString &name : names) {
        const QUrl url = localUrl(resolveName(name, m_view->url()));
        if (!url.isLocalFile()) {
            refuse(i18nc("@info", "Choose a file on this PC."));
            return false;
        }
        const QFileInfo info(url.toLocalFile());
        if (info.isDir() && names.size() == 1) {
            m_view->setUrl(url);
            m_field->clear();
            return false;
        }
        if (!info.exists() || info.isDir()) {
            refuse(i18nc("@info %1 is a file name", "%1\nFile not found. Check the file name and try again.", name));
            return false;
        }
        urls.append(url);
    }
    if (urls.size() > 1 && !m_request.multiple) {
        refuse(i18nc("@info", "Choose one file."));
        return false;
    }
    rememberAndClose(urls);
    return true;
}

bool FileDialog::acceptFolder()
{
    QUrl target = m_view->url();
    const QStringList names = namesFromField(m_field->text());
    if (names.size() > 1) {
        refuse(i18nc("@info", "Choose one folder."));
        return false;
    }
    if (names.size() == 1) {
        target = resolveName(names.first(), m_view->url());
    }
    const QUrl url = localUrl(target);
    if (!url.isLocalFile()) {
        refuse(i18nc("@info", "Choose a folder on this PC."));
        return false;
    }
    if (!QFileInfo(url.toLocalFile()).isDir()) {
        refuse(i18nc("@info %1 is a folder name", "%1\nFolder not found. Check the name and try again.", names.isEmpty() ? url.toLocalFile() : names.first()));
        return false;
    }
    rememberAndClose({url});
    return true;
}

bool FileDialog::acceptSave()
{
    const QStringList names = namesFromField(m_field->text());
    if (names.isEmpty()) {
        return false;
    }
    const QString typed = names.first();
    const QUrl typedUrl = localUrl(resolveName(typed, m_view->url()));
    if (typedUrl.isLocalFile() && QFileInfo(typedUrl.toLocalFile()).isDir()) {
        m_view->setUrl(typedUrl);
        m_field->clear();
        return false;
    }
    const QUrl url = localUrl(resolveName(withTypeExtension(typed, type()), m_view->url()));
    if (!url.isLocalFile()) {
        refuse(i18nc("@info", "Choose a folder on this PC."));
        return false;
    }
    const QFileInfo info(url.toLocalFile());
    if (!QFileInfo(info.path()).isDir()) {
        refuse(i18nc("@info %1 is a folder", "%1\nFolder not found. Check the name and try again.", info.path()));
        return false;
    }
    if (info.exists() && !m_confirm({info.fileName()})) {
        return false;
    }
    rememberAndClose({url});
    return true;
}

bool FileDialog::acceptSaveFiles()
{
    const QUrl folderUrl = localUrl(m_view->url());
    if (!folderUrl.isLocalFile()) {
        refuse(i18nc("@info", "Choose a folder on this PC."));
        return false;
    }
    const QDir dir(folderUrl.toLocalFile());
    QStringList existing;
    QList<QUrl> urls;
    for (const QString &name : std::as_const(m_request.names)) {
        // Only the last part: an app does not get to pick another folder here.
        const QString base = QFileInfo(name).fileName();
        if (dir.exists(base)) {
            existing.append(base);
        }
        urls.append(QUrl::fromLocalFile(dir.filePath(base)));
    }
    if (!existing.isEmpty() && !m_confirm(existing)) {
        return false;
    }
    rememberAndClose(urls);
    return true;
}

bool FileDialog::tryAccept()
{
    switch (m_request.mode) {
    case Mode::OpenFile:
        return acceptOpen();
    case Mode::OpenFolder:
        return acceptFolder();
    case Mode::SaveFile:
        return acceptSave();
    case Mode::SaveFiles:
        return acceptSaveFiles();
    }
    return false;
}

void FileDialog::rememberAndClose(const QList<QUrl> &urls)
{
    m_result = urls;
    KConfigGroup group = KSharedConfig::openStateConfig()->group(QLatin1String(configGroup()));
    if (m_view->url().isLocalFile()) {
        group.writeEntry("LastFolder", m_view->url());
    }
    accept();
}

void FileDialog::createFolder()
{
    CardDialog card(this);
    card.setTitle(i18nc("@title:window", "New folder"));
    auto *field = new QLineEdit(i18nc("@item:intext the name a new folder starts with", "New folder"));
    field->selectAll();
    card.setContentWidget(field);
    card.setButtonText(CardDialog::Primary, i18nc("@action:button", "Create"));
    card.setButtonText(CardDialog::Close, i18nc("@action:button", "Cancel"));
    field->setFocus();
    if (card.exec() != CardDialog::PrimaryChosen || field->text().trimmed().isEmpty()) {
        return;
    }
    const QUrl url = resolveName(field->text().trimmed(), m_view->url());
    auto *job = KIO::mkdir(url);
    KJobWidgets::setWindow(job, this);
    QPointer<FileDialog> guard(this);
    connect(job, &KJob::result, this, [this, guard, url](KJob *done) {
        if (!guard) {
            return;
        }
        if (done->error()) {
            refuse(done->errorString());
            return;
        }
        m_view->setUrl(url);
    });
}

void FileDialog::done(int result)
{
    KConfigGroup group = KSharedConfig::openStateConfig()->group(QLatin1String(configGroup()));
    if (windowHandle()) {
        KWindowConfig::saveWindowSize(windowHandle(), group);
    }
    QDialog::done(result);
}

QUrl pickFile(QWidget *parent, const QString &title, const QUrl &folder)
{
    FileRequest request;
    request.title = title;
    request.folder = folder;
    FileDialog dialog(request, parent);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedUrls().isEmpty()) {
        return {};
    }
    return dialog.selectedUrls().first();
}

}

static QWidget *showDialog(QWidget *parent, const Raye::Portal::FileRequest &request)
{
    auto *host = new QWidget(parent);
    host->setAutoFillBackground(true);
    auto *dialog = new Raye::Portal::FileDialog(request, host);
    QTimer::singleShot(0, dialog, [dialog] {
        dialog->show();
    });
    return host;
}

RAYE_SHOWCASE(openFileDialog, "OpenFileDialog", "The Open dialog of every app")
{
    Raye::Portal::FileRequest request;
    request.types = {{QStringLiteral("All files"), {QStringLiteral("*")}, {}},
                     {QStringLiteral("Pictures"), {QStringLiteral("*.png"), QStringLiteral("*.jpg")}, {}}};
    return showDialog(parent, request);
}

RAYE_SHOWCASE(saveFileDialog, "SaveFileDialog", "The Save dialog of every app")
{
    Raye::Portal::FileRequest request;
    request.mode = Raye::Portal::Mode::SaveFile;
    request.name = QStringLiteral("Budget 2026.ods");
    request.types = {{QStringLiteral("OpenDocument spreadsheet"), {QStringLiteral("*.ods")}, {}},
                     {QStringLiteral("All files"), {QStringLiteral("*")}, {}}};
    request.choices = {{QStringLiteral("encoding"), QStringLiteral("Encoding:"), {{QStringLiteral("utf8"), QStringLiteral("UTF-8")}, {QStringLiteral("latin"), QStringLiteral("Western")}}, QStringLiteral("utf8")},
                       {QStringLiteral("keep"), QStringLiteral("Keep a copy"), {}, QStringLiteral("true")}};
    return showDialog(parent, request);
}

RAYE_SHOWCASE(selectFolderDialog, "SelectFolderDialog", "The folder picker of every app")
{
    Raye::Portal::FileRequest request;
    request.mode = Raye::Portal::Mode::OpenFolder;
    return showDialog(parent, request);
}
