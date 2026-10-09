// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "platform.h"

#include <QGuiApplication>
#include <QStandardPaths>
#include <QVersionNumber>

namespace platform {

Desktop desktop()
{
    const QStringList parts = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part.compare(QLatin1String("KDE"), Qt::CaseInsensitive) == 0)
            return Desktop::Kde;
        if (part.compare(QLatin1String("GNOME"), Qt::CaseInsensitive) == 0
            || part.compare(QLatin1String("GNOME-Classic"), Qt::CaseInsensitive) == 0
            || part.compare(QLatin1String("GNOME-Flashback"), Qt::CaseInsensitive) == 0)
            return Desktop::Gnome;
    }
    return Desktop::Other;
}

QString desktopName()
{
    switch (desktop()) {
    case Desktop::Gnome:
        return QStringLiteral("GNOME");
    case Desktop::Kde:
        return QStringLiteral("KDE Plasma");
    case Desktop::Other:
        break;
    }
    const QString raw = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    return raw.isEmpty() ? QStringLiteral("unknown desktop") : raw;
}

bool isWayland()
{
    return QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
}

bool isX11()
{
    return QGuiApplication::platformName() == QLatin1String("xcb");
}

void prepareEnvironment()
{
    // Under Wayland we must be a native Wayland client: through XWayland we can
    // neither capture the screen nor cover other windows. Fall back to xcb only
    // if the Wayland plugin is missing.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("QT_QPA_PLATFORM", "wayland;xcb");

    // Older Qt keeps stale XInput2 devices around after a mouse reconnects (a
    // wireless receiver waking up, resume, a KVM switch) and from then on drops
    // its wheel events, so zoom, pen width and text size stop answering. The
    // daemon runs for days, so take the mouse from the core protocol instead:
    // the wheel arrives as buttons 4 and 5 whatever was plugged in meanwhile.
    // This costs smooth touchpad scrolling in the settings window; we use no
    // tablet or touch events. Drop it once the minimum Qt is 6.5.1 (QTBUG-99331).
    if (qtDropsWheelAfterHotplug(QVersionNumber::fromString(QLatin1String(qVersion())))
        && !qEnvironmentVariableIsSet("QT_XCB_NO_XI2"))
        qputenv("QT_XCB_NO_XI2", "1");
}

bool qtDropsWheelAfterHotplug(const QVersionNumber &qtVersion)
{
    return qtVersion < QVersionNumber(6, 5, 1);
}

bool hasProgram(const QString &name)
{
    return !QStandardPaths::findExecutable(name).isEmpty();
}

}  // namespace platform
