#include "config.h"
#include "hotkeys.h"
#include "keynames.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QLoggingCategory>
#include <QRandomGenerator>

Q_DECLARE_LOGGING_CATEGORY(lcHotkeys)

namespace {

struct PortalShortcut {
    QString id;
    QVariantMap properties;
};

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcut &shortcut)
{
    argument.beginStructure();
    argument << shortcut.id << shortcut.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcut &shortcut)
{
    argument.beginStructure();
    argument >> shortcut.id >> shortcut.properties;
    argument.endStructure();
    return argument;
}

}  // namespace

Q_DECLARE_METATYPE(PortalShortcut)

namespace {

const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kInterface = QStringLiteral("org.freedesktop.portal.GlobalShortcuts");

// Trigger syntax from the XDG shortcuts spec: "CTRL+SHIFT+s", "LOGO+Print".
QString toPortalTrigger(QKeyCombination combo)
{
    const QString key = keynames::keysymName(combo.key());
    if (key.isEmpty())
        return {};
    QStringList parts;
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    if (mods & Qt::ControlModifier)
        parts << QStringLiteral("CTRL");
    if (mods & Qt::AltModifier)
        parts << QStringLiteral("ALT");
    if (mods & Qt::ShiftModifier)
        parts << QStringLiteral("SHIFT");
    if (mods & Qt::MetaModifier)
        parts << QStringLiteral("LOGO");
    parts << key;
    return parts.join(QLatin1Char('+'));
}

QString requestPath(const QString &token)
{
    QString sender = QDBusConnection::sessionBus().baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    return kPath + QStringLiteral("/request/") + sender + QLatin1Char('/') + token;
}

QString newToken()
{
    return QStringLiteral(APP_BIN "%1").arg(QRandomGenerator::global()->generate());
}

// org.freedesktop.portal.GlobalShortcuts: KDE Plasma 5.27+, GNOME 48+,
// Hyprland. The desktop may ask the user to confirm the shortcuts.
class PortalHotkeys : public HotkeyBackend
{
    Q_OBJECT
public:
    explicit PortalHotkeys(QObject *parent)
        : HotkeyBackend(parent)
    {
        qDBusRegisterMetaType<PortalShortcut>();
        qDBusRegisterMetaType<QList<PortalShortcut>>();
        QDBusConnection::sessionBus().connect(kService, kPath, kInterface, QStringLiteral("Activated"), this,
                                              SLOT(onActivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
    }

    QString name() const override { return QStringLiteral("GlobalShortcuts portal"); }

    void apply(const Bindings &bindings) override
    {
        setError({});
        m_pending = bindings;
        // Shortcuts are bound once per session, so rebinding means a new session.
        closeSession();
        createSession();
    }

    void unregisterAll() override
    {
        m_pending.clear();
        closeSession();
    }

private Q_SLOTS:
    void onActivated(const QDBusObjectPath &session, const QString &id, qulonglong, const QVariantMap &)
    {
        if (session.path() != m_session)
            return;
        if (const auto action = actionFromId(id))
            Q_EMIT activated(*action);
    }

    void onCreateSessionResponse(uint response, const QVariantMap &results)
    {
        stopWatching();
        if (response != 0) {
            setError(QStringLiteral("The desktop refused to create a shortcut session."));
            return;
        }
        m_session = results.value(QStringLiteral("session_handle")).toString();
        bindShortcuts();
    }

    void onBindResponse(uint response, const QVariantMap &)
    {
        stopWatching();
        if (response != 0)
            setError(QStringLiteral("The shortcuts were not confirmed."));
    }

private:
    void watch(const QString &path, const char *slot)
    {
        stopWatching();
        m_watched = path;
        m_watchedSlot = slot;
        QDBusConnection::sessionBus().connect(kService, path, QStringLiteral("org.freedesktop.portal.Request"),
                                              QStringLiteral("Response"), this, slot);
    }

    void stopWatching()
    {
        if (m_watched.isEmpty())
            return;
        QDBusConnection::sessionBus().disconnect(kService, m_watched, QStringLiteral("org.freedesktop.portal.Request"),
                                                 QStringLiteral("Response"), this, m_watchedSlot);
        m_watched.clear();
    }

    void createSession()
    {
        const QString token = newToken();
        watch(requestPath(token), SLOT(onCreateSessionResponse(uint, QVariantMap)));
        QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("CreateSession"));
        message << QVariantMap{{QStringLiteral("handle_token"), token},
                               {QStringLiteral("session_handle_token"), newToken()}};
        checkCall(message);
    }

    void bindShortcuts()
    {
        QList<PortalShortcut> shortcuts;
        for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
            const ActionInfo &info = actionInfo(it.key());
            QVariantMap properties{{QStringLiteral("description"), info.label}};
            if (!it.value().isEmpty() && !it.value().constFirst().isEmpty()) {
                const QString trigger = toPortalTrigger(it.value().constFirst()[0]);
                if (!trigger.isEmpty())
                    properties.insert(QStringLiteral("preferred_trigger"), trigger);
            }
            shortcuts << PortalShortcut{info.id, properties};
        }
        const QString token = newToken();
        watch(requestPath(token), SLOT(onBindResponse(uint, QVariantMap)));
        QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("BindShortcuts"));
        message << QVariant::fromValue(QDBusObjectPath(m_session)) << QVariant::fromValue(shortcuts) << QString()
                << QVariantMap{{QStringLiteral("handle_token"), token}};
        checkCall(message);
    }

    void checkCall(const QDBusMessage &message)
    {
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
            watcher->deleteLater();
            if (watcher->isError()) {
                setError(QStringLiteral("GlobalShortcuts portal: %1").arg(watcher->error().message()));
                qCWarning(lcHotkeys) << lastError();
                stopWatching();
            }
        });
    }

    void closeSession()
    {
        if (m_session.isEmpty())
            return;
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
            kService, m_session, QStringLiteral("org.freedesktop.portal.Session"), QStringLiteral("Close")));
        m_session.clear();
    }

    Bindings m_pending;
    QString m_session;
    QString m_watched;
    const char *m_watchedSlot = nullptr;
};

}  // namespace

HotkeyBackend *createPortalHotkeys(QObject *parent)
{
    return new PortalHotkeys(parent);
}

#include "portalhotkeys.moc"
