// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

// GIO headers use "signals" as an identifier, which Qt defines as a macro.
#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")

#include "config.h"
#include "gsettingsutil.h"
#include "hotkeys.h"
#include "keynames.h"

namespace {

constexpr const char *kMediaKeysSchema = "org.gnome.settings-daemon.plugins.media-keys";
constexpr const char *kCustomSchema = "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding";
constexpr const char *kListKey = "custom-keybindings";
const QString kBasePath = QStringLiteral("/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/");

QString ourPrefix()
{
    return kBasePath + QStringLiteral(APP_BIN) + QLatin1Char('-');
}

// GNOME custom shortcuts (Settings > Keyboard > Custom Shortcuts). They work on
// X11 and Wayland, survive restarts, and the user can see and edit them in
// GNOME Settings. Pressing one runs "<app> <action>".
class GnomeHotkeys : public HotkeyBackend
{
public:
    using HotkeyBackend::HotkeyBackend;

    QString name() const override { return QStringLiteral("GNOME custom shortcuts"); }

    void apply(const Bindings &bindings) override
    {
        setError({});
        if (!gsettingsutil::hasSchema(kMediaKeysSchema) || !gsettingsutil::hasSchema(kCustomSchema)) {
            setError(QStringLiteral("GNOME settings schemas for custom shortcuts are not installed."));
            return;
        }

        GSettings *mediaKeys = g_settings_new(kMediaKeysSchema);
        const QStringList current = gsettingsutil::getStrv(mediaKeys, kListKey);

        QStringList ours;
        QStringList skipped;
        for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
            const ActionInfo &info = actionInfo(it.key());
            int index = 0;
            for (const QKeySequence &sequence : it.value()) {
                const QString accel = keynames::toGtkAccelerator(sequence);
                if (accel.isEmpty()) {
                    skipped << sequence.toString(QKeySequence::NativeText);
                    continue;
                }
                const QString path = ourPrefix() + info.id + (index ? QStringLiteral("-%1").arg(index) : QString())
                                     + QLatin1Char('/');
                ++index;
                GSettings *item = g_settings_new_with_path(kCustomSchema, path.toUtf8().constData());
                gsettingsutil::setString(item, "name", QStringLiteral(APP_NAME ": ") + info.label);
                gsettingsutil::setString(item, "command", commandForAction(it.key()));
                gsettingsutil::setString(item, "binding", accel);
                g_object_unref(item);
                ours << path;
            }
        }

        QStringList next;
        for (const QString &path : current) {
            if (!path.startsWith(ourPrefix()))
                next << path;
            else if (!ours.contains(path))
                resetItem(path);
        }
        next << ours;
        if (next != current)
            gsettingsutil::setStrv(mediaKeys, kListKey, next);
        g_object_unref(mediaKeys);
        g_settings_sync();

        if (!skipped.isEmpty())
            setError(QStringLiteral("GNOME cannot bind: %1").arg(skipped.join(QStringLiteral(", "))));
    }

    void unregisterAll() override
    {
        if (!gsettingsutil::hasSchema(kMediaKeysSchema))
            return;
        GSettings *mediaKeys = g_settings_new(kMediaKeysSchema);
        const QStringList current = gsettingsutil::getStrv(mediaKeys, kListKey);
        QStringList next;
        for (const QString &path : current) {
            if (path.startsWith(ourPrefix()))
                resetItem(path);
            else
                next << path;
        }
        if (next != current)
            gsettingsutil::setStrv(mediaKeys, kListKey, next);
        g_object_unref(mediaKeys);
        g_settings_sync();
    }

private:
    static void resetItem(const QString &path)
    {
        GSettings *item = g_settings_new_with_path(kCustomSchema, path.toUtf8().constData());
        g_settings_reset(item, "name");
        g_settings_reset(item, "command");
        g_settings_reset(item, "binding");
        g_object_unref(item);
    }
};

}  // namespace

HotkeyBackend *createGnomeHotkeys(QObject *parent)
{
    return new GnomeHotkeys(parent);
}
