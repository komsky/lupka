// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hotkeys.h"

#include "config.h"
#include "platform.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcHotkeys, "app.hotkeys")

namespace {

QString shellQuote(const QString &arg)
{
    QString quoted = arg;
    quoted.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

// Used when nothing else can grab keys (e.g. sway without the portal): the
// user binds "<app> zoom" etc. in their compositor config.
class ManualHotkeys : public HotkeyBackend
{
public:
    using HotkeyBackend::HotkeyBackend;
    QString name() const override { return QStringLiteral("manual"); }
    void apply(const Bindings &) override
    {
        setError(QStringLiteral("This desktop offers no global shortcut service. Bind keys to commands such as "
                                "\"%1 zoom\" in your compositor settings.")
                     .arg(QStringLiteral(APP_BIN)));
    }
    void unregisterAll() override {}
};

}  // namespace

QString commandForAction(Action action)
{
    return shellQuote(QCoreApplication::applicationFilePath()) + QLatin1Char(' ') + actionInfo(action).id;
}

Hotkeys::Hotkeys(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    const QString forced = qEnvironmentVariable("APP_HOTKEY_BACKEND");
    auto pick = [&](const QString &name) { return forced.isEmpty() || forced == name; };

    if (pick(QStringLiteral("gnome")) && platform::desktop() == platform::Desktop::Gnome)
        m_backend = createGnomeHotkeys(this);
    if (!m_backend && pick(QStringLiteral("kde")) && platform::desktop() == platform::Desktop::Kde
        && QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.kglobalaccel")))
        m_backend = createKdeHotkeys(this);
    if (!m_backend && pick(QStringLiteral("x11")) && platform::isX11())
        m_backend = createX11Hotkeys(this);
    if (!m_backend && pick(QStringLiteral("portal")) && platform::isWayland())
        m_backend = createPortalHotkeys(this);
    if (!m_backend)
        m_backend = new ManualHotkeys(this);

    qCInfo(lcHotkeys) << "hotkey backend:" << m_backend->name();
    connect(m_backend, &HotkeyBackend::activated, this, &Hotkeys::activated);
}

void Hotkeys::apply()
{
    Bindings bindings;
    for (Action action : configurableActions())
        bindings.insert(action, m_settings->shortcuts(action));
    for (auto it = m_temporary.constBegin(); it != m_temporary.constEnd(); ++it)
        bindings.insert(it.key(), it.value());
    m_backend->apply(bindings);
    if (!m_backend->lastError().isEmpty())
        qCWarning(lcHotkeys) << m_backend->lastError();
}

void Hotkeys::setTemporary(const Bindings &bindings)
{
    if (bindings == m_temporary)
        return;
    m_temporary = bindings;
    apply();
}

void Hotkeys::unregisterAll()
{
    m_backend->unregisterAll();
}

QString Hotkeys::backendName() const
{
    return m_backend->name();
}

QString Hotkeys::status() const
{
    return m_backend->lastError();
}
