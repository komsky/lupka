// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "backends.h"

#include <QPixmap>

void X11Capture::capture(const QList<QScreen *> &screens)
{
    ScreenImages images;
    for (QScreen *screen : screens) {
        const QPixmap pixmap = screen->grabWindow(0);
        if (pixmap.isNull()) {
            QMetaObject::invokeMethod(this, [this] { Q_EMIT failed(QStringLiteral("could not read the X11 root window")); },
                                      Qt::QueuedConnection);
            return;
        }
        QImage image = pixmap.toImage();
        image.setDevicePixelRatio(pixmap.devicePixelRatio());
        images.append({screen, image});
    }
    // Keep the asynchronous contract: callers connect after calling capture().
    QMetaObject::invokeMethod(this, [this, images] { Q_EMIT finished(images); }, Qt::QueuedConnection);
}
