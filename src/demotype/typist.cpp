// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "typist.h"

#include "config.h"
#include "platform.h"
#include "settings.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QGuiApplication>
#include <QRandomGenerator>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xtest.h>

namespace {

constexpr quint32 kShiftL = 0xffe1;
constexpr quint32 kControlL = 0xffe3;

// ---------------------------------------------------------------- X11

// XTest: the X server treats these like real key presses.
class X11Typist : public Typist
{
public:
    explicit X11Typist(QObject *parent)
        : Typist(parent)
    {
        if (auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
            m_connection = x11->connection();
        if (m_connection)
            m_symbols = xcb_key_symbols_alloc(m_connection);
    }

    ~X11Typist() override
    {
        if (m_symbols)
            xcb_key_symbols_free(m_symbols);
    }

    void prepare() override
    {
        if (!m_connection) {
            QMetaObject::invokeMethod(this, [this] { Q_EMIT failed(QStringLiteral("no X11 connection")); },
                                      Qt::QueuedConnection);
            return;
        }
        QMetaObject::invokeMethod(this, &Typist::ready, Qt::QueuedConnection);
    }

    bool typeText(const QString &character) override
    {
        const quint32 keysym = keysymFor(character);
        xcb_keycode_t keycode = 0;
        bool shifted = false;
        if (!keysym || !lookup(keysym, &keycode, &shifted))
            return false;
        if (shifted)
            fake(XCB_KEY_PRESS, lookupKeycode(kShiftL));
        fake(XCB_KEY_PRESS, keycode);
        fake(XCB_KEY_RELEASE, keycode);
        if (shifted)
            fake(XCB_KEY_RELEASE, lookupKeycode(kShiftL));
        xcb_flush(m_connection);
        return true;
    }

    void pressKey(quint32 keysym, bool withControl) override
    {
        const xcb_keycode_t keycode = lookupKeycode(keysym);
        if (!keycode)
            return;
        if (withControl)
            fake(XCB_KEY_PRESS, lookupKeycode(kControlL));
        fake(XCB_KEY_PRESS, keycode);
        fake(XCB_KEY_RELEASE, keycode);
        if (withControl)
            fake(XCB_KEY_RELEASE, lookupKeycode(kControlL));
        xcb_flush(m_connection);
    }

    void finish() override {}

    bool modifiersHeld() const override
    {
        if (!m_connection)
            return false;
        const xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(m_connection)).data->root;
        xcb_query_pointer_reply_t *reply =
            xcb_query_pointer_reply(m_connection, xcb_query_pointer(m_connection, root), nullptr);
        const bool held = reply && (reply->mask & (XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1 | XCB_MOD_MASK_4));
        free(reply);
        return held;
    }

private:
    void fake(uint8_t type, xcb_keycode_t keycode)
    {
        if (keycode)
            xcb_test_fake_input(m_connection, type, keycode, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    }

    xcb_keycode_t lookupKeycode(quint32 keysym)
    {
        xcb_keycode_t keycode = 0;
        bool shifted = false;
        lookup(keysym, &keycode, &shifted);
        return keycode;
    }

    bool lookup(quint32 keysym, xcb_keycode_t *keycode, bool *shifted)
    {
        xcb_keycode_t *codes = xcb_key_symbols_get_keycode(m_symbols, keysym);
        if (!codes)
            return false;
        bool found = false;
        for (xcb_keycode_t *code = codes; *code && !found; ++code) {
            for (int column = 0; column < 2; ++column) {
                if (xcb_key_symbols_get_keysym(m_symbols, *code, column) == keysym) {
                    *keycode = *code;
                    *shifted = column == 1;
                    found = true;
                    break;
                }
            }
        }
        free(codes);
        return found;
    }

    xcb_connection_t *m_connection = nullptr;
    xcb_key_symbols_t *m_symbols = nullptr;
};

// ---------------------------------------------------------------- Wayland

const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kInterface = QStringLiteral("org.freedesktop.portal.RemoteDesktop");

// org.freedesktop.portal.RemoteDesktop with keyboard access. The desktop asks
// once; the restore token lets later sessions start silently.
class PortalTypist : public Typist
{
    Q_OBJECT
public:
    PortalTypist(Settings *settings, QObject *parent)
        : Typist(parent)
        , m_settings(settings)
    {
        m_timeout.setSingleShot(true);
        m_timeout.setInterval(120000);
        connect(&m_timeout, &QTimer::timeout, this, [this] { fail(QStringLiteral("the desktop did not answer")); });
    }

    ~PortalTypist() override { finish(); }

    void prepare() override
    {
        finish();
        m_timeout.start();
        request(QStringLiteral("CreateSession"), {}, {{QStringLiteral("session_handle_token"), token()}},
                SLOT(onCreateSession(uint, QVariantMap)));
    }

    bool typeText(const QString &character) override
    {
        const quint32 keysym = keysymFor(character);
        if (!keysym || m_session.isEmpty())
            return false;
        // The compositor finds the key (and Shift) for the keysym in the
        // current layout; characters outside it are pasted instead.
        if (character.size() > 1 || character.at(0).unicode() > 0x7e)
            return false;
        send(keysym, true);
        send(keysym, false);
        return true;
    }

    void pressKey(quint32 keysym, bool withControl) override
    {
        if (m_session.isEmpty())
            return;
        if (withControl)
            send(kControlL, true);
        send(keysym, true);
        send(keysym, false);
        if (withControl)
            send(kControlL, false);
    }

    void finish() override
    {
        m_timeout.stop();
        stopWatching();
        if (m_session.isEmpty())
            return;
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
            kService, m_session, QStringLiteral("org.freedesktop.portal.Session"), QStringLiteral("Close")));
        m_session.clear();
    }

private Q_SLOTS:
    void onCreateSession(uint response, const QVariantMap &results)
    {
        stopWatching();
        if (response != 0) {
            fail(QStringLiteral("the desktop refused remote typing"));
            return;
        }
        m_pending = results.value(QStringLiteral("session_handle")).toString();
        QVariantMap options{{QStringLiteral("types"), uint(1)}, {QStringLiteral("persist_mode"), uint(2)}};
        if (!m_settings->demoTypeToken().isEmpty())
            options.insert(QStringLiteral("restore_token"), m_settings->demoTypeToken());
        request(QStringLiteral("SelectDevices"), {QVariant::fromValue(QDBusObjectPath(m_pending))}, options,
                SLOT(onSelectDevices(uint, QVariantMap)));
    }

    void onSelectDevices(uint response, const QVariantMap &)
    {
        stopWatching();
        if (response != 0) {
            fail(QStringLiteral("keyboard access was not granted"));
            return;
        }
        request(QStringLiteral("Start"), {QVariant::fromValue(QDBusObjectPath(m_pending)), QString()}, {},
                SLOT(onStart(uint, QVariantMap)));
    }

    void onStart(uint response, const QVariantMap &results)
    {
        stopWatching();
        m_timeout.stop();
        if (response != 0) {
            m_settings->setDemoTypeToken(QString());
            fail(response == 1 ? QStringLiteral("remote typing was cancelled") : QStringLiteral("remote typing failed"));
            return;
        }
        m_session = m_pending;
        m_settings->setDemoTypeToken(results.value(QStringLiteral("restore_token")).toString());
        Q_EMIT ready();
    }

private:
    static QString token()
    {
        return QStringLiteral(APP_BIN "%1").arg(QRandomGenerator::global()->generate());
    }

    void request(const QString &method, QList<QVariant> args, QVariantMap options, const char *slot)
    {
        const QString handle = token();
        options.insert(QStringLiteral("handle_token"), handle);
        QString sender = QDBusConnection::sessionBus().baseService().mid(1);
        sender.replace(QLatin1Char('.'), QLatin1Char('_'));
        m_watched = kPath + QStringLiteral("/request/") + sender + QLatin1Char('/') + handle;
        m_watchedSlot = slot;
        QDBusConnection::sessionBus().connect(kService, m_watched, QStringLiteral("org.freedesktop.portal.Request"),
                                              QStringLiteral("Response"), this, slot);
        QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, kInterface, method);
        args << options;
        message.setArguments(args);
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, method] {
            watcher->deleteLater();
            if (watcher->isError())
                fail(method + QStringLiteral(": ") + watcher->error().message());
        });
    }

    void stopWatching()
    {
        if (m_watched.isEmpty())
            return;
        QDBusConnection::sessionBus().disconnect(kService, m_watched, QStringLiteral("org.freedesktop.portal.Request"),
                                                 QStringLiteral("Response"), this, m_watchedSlot);
        m_watched.clear();
    }

    void send(quint32 keysym, bool pressed)
    {
        QDBusMessage message =
            QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("NotifyKeyboardKeysym"));
        message << QVariant::fromValue(QDBusObjectPath(m_session)) << QVariantMap() << int(keysym)
                << uint(pressed ? 1 : 0);
        QDBusConnection::sessionBus().call(message, QDBus::Block, 1000);
    }

    void fail(const QString &reason)
    {
        finish();
        Q_EMIT failed(reason);
    }

    Settings *m_settings;
    QString m_pending;
    QString m_session;
    QString m_watched;
    const char *m_watchedSlot = nullptr;
    QTimer m_timeout;
};

}  // namespace

quint32 Typist::keysymFor(const QString &character)
{
    if (character.isEmpty())
        return 0;
    const uint code = character.toUcs4().value(0);
    if (code == '\t')
        return 0xff09;
    if ((code >= 0x20 && code <= 0x7e) || (code >= 0xa0 && code <= 0xff))
        return code;
    return code >= 0x100 ? 0x01000000 | code : 0;
}

Typist *Typist::create(Settings *settings, QObject *parent)
{
    if (platform::isX11())
        return new X11Typist(parent);
    return new PortalTypist(settings, parent);
}

#include "typist.moc"
