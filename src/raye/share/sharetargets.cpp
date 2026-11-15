/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharetargets.h"

#include <KLocalizedString>

namespace Raye::Share
{

Target describe(const QString &pluginId, const QString &purposeName, const QString &purposeIcon)
{
    const QString id = pluginId.endsWith(QLatin1String("plugin")) ? pluginId.chopped(6) : pluginId;
    if (pluginId == QLatin1String(copyTarget)) {
        return {i18nc("@action:button share target: copy the files", "Copy"), QStringLiteral("edit-copy"), Place::Row, 90, true};
    }
    if (pluginId == QLatin1String(copyLinkTarget)) {
        return {i18nc("@action:button share target: copy the address of network files", "Copy link"), QStringLiteral("edit-link"), Place::Row, 95, true};
    }
    if (id == QLatin1String("kdeconnect")) {
        return {i18nc("@action:button share target: a phone or computer paired with this one", "Nearby devices"), QStringLiteral("smartphone"), Place::Row, 10};
    }
    if (id == QLatin1String("bluetooth")) {
        return {i18nc("@action:button share target", "Bluetooth"), QStringLiteral("preferences-system-bluetooth"), Place::Row, 20};
    }
    if (id == QLatin1String("email")) {
        return {i18nc("@action:button share target: the mail app", "Email"), QStringLiteral("mail-message-new"), Place::Row, 30, true};
    }
    // Files' own Copy does what these do, the Raye way.
    if (id == QLatin1String("clipboard") || id == QLatin1String("kdeconnectsms")) {
        return {purposeName, purposeIcon, Place::Hidden, 100};
    }
    if (id == QLatin1String("saveas")) {
        return {i18nc("@action:button share target: save a copy somewhere else", "Save a copy"), QStringLiteral("document-save-as"), Place::More, 100};
    }
    if (id == QLatin1String("barcode")) {
        return {i18nc("@action:button share target", "QR code"), QStringLiteral("view-barcode-qr"), Place::More, 100};
    }
    return {purposeName.isEmpty() ? pluginId : purposeName, purposeIcon.isEmpty() ? QStringLiteral("document-share") : purposeIcon, Place::More, 100};
}

}
