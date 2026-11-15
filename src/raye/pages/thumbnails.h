/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "dolphin_export.h"

#include <QQuickAsyncImageProvider>

namespace Raye::Pages
{
// "image://rayethumb/<file url>": the thumbnail KIO makes for a file (a
// video's frame, a PDF's page), at the size the Image asks for. Every QML
// page of Files has it.
class DOLPHIN_EXPORT ThumbnailProvider : public QQuickAsyncImageProvider
{
public:
    static constexpr char16_t id[] = u"rayethumb";
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};
}
