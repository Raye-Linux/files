/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "preview/detailsmodel.h"

#include <KFileItem>

#include <QPointer>
#include <QWidget>

class QLabel;
class QVBoxLayout;

namespace KIO
{
class PreviewJob;
}

namespace Raye::Preview
{

// The Details pane (explorer spec 3.8, Alt+Shift+P): a large thumbnail
// with round corners, the name, Share and Open, then "Properties" as
// small grey labels over their values.
class DOLPHIN_EXPORT DetailsPane : public QWidget
{
    Q_OBJECT

public:
    explicit DetailsPane(QWidget *parent = nullptr);
    ~DetailsPane() override;

    // The selection; folder is what the view shows, described when nothing
    // is selected.
    void showItems(const KFileItemList &selection, const KFileItem &folder);

    const Details &details() const;

Q_SIGNALS:
    void urlActivated(const QUrl &url);
    void shareRequested(const KFileItemList &items);
    void openRequested(const KFileItemList &items);

private:
    void rebuildRows();
    void startThumbnail(const KFileItem &item);
    void setIcon(const QString &iconName);

    QLabel *m_thumbnail = nullptr;
    QLabel *m_title = nullptr;
    QWidget *m_buttons = nullptr;
    QLabel *m_propertiesTitle = nullptr;
    QWidget *m_rows = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    QPointer<KIO::PreviewJob> m_previewJob;
    KFileItemList m_selection;
    Details m_details;
};

}
