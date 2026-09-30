#include "x11util.h"

#include <QGuiApplication>
#include <QPointer>
#include <QTimer>
#include <QWindow>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include <cstdlib>
#include <cstring>
#include <memory>

namespace x11util {
namespace {

xcb_connection_t *connection()
{
    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    return x11 ? x11->connection() : nullptr;
}

bool tryGrab(xcb_connection_t *c, xcb_window_t window)
{
    const xcb_grab_keyboard_cookie_t cookie =
        xcb_grab_keyboard(c, 1, window, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xcb_grab_keyboard_reply_t *reply = xcb_grab_keyboard_reply(c, cookie, nullptr);
    const bool ok = reply && reply->status == XCB_GRAB_STATUS_SUCCESS;
    free(reply);
    return ok;
}

xcb_atom_t atom(xcb_connection_t *c, const char *name)
{
    xcb_intern_atom_reply_t *reply =
        xcb_intern_atom_reply(c, xcb_intern_atom(c, 0, uint16_t(strlen(name)), name), nullptr);
    const xcb_atom_t result = reply ? reply->atom : xcb_atom_t(XCB_ATOM_NONE);
    free(reply);
    return result;
}

}  // namespace

void grabKeyboard(QWindow *window)
{
    xcb_connection_t *c = connection();
    if (!c || !window)
        return;
    if (tryGrab(c, xcb_window_t(window->winId())))
        return;
    auto *timer = new QTimer(window);
    QPointer<QWindow> target(window);
    auto attempts = std::make_shared<int>(0);
    timer->setInterval(25);
    QObject::connect(timer, &QTimer::timeout, window, [timer, target, attempts, c] {
        if (!target || !target->isVisible() || ++*attempts > 40 || tryGrab(c, xcb_window_t(target->winId())))
            timer->deleteLater();
    });
    timer->start();
}

void ungrabKeyboard()
{
    if (xcb_connection_t *c = connection()) {
        xcb_ungrab_keyboard(c, XCB_CURRENT_TIME);
        xcb_flush(c);
    }
}

void activate(QWindow *window)
{
    xcb_connection_t *c = connection();
    if (!c || !window)
        return;
    const xcb_window_t id = xcb_window_t(window->winId());
    const xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;

    // _NET_ACTIVE_WINDOW from a pager (source 2) is honoured without the
    // focus-stealing checks that apply to applications.
    xcb_client_message_event_t event;
    memset(&event, 0, sizeof event);
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = id;
    event.type = atom(c, "_NET_ACTIVE_WINDOW");
    event.data.data32[0] = 2;
    event.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(c, 0, root, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                   reinterpret_cast<const char *>(&event));
    xcb_set_input_focus(c, XCB_INPUT_FOCUS_PARENT, id, XCB_CURRENT_TIME);
    xcb_flush(c);
}

QList<int> keycodesFor(quint32 keysym)
{
    QList<int> result;
    xcb_connection_t *c = connection();
    if (!c || !keysym)
        return result;
    xcb_key_symbols_t *symbols = xcb_key_symbols_alloc(c);
    if (xcb_keycode_t *codes = xcb_key_symbols_get_keycode(symbols, keysym)) {
        for (xcb_keycode_t *code = codes; *code; ++code)
            result << *code;
        free(codes);
    }
    xcb_key_symbols_free(symbols);
    return result;
}

}  // namespace x11util
