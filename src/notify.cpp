// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "notify.h"

#include "config.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QUrl>

namespace notify {

void show(const QString &summary, const QString &body, const QString &imagePath)
{
    QVariantMap hints{{QStringLiteral("desktop-entry"), QStringLiteral(APP_ID)}};
    if (!imagePath.isEmpty())
        hints.insert(QStringLiteral("image-path"), QUrl::fromLocalFile(imagePath).toString());

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    message << QStringLiteral(APP_NAME) << uint(0) << QStringLiteral(APP_ID) << summary << body << QStringList()
            << hints << int(4000);
    QDBusConnection::sessionBus().asyncCall(message);
}

}  // namespace notify
