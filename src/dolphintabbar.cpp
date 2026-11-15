/*
 * SPDX-FileCopyrightText: 2014 Emmanuel Pescosta <emmanuelpescosta099@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphintabbar.h"

#include <QDragEnterEvent>
#include <QMimeData>
#include <QTimer>

class PreventFocusWhileHidden : public QObject
{
public:
    PreventFocusWhileHidden(QObject *parent)
        : QObject(parent){};

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        switch (ev->type()) {
        case QEvent::Hide:
            static_cast<QWidget *>(obj)->setFocusPolicy(Qt::NoFocus);
            return false;
        case QEvent::Show:
            static_cast<QWidget *>(obj)->setFocusPolicy(Qt::TabFocus);
            return false;
        default:
            return false;
        }
    };
};

DolphinTabBar::DolphinTabBar(QWidget *parent)
    : Raye::Title::TabBar(parent)
    , m_autoActivationIndex(-1)
{
    setAcceptDrops(true);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    // Raye: movable tabs and close buttons come from Raye::Title::TabBar.
    // Duplicate tab opens the same folder in a new tab; Move tab to new
    // window is Dolphin's detach.
    connect(this, &Raye::Title::TabBar::duplicateTabRequested, this, &DolphinTabBar::openNewActivatedTab);
    connect(this, &Raye::Title::TabBar::moveTabToNewWindowRequested, this, &DolphinTabBar::tabDetachRequested);

    setFocusPolicy(Qt::NoFocus);
    installEventFilter(new PreventFocusWhileHidden(this));

    m_autoActivationTimer = new QTimer(this);
    m_autoActivationTimer->setSingleShot(true);
    m_autoActivationTimer->setInterval(800);
    connect(m_autoActivationTimer, &QTimer::timeout, this, &DolphinTabBar::slotAutoActivationTimeout);
}

void DolphinTabBar::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    const int index = tabAt(event->position().toPoint());

    if (mimeData->hasUrls()) {
        event->acceptProposedAction();
        updateAutoActivationTimer(index);
        setDropTargetIndex(index); // Raye: light the tab under the drop
    }

    QTabBar::dragEnterEvent(event);
}

void DolphinTabBar::dragLeaveEvent(QDragLeaveEvent *event)
{
    updateAutoActivationTimer(-1);
    setDropTargetIndex(-1); // Raye

    QTabBar::dragLeaveEvent(event);
}

void DolphinTabBar::dragMoveEvent(QDragMoveEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    const int index = tabAt(event->position().toPoint());

    if (mimeData->hasUrls()) {
        Q_EMIT tabDragMoveEvent(index, event);
        updateAutoActivationTimer(index);
        setDropTargetIndex(index); // Raye
    }

    QTabBar::dragMoveEvent(event);
}

void DolphinTabBar::dropEvent(QDropEvent *event)
{
    // Disable the auto activation timer
    updateAutoActivationTimer(-1);
    setDropTargetIndex(-1); // Raye

    const QMimeData *mimeData = event->mimeData();
    const int index = tabAt(event->position().toPoint());

    if (mimeData->hasUrls()) {
        Q_EMIT tabDropEvent(index, event);
    }

    QTabBar::dropEvent(event);
}

void DolphinTabBar::slotAutoActivationTimeout()
{
    if (m_autoActivationIndex >= 0) {
        setCurrentIndex(m_autoActivationIndex);
        updateAutoActivationTimer(-1);
    }
}

void DolphinTabBar::updateAutoActivationTimer(const int index)
{
    if (m_autoActivationIndex != index) {
        m_autoActivationIndex = index;

        if (m_autoActivationIndex < 0) {
            m_autoActivationTimer->stop();
        } else {
            m_autoActivationTimer->start();
        }
    }
}

#include "moc_dolphintabbar.cpp"
