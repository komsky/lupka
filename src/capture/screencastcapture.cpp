// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "backends.h"

#include "pipewireframe.h"
#include "screencast.h"
#include "settings.h"

#include <QtConcurrent>

#include <unistd.h>

namespace {

struct Grab {
    CastStream stream;
    int fd;
};

struct Grabbed {
    CastStream stream;
    QImage image;
    QString error;
};

QList<Grabbed> grabAll(const QList<Grab> &grabs)
{
    QList<Grabbed> results;
    for (const Grab &grab : grabs) {
        Grabbed result{grab.stream, {}, {}};
        result.image = pipewire::grabFrame(grab.fd, grab.stream.node, 3000, &result.error);
        results << result;
    }
    return results;
}

// The screen a stream shows: by position when the portal reports it,
// otherwise by size, otherwise the only screen there is.
QScreen *screenFor(const CastStream &stream, const QList<QPointer<QScreen>> &screens, const QList<QScreen *> &taken)
{
    auto usable = [&](QScreen *screen) { return screen && !taken.contains(screen); };
    for (QScreen *screen : screens) {
        if (usable(screen) && !stream.geometry.isEmpty() && screen->geometry().topLeft() == stream.geometry.topLeft())
            return screen;
    }
    for (QScreen *screen : screens) {
        if (usable(screen) && !stream.geometry.isEmpty() && screen->geometry().size() == stream.geometry.size())
            return screen;
    }
    for (QScreen *screen : screens) {
        if (usable(screen))
            return screen;
    }
    return nullptr;
}

}  // namespace

ScreenCastCapture::ScreenCastCapture(Settings *settings, QObject *parent)
    : CaptureBackend(parent)
    , m_settings(settings)
{
}

bool ScreenCastCapture::isUsable()
{
    return pipewire::isAvailable() && ScreenCastSession::isAvailable();
}

void ScreenCastCapture::capture(const QList<QScreen *> &screens)
{
    m_screens.clear();
    for (QScreen *screen : screens)
        m_screens << screen;
    delete m_session;
    m_session = new ScreenCastSession(this);
    connect(m_session, &ScreenCastSession::failed, this, [this](const QString &reason) {
        // A stale token (monitor unplugged, permission revoked) must not stick.
        m_settings->setScreenCastToken(QString());
        m_session->deleteLater();
        m_session = nullptr;
        Q_EMIT failed(reason);
    });
    connect(m_session, &ScreenCastSession::started, this, &ScreenCastCapture::onStarted);

    ScreenCastSession::Options options;
    options.types = ScreenCastSession::Monitor;
    options.multiple = true;
    options.cursorMode = ScreenCastSession::Hidden;
    options.restoreToken = m_settings->screenCastToken();
    m_session->start(options);
}

void ScreenCastCapture::onStarted()
{
    m_settings->setScreenCastToken(m_session->restoreToken());
    QList<Grab> grabs;
    for (const CastStream &stream : m_session->streams()) {
        QString error;
        const int fd = m_session->openPipeWireRemote(&error);
        if (fd < 0) {
            for (const Grab &g : std::as_const(grabs))
                ::close(g.fd);
            m_session->close();
            Q_EMIT failed(QStringLiteral("OpenPipeWireRemote: %1").arg(error));
            return;
        }
        grabs << Grab{stream, fd};
    }

    auto *watcher = new QFutureWatcher<QList<Grabbed>>(this);
    connect(watcher, &QFutureWatcher<QList<Grabbed>>::finished, this, [this, watcher] {
        watcher->deleteLater();
        const QList<Grabbed> results = watcher->result();
        if (m_session) {
            m_session->close();
            m_session->deleteLater();
            m_session = nullptr;
        }
        ScreenImages images;
        QList<QScreen *> taken;
        for (const Grabbed &result : results) {
            if (result.image.isNull()) {
                Q_EMIT failed(result.error);
                return;
            }
            QScreen *screen = screenFor(result.stream, m_screens, taken);
            if (!screen)
                continue;
            taken << screen;
            QImage image = result.image;
            image.setDevicePixelRatio(double(image.width()) / qMax(1, screen->geometry().width()));
            images.append({screen, image});
        }
        if (images.isEmpty())
            Q_EMIT failed(QStringLiteral("no usable frames"));
        else
            Q_EMIT finished(images);
    });
    watcher->setFuture(QtConcurrent::run(grabAll, grabs));
}
