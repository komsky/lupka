#include "platform.h"

#include <QGuiApplication>
#include <QStandardPaths>

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
}

bool hasProgram(const QString &name)
{
    return !QStandardPaths::findExecutable(name).isEmpty();
}

}  // namespace platform
