#include "hotkeys.h"
#include "keynames.h"

#include <QAbstractNativeEventFilter>
#include <QGuiApplication>
#include <QLoggingCategory>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

Q_DECLARE_LOGGING_CATEGORY(lcHotkeys)

namespace {

constexpr uint16_t kRelevantMods = XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1 | XCB_MOD_MASK_4;
// Grab every combination of the lock modifiers, otherwise Num Lock or Caps
// Lock being on would make the hotkey silently stop working.
constexpr uint16_t kLockCombos[] = {0, XCB_MOD_MASK_LOCK, XCB_MOD_MASK_2, XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2};

uint16_t toX11Mods(Qt::KeyboardModifiers mods)
{
    uint16_t result = 0;
    if (mods & Qt::ShiftModifier)
        result |= XCB_MOD_MASK_SHIFT;
    if (mods & Qt::ControlModifier)
        result |= XCB_MOD_MASK_CONTROL;
    if (mods & Qt::AltModifier)
        result |= XCB_MOD_MASK_1;
    if (mods & Qt::MetaModifier)
        result |= XCB_MOD_MASK_4;
    return result;
}

// Plain XGrabKey on the root window, for X11 desktops other than GNOME and KDE.
class X11Hotkeys : public HotkeyBackend, public QAbstractNativeEventFilter
{
public:
    explicit X11Hotkeys(QObject *parent)
        : HotkeyBackend(parent)
    {
        if (auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
            m_connection = x11->connection();
        if (m_connection) {
            m_root = xcb_setup_roots_iterator(xcb_get_setup(m_connection)).data->root;
            m_symbols = xcb_key_symbols_alloc(m_connection);
            qGuiApp->installNativeEventFilter(this);
        }
    }

    ~X11Hotkeys() override
    {
        if (!m_connection)
            return;
        qGuiApp->removeNativeEventFilter(this);
        ungrabAll();
        xcb_key_symbols_free(m_symbols);
    }

    QString name() const override { return QStringLiteral("X11 key grabs"); }

    void apply(const Bindings &bindings) override
    {
        setError({});
        if (!m_connection) {
            setError(QStringLiteral("No X11 connection."));
            return;
        }
        m_bindings = bindings;
        ungrabAll();
        QStringList taken;
        for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
            for (const QKeySequence &sequence : it.value()) {
                if (sequence.isEmpty())
                    continue;
                if (!grab(sequence[0], it.key()))
                    taken << sequence.toString(QKeySequence::NativeText);
            }
        }
        xcb_flush(m_connection);
        if (!taken.isEmpty())
            setError(QStringLiteral("Another program already uses: %1").arg(taken.join(QStringLiteral(", "))));
    }

    void unregisterAll() override
    {
        ungrabAll();
        m_bindings.clear();
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override
    {
        if (eventType != "xcb_generic_event_t")
            return false;
        auto *event = static_cast<xcb_generic_event_t *>(message);
        const uint8_t type = event->response_type & ~0x80;
        if (type == XCB_MAPPING_NOTIFY) {
            // Keyboard layout changed: keycodes may have moved.
            xcb_refresh_keyboard_mapping(m_symbols, reinterpret_cast<xcb_mapping_notify_event_t *>(event));
            apply(m_bindings);
            return false;
        }
        if (type != XCB_KEY_PRESS)
            return false;
        auto *press = reinterpret_cast<xcb_key_press_event_t *>(event);
        if (press->event != m_root)
            return false;
        const uint16_t mods = press->state & kRelevantMods;
        for (const Grab &g : std::as_const(m_grabs)) {
            if (g.keycode == press->detail && g.mods == mods) {
                Q_EMIT activated(g.action);
                return true;
            }
        }
        return false;
    }

private:
    struct Grab {
        xcb_keycode_t keycode;
        uint16_t mods;
        Action action;
    };

    bool grab(QKeyCombination combo, Action action)
    {
        const uint32_t sym = keynames::keysym(combo.key());
        if (!sym)
            return false;
        xcb_keycode_t *codes = xcb_key_symbols_get_keycode(m_symbols, sym);
        if (!codes)
            return false;
        const uint16_t mods = toX11Mods(combo.keyboardModifiers());
        bool ok = true;
        for (xcb_keycode_t *code = codes; *code; ++code) {
            for (uint16_t lock : kLockCombos) {
                const xcb_void_cookie_t cookie = xcb_grab_key_checked(
                    m_connection, 1, m_root, mods | lock, *code, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
                if (xcb_generic_error_t *error = xcb_request_check(m_connection, cookie)) {
                    ok = false;
                    free(error);
                }
            }
            m_grabs.append({*code, mods, action});
        }
        free(codes);
        return ok;
    }

    void ungrabAll()
    {
        for (const Grab &g : std::as_const(m_grabs)) {
            for (uint16_t lock : kLockCombos)
                xcb_ungrab_key(m_connection, g.keycode, m_root, g.mods | lock);
        }
        m_grabs.clear();
        if (m_connection)
            xcb_flush(m_connection);
    }

    xcb_connection_t *m_connection = nullptr;
    xcb_window_t m_root = 0;
    xcb_key_symbols_t *m_symbols = nullptr;
    QList<Grab> m_grabs;
    Bindings m_bindings;
};

}  // namespace

HotkeyBackend *createX11Hotkeys(QObject *parent)
{
    return new X11Hotkeys(parent);
}
