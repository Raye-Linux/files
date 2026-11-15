/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include "locations/locationnames.h"

#include <QPointer>
#include <QUrl>
#include <QWidget>

#include <optional>

class KLineEdit;
class QDropEvent;
class QMenu;
class QVariantAnimation;

namespace Raye::Address
{

class CrumbButton;
class ChevronButton;
class SubfolderFlyout;

// The address box of the address row (explorer spec 3.3): 32 tall,
// radius 4, the control fill and a 1 px stroke. It shows the location's
// icon and its crumbs ("This PC › Documents › Work"); a chevron after a
// crumb drops down the folders in it; files dropped on a crumb go there.
// A click on its empty part, Tab, or editLocation() turns it into a text
// field with the real path selected, with completion; Enter goes, Esc
// gives the crumbs back. Long paths fold their first crumbs into "«".
class DOLPHIN_EXPORT AddressBox : public QWidget
{
    Q_OBJECT
public:
    // Room around the box for the focus glow.
    static constexpr int glowMargin = 3;
    static constexpr int boxHeight = 32;

    explicit AddressBox(QWidget *parent = nullptr);
    ~AddressBox() override;

    void setLocationNames(const Locations::LocationNames &names);
    const Locations::LocationNames &locationNames() const;

    QUrl url() const
    {
        return m_url;
    }
    void setUrl(const QUrl &url);

    // Edit mode.
    void editLocation();
    void cancelEditing();
    bool isEditing() const
    {
        return m_editing;
    }
    KLineEdit *lineEdit() const
    {
        return m_edit;
    }

    // The loading line under the crumbs: -1 runs back and forth.
    void setLoading(bool loading);
    bool isLoading() const
    {
        return m_loading;
    }
    void setLoadingProgress(int percent);
    int loadingProgress() const
    {
        return m_progress;
    }

    // Drops down the folders of a crumb, as its chevron does.
    void showSubfolders(int index);

    // For tests and the address row.
    QStringList crumbTexts() const;
    QString locationIconName() const
    {
        return m_iconName;
    }
    QWidget *crumbWidget(int index) const;
    QWidget *overflowButton() const;
    int hiddenCrumbCount() const
    {
        return m_hidden;
    }
    // The crumbs folded into "«", deepest first; the caller owns it.
    QMenu *overflowMenu();
    QRect boxRect() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void navigateRequested(const QUrl &url);
    void urlsDropped(const QUrl &destination, QDropEvent *event);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void changeEvent(QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuild();
    void relayout();
    void finishEditing();
    int contentLeft() const;

    std::optional<Locations::LocationNames> m_names;
    QUrl m_url;
    QList<Locations::Crumb> m_crumbs;
    QString m_iconName;
    QList<CrumbButton *> m_crumbButtons;
    QList<ChevronButton *> m_chevrons;
    CrumbButton *m_overflow = nullptr;
    KLineEdit *m_edit = nullptr;
    QPointer<SubfolderFlyout> m_flyout;
    int m_hidden = 0;
    bool m_editing = false;
    bool m_hovered = false;
    bool m_loading = false;
    int m_progress = -1;
    qreal m_phase = 0;
    QPointer<QVariantAnimation> m_loadingAnimation;
};

}
