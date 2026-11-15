/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"
#include "request.h"

#include <QDialog>
#include <QHash>
#include <QUrl>

#include <functional>

class DolphinView;
class KFileItem;
class KFileItemList;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QMenu;

namespace Raye::Address
{
class AddressBar;
}
namespace Raye::NavPane
{
class NavigationPane;
}
namespace Raye::Ui
{
class CommandButton;
}

namespace Raye::Portal
{

// The Open and Save dialog (explorer spec 3.16): the Files window with what
// an app needs from it. Address row, New folder and View, the navigation pane
// and the list, then the file name, the type and the app's own choices,
// Open or Save and Cancel. Every app gets this one through the portal.
class DOLPHIN_EXPORT FileDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FileDialog(const FileRequest &request, QWidget *parent = nullptr);
    ~FileDialog() override;

    // After accept(): what the app gets, all local files.
    QList<QUrl> selectedUrls() const
    {
        return m_result;
    }
    int currentType() const;
    QList<FileType> types() const
    {
        return m_request.types;
    }
    // Choice id to value, as the app reads them back.
    QHash<QString, QString> choiceValues() const;

    QUrl folder() const;
    void setFolder(const QUrl &url);
    QString fieldText() const;
    void setFieldText(const QString &text);

    // Open or Save: reads the field and the selection and closes the dialog
    // when it can. False leaves it open (a folder entered, a question asked).
    bool tryAccept();

    // Asks before writing over files. Replaced in tests; true goes ahead.
    using Confirm = std::function<bool(const QStringList &existing)>;
    void setConfirmOverwrite(const Confirm &confirm)
    {
        m_confirm = confirm;
    }

    // For tests: what the dialog last refused and said to the person, the
    // names the list shows, and the type list and buttons.
    QString lastMessage() const
    {
        return m_lastMessage;
    }
    QStringList shownNames() const;
    void setCurrentType(int index);
    Raye::Ui::CommandButton *acceptButton() const
    {
        return m_accept;
    }
    Raye::Ui::CommandButton *cancelButton() const
    {
        return m_cancel;
    }

    // Every way out comes through here: it keeps the size for next time.
    void done(int result) override;

private:
    void buildTop();
    void buildBottom();
    void selectionChanged(const KFileItemList &items);
    void itemActivated(const KFileItem &item);
    void applyType();
    void updateAcceptButton();
    void createFolder();
    void refuse(const QString &message);
    bool acceptOpen();
    bool acceptFolder();
    bool acceptSave();
    bool acceptSaveFiles();
    void rememberAndClose(const QList<QUrl> &urls);
    QUrl localUrl(const QUrl &url) const;
    FileType type() const;

    FileRequest m_request;
    Raye::Address::AddressBar *m_address = nullptr;
    Raye::NavPane::NavigationPane *m_pane = nullptr;
    DolphinView *m_view = nullptr;
    QLineEdit *m_field = nullptr;
    QComboBox *m_types = nullptr;
    Raye::Ui::CommandButton *m_accept = nullptr;
    Raye::Ui::CommandButton *m_cancel = nullptr;
    QHash<QString, QComboBox *> m_choiceLists;
    QHash<QString, QCheckBox *> m_choiceChecks;
    QList<QUrl> m_result;
    Confirm m_confirm;
    QString m_lastMessage;
    bool m_settingField = false;
};

// An Open dialog in this process, with no portal in between: what "Look for an
// app" uses. Empty when cancelled.
DOLPHIN_EXPORT QUrl pickFile(QWidget *parent, const QString &title, const QUrl &folder);

}
