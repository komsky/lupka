#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")

#include "livezoom.h"

#include "hotkeys/gsettingsutil.h"
#include "overlay/zoommath.h"
#include "platform.h"
#include "settings.h"

#include <QDBusConnection>
#include <QDBusMessage>

#include <cmath>

namespace {

constexpr const char *kAppsSchema = "org.gnome.desktop.a11y.applications";
constexpr const char *kMagSchema = "org.gnome.desktop.a11y.magnifier";
// Zoom factor of one KWin "view_zoom_in" step (KWin's default).
constexpr double kKwinStep = 1.2;

bool gnomeAvailable()
{
    return platform::desktop() == platform::Desktop::Gnome && gsettingsutil::hasSchema(kAppsSchema)
           && gsettingsutil::hasKey(kMagSchema, "mag-factor");
}

bool kdeAvailable()
{
    return platform::desktop() == platform::Desktop::Kde;
}

void invokeKwin(const QString &shortcut)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.kglobalaccel"), QStringLiteral("/component/kwin"),
        QStringLiteral("org.kde.kglobalaccel.Component"), QStringLiteral("invokeShortcut"));
    message << shortcut;
    QDBusConnection::sessionBus().call(message, QDBus::Block, 1000);
}

}  // namespace

LiveZoom::LiveZoom(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

LiveZoom::~LiveZoom()
{
    if (m_active)
        setActive(false);
}

bool LiveZoom::isSupported() const
{
    return gnomeAvailable() || kdeAvailable();
}

void LiveZoom::toggle()
{
    setActive(!m_active);
}

void LiveZoom::zoomIn()
{
    if (!m_active)
        return;
    m_factor = qMin(zoommath::zoomIn(m_factor), 32.0);
    applyFactor();
}

void LiveZoom::zoomOut()
{
    if (!m_active)
        return;
    m_factor = zoommath::zoomOut(m_factor);
    // Zooming all the way out ends live zoom, as in ZoomIt.
    if (m_factor <= 1.001)
        setActive(false);
    else
        applyFactor();
}

void LiveZoom::setActive(bool active)
{
    if (active == m_active || !isSupported())
        return;
    m_active = active;

    if (gnomeAvailable()) {
        GSettings *apps = g_settings_new(kAppsSchema);
        GSettings *mag = g_settings_new(kMagSchema);
        if (active) {
            m_savedFactor = g_settings_get_double(mag, "mag-factor");
            m_savedTracking = gsettingsutil::getString(mag, "mouse-tracking");
            m_factor = m_settings->liveZoomFactor();
            g_settings_set_string(mag, "mouse-tracking", "proportional");
            g_settings_set_double(mag, "mag-factor", m_factor);
            g_settings_set_boolean(apps, "screen-magnifier-enabled", TRUE);
        } else {
            g_settings_set_boolean(apps, "screen-magnifier-enabled", FALSE);
            if (m_savedFactor > 0)
                g_settings_set_double(mag, "mag-factor", m_savedFactor);
            if (!m_savedTracking.isEmpty())
                g_settings_set_string(mag, "mouse-tracking", m_savedTracking.toUtf8().constData());
        }
        g_settings_sync();
        g_object_unref(mag);
        g_object_unref(apps);
    } else if (kdeAvailable()) {
        if (active) {
            m_factor = m_settings->liveZoomFactor();
            m_kwinSteps = 0;
            applyFactor();
        } else {
            invokeKwin(QStringLiteral("view_actual_size"));
            m_kwinSteps = 0;
        }
    }
    Q_EMIT activeChanged(m_active);
}

void LiveZoom::applyFactor()
{
    if (gnomeAvailable()) {
        GSettings *mag = g_settings_new(kMagSchema);
        g_settings_set_double(mag, "mag-factor", m_factor);
        g_settings_sync();
        g_object_unref(mag);
    } else if (kdeAvailable()) {
        // KWin only zooms in fixed steps; walk to the nearest one.
        const int wanted = qMax(0, int(std::lround(std::log(m_factor) / std::log(kKwinStep))));
        for (; m_kwinSteps < wanted; ++m_kwinSteps)
            invokeKwin(QStringLiteral("view_zoom_in"));
        for (; m_kwinSteps > wanted; --m_kwinSteps)
            invokeKwin(QStringLiteral("view_zoom_out"));
    }
}
