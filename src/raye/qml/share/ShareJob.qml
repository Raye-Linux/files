/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import org.kde.purpose as Purpose

// One share target at work, with the questions it asks first (which
// device, which account). Loaded by ShareSheet only when needed, so the
// sheet itself opens even when Purpose's QML is missing.
Item {
    id: job

    required property QtObject model
    required property int row

    signal finished(bool ok, string message)

    implicitHeight: Math.max(view.implicitHeight, 64)

    Purpose.JobView {
        id: view
        anchors.fill: parent
        model: job.model
        index: job.row
        onStateChanged: {
            if (state === Purpose.PurposeJobController.Finished) {
                job.finished(true, "")
            } else if (state === Purpose.PurposeJobController.Error) {
                job.finished(false, view.job ? view.job.errorString : "")
            } else if (state === Purpose.PurposeJobController.Cancelled) {
                job.finished(false, "")
            }
        }
        Component.onCompleted: start()
    }
}
