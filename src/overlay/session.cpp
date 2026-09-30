#include "session.h"

#include "platform.h"

#include <QCursor>
#include <QGuiApplication>

Session::Session(Kind kind, const ScreenImages &images, Settings *settings, QObject *parent)
    : QObject(parent)
    , m_kind(kind)
{
    for (const ScreenImage &shot : images) {
        if (!shot.screen || shot.image.isNull())
            continue;
        auto *window = new OverlayWindow(shot.screen, shot.image, settings, kind);
        m_windows << window;
        connect(window, &OverlayWindow::finished, this, &Session::finish);
        connect(window, &OverlayWindow::hotkeyPressed, this, &Session::hotkeyPressed);
        // Direct connections: under Wayland the clipboard must be set while
        // the overlay still has focus, i.e. before it closes.
        connect(window, &OverlayWindow::copyRequested, this, &Session::copyRequested);
        connect(window, &OverlayWindow::saved, this, &Session::saved);
        connect(window, &OverlayWindow::pointerSeen, this, &Session::activate);
    }

    // Under Wayland we only learn where the pointer is once it enters one of
    // our windows. If that never happens (pointer hidden, touch screen), use
    // the primary screen.
    m_pointerTimeout.setSingleShot(true);
    m_pointerTimeout.setInterval(250);
    connect(&m_pointerTimeout, &QTimer::timeout, this, [this] {
        if (m_active)
            return;
        OverlayWindow *fallback = nullptr;
        for (const auto &window : std::as_const(m_windows)) {
            if (window && window->targetScreen() == QGuiApplication::primaryScreen())
                fallback = window;
        }
        if (!fallback && !m_windows.isEmpty())
            fallback = m_windows.constFirst();
        if (fallback)
            activate(fallback, QPointF(fallback->width() / 2.0, fallback->height() / 2.0));
    });
}

Session::~Session()
{
    for (const auto &window : std::as_const(m_windows)) {
        if (window)
            window->deleteLater();
    }
}

Session::Response Session::respondTo(Action action) const
{
    const bool snipping = m_kind == Kind::Snip || m_kind == Kind::SnipSave;
    switch (action) {
    case Action::Zoom:
        // Ctrl+1 ends zoom and draw alike, as in ZoomIt.
        return snipping || m_kind == Kind::LiveDraw ? Response::Ignore : Response::Leave;
    case Action::Draw:
        // ZoomIt ignores Ctrl+2 while drawing; a toggle is what people expect.
        return m_kind == Kind::Draw ? Response::Leave : Response::Ignore;
    case Action::LiveDraw:
        return m_kind == Kind::LiveDraw ? Response::Leave : Response::Ignore;
    case Action::Snip:
        return snipping || m_kind == Kind::LiveDraw ? Response::Ignore : Response::CropCopy;
    case Action::SnipSave:
        return snipping || m_kind == Kind::LiveDraw ? Response::Ignore : Response::CropSave;
    default:
        return Response::Ignore;
    }
}

void Session::crop(OverlayWindow::CropPurpose purpose)
{
    if (m_active)
        m_active->startCrop(purpose);
}

void Session::start()
{
    if (m_windows.isEmpty()) {
        finish();
        return;
    }

    if (m_kind == Kind::Snip || m_kind == Kind::SnipSave) {
        // Snip on whichever monitor the user drags on; every overlay is live.
        for (const auto &window : std::as_const(m_windows)) {
            window->showOnScreen();
            window->begin(QPointF());
        }
        m_active = m_windows.constFirst();
        return;
    }

    // X11 tells us where the pointer is, so only that monitor gets an overlay.
    if (platform::isX11()) {
        const QPoint global = QCursor::pos();
        QScreen *screen = QGuiApplication::screenAt(global);
        for (const auto &window : std::as_const(m_windows)) {
            if (window->targetScreen() == screen || m_windows.size() == 1) {
                window->showOnScreen();
                activate(window, QPointF(global - window->targetScreen()->geometry().topLeft()));
                return;
            }
        }
    }

    for (const auto &window : std::as_const(m_windows))
        window->showOnScreen();
    m_pointerTimeout.start();
}

void Session::activate(OverlayWindow *window, const QPointF &pos)
{
    if (m_finished)
        return;
    if (m_kind == Kind::Snip || m_kind == Kind::SnipSave) {
        // All snip overlays stay; keys typed anywhere go to the one in use.
        m_active = window;
        for (const auto &other : std::as_const(m_windows)) {
            if (other)
                other->setKeyTarget(window);
        }
        return;
    }
    if (m_active)
        return;
    m_active = window;
    m_pointerTimeout.stop();

    // Zoom and draw work on one monitor; give the others back to the user.
    for (const auto &other : std::as_const(m_windows)) {
        if (other && other != window) {
            other->hide();
            other->deleteLater();
        }
    }
    m_windows = {window};
    window->begin(pos);
}

void Session::leave()
{
    if (m_kind == Kind::Snip || m_kind == Kind::SnipSave || !m_active) {
        finish();
        return;
    }
    m_active->leave();
}

void Session::finish()
{
    if (m_finished)
        return;
    m_finished = true;
    m_pointerTimeout.stop();
    for (const auto &window : std::as_const(m_windows)) {
        if (window)
            window->hide();
    }
    Q_EMIT finished();
    deleteLater();
}
