#include "backends.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingReply>
#include <QFile>
#include <QRandomGenerator>
#include <QUrl>

namespace {
const QString kPortalService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPortalPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kRequestInterface = QStringLiteral("org.freedesktop.portal.Request");
}  // namespace

PortalCapture::PortalCapture(QObject *parent)
    : CaptureBackend(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(15000);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(QStringLiteral("the portal did not answer")); });
}

void PortalCapture::capture(const QList<QScreen *> &screens)
{
    unwatch();
    m_screens.clear();
    for (QScreen *screen : screens)
        m_screens << screen;
    m_active = true;
    const int generation = ++m_generation;

    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString token = QStringLiteral("capture%1").arg(QRandomGenerator::global()->generate());
    // Subscribe before calling, or a fast portal could answer before we listen.
    // The request path is fixed by the spec: .../request/<sender>/<token>.
    QString sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    watchRequest(kPortalPath + QStringLiteral("/request/") + sender + QLatin1Char('/') + token);

    QDBusMessage message = QDBusMessage::createMethodCall(kPortalService, kPortalPath,
                                                          QStringLiteral("org.freedesktop.portal.Screenshot"),
                                                          QStringLiteral("Screenshot"));
    message << QString() << QVariantMap{
        {QStringLiteral("handle_token"), token},
        {QStringLiteral("interactive"), false},
        {QStringLiteral("modal"), false},
    };
    auto *call = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call, generation] {
        call->deleteLater();
        if (generation != m_generation)
            return;
        const QDBusPendingReply<QDBusObjectPath> reply = *call;
        if (reply.isError()) {
            fail(reply.error().message());
            return;
        }
        // Portals older than 0.9 ignore handle_token and pick their own path.
        if (m_active && reply.value().path() != m_requestPath) {
            unwatch();
            watchRequest(reply.value().path());
        }
    });
    m_timeout.start();
}

void PortalCapture::watchRequest(const QString &path)
{
    m_requestPath = path;
    QDBusConnection::sessionBus().connect(kPortalService, path, kRequestInterface, QStringLiteral("Response"), this,
                                          SLOT(onResponse(uint, QVariantMap)));
}

void PortalCapture::unwatch()
{
    if (m_requestPath.isEmpty())
        return;
    QDBusConnection::sessionBus().disconnect(kPortalService, m_requestPath, kRequestInterface,
                                             QStringLiteral("Response"), this, SLOT(onResponse(uint, QVariantMap)));
    m_requestPath.clear();
}

void PortalCapture::onResponse(uint response, const QVariantMap &results)
{
    if (!m_active)
        return;
    m_timeout.stop();
    unwatch();
    if (response != 0) {
        fail(response == 1 ? QStringLiteral("screenshot was cancelled or denied") : QStringLiteral("portal error"));
        return;
    }
    const QString file = QUrl(results.value(QStringLiteral("uri")).toString()).toLocalFile();
    const QImage desktop(file);
    // The portal writes a fresh file for every request; it is ours to remove.
    if (!file.isEmpty())
        QFile::remove(file);
    if (desktop.isNull()) {
        fail(QStringLiteral("could not read %1").arg(file));
        return;
    }
    m_active = false;
    QList<QScreen *> screens;
    for (const auto &screen : std::as_const(m_screens)) {
        if (screen)
            screens << screen;
    }
    Q_EMIT finished(splitDesktopImage(desktop, screens));
}

void PortalCapture::fail(const QString &reason)
{
    if (!m_active)
        return;
    m_active = false;
    m_timeout.stop();
    unwatch();
    Q_EMIT failed(reason);
}
