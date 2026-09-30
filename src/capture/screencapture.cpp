#include "screencapture.h"

#include "backends.h"
#include "platform.h"

#include <QGuiApplication>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcCapture, "app.capture")

ScreenCapture::ScreenCapture(Settings *settings, QObject *parent)
    : QObject(parent)
{
    if (platform::isX11()) {
        m_backends << new X11Capture(this);
    } else {
        if (platform::desktop() == platform::Desktop::Kde)
            m_backends << new KWinCapture(this);
        if (platform::desktop() == platform::Desktop::Gnome && ScreenCastCapture::isUsable())
            m_backends << new ScreenCastCapture(settings, this);
        if (platform::desktop() == platform::Desktop::Other && platform::hasProgram(QStringLiteral("grim")))
            m_backends << new GrimCapture(this);
        m_backends << new PortalCapture(this);
    }

    for (CaptureBackend *backend : std::as_const(m_backends)) {
        connect(backend, &CaptureBackend::finished, this, [this, backend](const ScreenImages &images) {
            if (!m_busy || m_backends.value(m_current) != backend)
                return;
            m_preferred = m_current;
            m_busy = false;
            qCDebug(lcCapture) << "captured" << images.size() << "screens via" << backend->name();
            Q_EMIT finished(images);
        });
        connect(backend, &CaptureBackend::failed, this, [this, backend](const QString &reason) {
            if (!m_busy || m_backends.value(m_current) != backend)
                return;
            qCWarning(lcCapture) << backend->name() << "failed:" << reason;
            m_errors << backend->name() + QStringLiteral(": ") + reason;
            // Walk the chain once, starting at the preferred backend.
            const int next = (m_current + 1) % m_backends.size();
            if (next == m_preferred) {
                m_busy = false;
                Q_EMIT failed(m_errors.join(QLatin1Char('\n')));
                return;
            }
            tryBackend(next);
        });
    }
}

QString ScreenCapture::backendName() const
{
    const CaptureBackend *backend = m_backends.value(m_preferred);
    return backend ? backend->name() : QString();
}

void ScreenCapture::capture()
{
    if (m_busy)
        return;
    m_busy = true;
    m_errors.clear();
    tryBackend(m_preferred);
}

void ScreenCapture::tryBackend(int index)
{
    m_current = index;
    m_backends.at(index)->capture(QGuiApplication::screens());
}

ScreenImages splitDesktopImage(const QImage &desktop, const QList<QScreen *> &screens)
{
    ScreenImages result;
    QRect virtualRect;
    for (QScreen *screen : screens)
        virtualRect |= screen->geometry();
    if (virtualRect.isEmpty() || desktop.isNull())
        return result;

    const double sx = double(desktop.width()) / virtualRect.width();
    const double sy = double(desktop.height()) / virtualRect.height();
    for (QScreen *screen : screens) {
        const QRect g = screen->geometry().translated(-virtualRect.topLeft());
        const QRect px(qRound(g.x() * sx), qRound(g.y() * sy), qRound(g.width() * sx), qRound(g.height() * sy));
        QImage part = desktop.copy(px);
        part.setDevicePixelRatio(sx);
        result.append({screen, part});
    }
    return result;
}
