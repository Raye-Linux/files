/*
 * SPDX-FileCopyrightText: 2012 Peter Penz <peter.penz19@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KSTANDARDITEMLISTVIEW_H
#define KSTANDARDITEMLISTVIEW_H

#include "dolphin_export.h"
#include "kitemviews/kitemlistview.h"

/**
 * @brief Provides layouts for icons-, compact- and details-view.
 *
 * Together with the KStandardItemModel lists for standard usecases
 * can be created in a straight forward way.
 *
 * Example code:
 * <code>
 * KStandardItemListView* view = new KStandardItemListView();
 * KStandardItemModel* model = new KStandardItemModel();
 * model->appendItem(new KStandardItem("Item 1"));
 * model->appendItem(new KStandardItem("Item 2"));
 * KItemListController* controller = new KItemListController(model, view);
 * KItemListContainer* container = new KItemListContainer(controller, parentWidget);
 * </code>
 */
class DOLPHIN_EXPORT KStandardItemListView : public KItemListView
{
    Q_OBJECT

public:
    enum ItemLayout { IconsLayout, CompactLayout, DetailsLayout };

    explicit KStandardItemListView(QGraphicsWidget *parent = nullptr);
    ~KStandardItemListView() override;

    void setItemLayout(ItemLayout layout);
    ItemLayout itemLayout() const;

    // Raye: Windows' sizes (explorer spec 3.6). rowHeight is the least height
    // of a row in the details and compact layouts (0: as the text needs);
    // cellWidth fixes the width of an item outside the details layout (0:
    // as the layout decides); flowRows lays the compact layout out in rows
    // instead of columns (Small icons rather than List).
    void setRayeMetrics(int rowHeight, int cellWidth, bool flowRows);
    int rayeRowHeight() const;
    int rayeCellWidth() const;
    bool rayeFlowRows() const;

protected:
    KItemListWidgetCreatorBase *defaultWidgetCreator() const override;
    KItemListGroupHeaderCreatorBase *defaultGroupHeaderCreator() const override;
    void initializeItemListWidget(KItemListWidget *item) override;
    bool itemSizeHintUpdateRequired(const QSet<QByteArray> &changedRoles) const override;
    virtual bool itemLayoutSupportsItemExpanding(ItemLayout layout) const;
    /** To be overridden by sub-classes to specify when full row highlighting should be enabled. */
    virtual bool itemLayoutHighlightEntireRow(ItemLayout layout) const;
    virtual void onItemLayoutChanged(ItemLayout current, ItemLayout previous);
    virtual qreal scrollSingleStep() const override;
    void onScrollOrientationChanged(Qt::Orientation current, Qt::Orientation previous) override;
    void onSupportsItemExpandingChanged(bool supportsExpanding) override;
    void polishEvent() override;
    QSizeF adjustedItemSize(const QSizeF &requested) const override;

private:
    void applyDefaultStyleOption(int iconSize, int padding, int horizontalMargin, int verticalMargin);
    void updateLayoutOfVisibleItems();

private:
    ItemLayout m_itemLayout;
    int m_rayeRowHeight = 0;
    int m_rayeCellWidth = 0;
    bool m_rayeFlowRows = false;
};

#endif
