#include "config.h"
#include "hotkeys.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcHotkeys)

namespace {

// kglobalaccel marshals a QKeySequence as a struct holding four key codes.
struct DBusKeySequence {
    QList<int> keys;
};

QDBusArgument &operator<<(QDBusArgument &argument, const DBusKeySequence &sequence)
{
    argument.beginStructure();
    argument << sequence.keys;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, DBusKeySequence &sequence)
{
    argument.beginStructure();
    argument >> sequence.keys;
    argument.endStructure();
    return argument;
}

}  // namespace

Q_DECLARE_METATYPE(DBusKeySequence)

namespace {

const QString kService = QStringLiteral("org.kde.kglobalaccel");
const QString kPath = QStringLiteral("/kglobalaccel");
const QString kInterface = QStringLiteral("org.kde.KGlobalAccel");
const QString kComponentInterface = QStringLiteral("org.kde.kglobalaccel.Component");

// Flags understood by setShortcut / setShortcutKeys.
constexpr uint kSetPresent = 2;
constexpr uint kNoAutoloading = 4;
constexpr uint kIsDefault = 8;

// KDE's own global shortcut daemon, the same thing KDE applications use. The
// shortcuts appear in System Settings > Shortcuts under our name. Works on
// Plasma 5 and 6, X11 and Wayland, and delivers presses straight to us.
class KdeHotkeys : public HotkeyBackend
{
    Q_OBJECT
public:
    explicit KdeHotkeys(QObject *parent)
        : HotkeyBackend(parent)
    {
        qDBusRegisterMetaType<DBusKeySequence>();
        qDBusRegisterMetaType<QList<DBusKeySequence>>();
        // kglobalaccel can restart (it lives in KWin on Plasma 6); register again.
        auto *watcher = new QDBusServiceWatcher(kService, QDBusConnection::sessionBus(),
                                                QDBusServiceWatcher::WatchForRegistration, this);
        connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] {
            m_componentConnected = false;
            if (!m_last.isEmpty())
                apply(m_last);
        });
    }

    QString name() const override { return QStringLiteral("KDE global shortcuts"); }

    void apply(const Bindings &bindings) override
    {
        setError({});
        m_last = bindings;
        QStringList failures;
        QStringList taken;
        for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
            const QStringList id = actionId(it.key());
            call(QStringLiteral("doRegister"), {id});
            QList<int> keys;
            for (const QKeySequence &sequence : it.value()) {
                if (!sequence.isEmpty())
                    keys << sequence[0].toCombined();
            }
            // Our settings are the source of truth, so disable autoloading of
            // whatever kglobalaccel remembered from last time.
            QList<int> granted;
            if (!setKeys(id, keys, kSetPresent | kNoAutoloading, &granted))
                failures << actionInfo(it.key()).label;
            // kglobalaccel silently drops keys another application owns.
            for (int key : std::as_const(keys)) {
                if (!granted.contains(key))
                    taken << QKeySequence(key).toString(QKeySequence::NativeText);
            }
            setKeys(id, defaultKeys(it.key()), kIsDefault);
            if (!m_registered.contains(it.key()))
                m_registered << it.key();
        }
        // Actions that are no longer bound (temporary ones) get released.
        for (Action action : std::as_const(m_registered)) {
            if (!bindings.contains(action)) {
                setKeys(actionId(action), {}, kSetPresent | kNoAutoloading);
                call(QStringLiteral("setInactive"), {actionId(action)});
            }
        }
        connectComponent();
        QStringList problems;
        if (!failures.isEmpty())
            problems << QStringLiteral("KDE refused shortcuts for: %1").arg(failures.join(QStringLiteral(", ")));
        if (!taken.isEmpty())
            problems << QStringLiteral("Already used by another application: %1").arg(taken.join(QStringLiteral(", ")));
        setError(problems.join(QLatin1Char('\n')));
    }

    void unregisterAll() override
    {
        for (const ActionInfo &info : actionTable()) {
            if (!info.configurable && info.action != Action::LiveZoomIn && info.action != Action::LiveZoomOut)
                continue;
            call(QStringLiteral("setInactive"), {actionId(info.action)});
            call(QStringLiteral("unregister"), {QStringLiteral(APP_BIN), info.id});
        }
        m_registered.clear();
    }

private Q_SLOTS:
    void onPressed(const QString &component, const QString &shortcut, qlonglong /*timestamp*/)
    {
        if (component != QLatin1String(APP_BIN))
            return;
        if (const auto action = actionFromId(shortcut))
            Q_EMIT activated(*action);
    }

private:
    static QStringList actionId(Action action)
    {
        const ActionInfo &info = actionInfo(action);
        // componentUnique, actionUnique, componentFriendly, actionFriendly
        return {QStringLiteral(APP_BIN), info.id, QStringLiteral(APP_NAME), info.label};
    }

    static QList<int> defaultKeys(Action action)
    {
        QList<int> keys;
        for (const QKeySequence &sequence : actionInfo(action).defaults)
            keys << sequence[0].toCombined();
        return keys;
    }

    QDBusMessage call(const QString &method, const QVariantList &args)
    {
        QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, kInterface, method);
        message.setArguments(args);
        return QDBusConnection::sessionBus().call(message, QDBus::Block, 3000);
    }

    bool setKeys(const QStringList &id, const QList<int> &keys, uint flags, QList<int> *granted = nullptr)
    {
        // Plasma 6 (and late KF5) take QList<QKeySequence>; older ones a flat int list.
        if (m_useKeySequences) {
            QList<DBusKeySequence> sequences;
            for (int key : keys)
                sequences << DBusKeySequence{{key, 0, 0, 0}};
            const QDBusMessage reply =
                call(QStringLiteral("setShortcutKeys"), {id, QVariant::fromValue(sequences), flags});
            if (reply.type() != QDBusMessage::ErrorMessage) {
                if (granted && !reply.arguments().isEmpty()) {
                    const auto result = qdbus_cast<QList<DBusKeySequence>>(reply.arguments().constFirst());
                    for (const DBusKeySequence &sequence : result) {
                        if (!sequence.keys.isEmpty() && sequence.keys.constFirst() != 0)
                            *granted << sequence.keys.constFirst();
                    }
                }
                return true;
            }
            if (reply.errorName() != QLatin1String("org.freedesktop.DBus.Error.UnknownMethod")) {
                qCWarning(lcHotkeys) << "setShortcutKeys failed:" << reply.errorMessage();
                return false;
            }
            m_useKeySequences = false;
        }
        const QDBusMessage reply = call(QStringLiteral("setShortcut"), {id, QVariant::fromValue(keys), flags});
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qCWarning(lcHotkeys) << "setShortcut failed:" << reply.errorMessage();
            return false;
        }
        if (granted && !reply.arguments().isEmpty()) {
            for (int key : qdbus_cast<QList<int>>(reply.arguments().constFirst())) {
                if (key)
                    *granted << key;
            }
        }
        return true;
    }

    void connectComponent()
    {
        if (m_componentConnected)
            return;
        const QDBusMessage reply = call(QStringLiteral("getComponent"), {QStringLiteral(APP_BIN)});
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
            qCWarning(lcHotkeys) << "getComponent failed:" << reply.errorMessage();
            return;
        }
        const QString path = qvariant_cast<QDBusObjectPath>(reply.arguments().constFirst()).path();
        m_componentConnected = QDBusConnection::sessionBus().connect(
            kService, path, kComponentInterface, QStringLiteral("globalShortcutPressed"), this,
            SLOT(onPressed(QString, QString, qlonglong)));
    }

    Bindings m_last;
    bool m_useKeySequences = true;
    bool m_componentConnected = false;
    QList<Action> m_registered;
};

}  // namespace

HotkeyBackend *createKdeHotkeys(QObject *parent)
{
    return new KdeHotkeys(parent);
}

#include "kdehotkeys.moc"
