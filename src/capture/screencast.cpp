#include "screencast.h"

#include "config.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QRandomGenerator>

#include <fcntl.h>
#include <unistd.h>

namespace {

const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kInterface = QStringLiteral("org.freedesktop.portal.ScreenCast");
const QString kRequest = QStringLiteral("org.freedesktop.portal.Request");

QString newToken()
{
    return QStringLiteral(APP_BIN "%1").arg(QRandomGenerator::global()->generate());
}

QString requestPath(const QString &token)
{
    QString sender = QDBusConnection::sessionBus().baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    return kPath + QStringLiteral("/request/") + sender + QLatin1Char('/') + token;
}

QPoint readPair(const QVariant &value)
{
    // (ii) arrives as a QDBusArgument.
    int a = 0;
    int b = 0;
    if (value.canConvert<QDBusArgument>()) {
        const QDBusArgument arg = value.value<QDBusArgument>();
        arg.beginStructure();
        arg >> a >> b;
        arg.endStructure();
    }
    return QPoint(a, b);
}

QList<CastStream> readStreams(const QVariant &value)
{
    QList<CastStream> streams;
    if (!value.canConvert<QDBusArgument>())
        return streams;
    const QDBusArgument arg = value.value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd()) {
        CastStream stream;
        QVariantMap properties;
        arg.beginStructure();
        arg >> stream.node >> properties;
        arg.endStructure();
        if (properties.contains(QStringLiteral("position")) && properties.contains(QStringLiteral("size"))) {
            const QPoint pos = readPair(properties.value(QStringLiteral("position")));
            const QPoint size = readPair(properties.value(QStringLiteral("size")));
            stream.geometry = QRect(pos, QSize(size.x(), size.y()));
        }
        stream.sourceType = properties.value(QStringLiteral("source_type")).toUInt();
        streams << stream;
    }
    arg.endArray();
    return streams;
}

}  // namespace

ScreenCastSession::ScreenCastSession(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(QStringLiteral("the screen-sharing request timed out")); });
}

ScreenCastSession::~ScreenCastSession()
{
    close();
}

bool ScreenCastSession::isAvailable()
{
    QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                          QStringLiteral("Get"));
    message << kInterface << QStringLiteral("version");
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    return reply.type() == QDBusMessage::ReplyMessage;
}

void ScreenCastSession::start(const Options &options)
{
    m_options = options;
    m_active = true;
    m_streams.clear();
    m_timeout.start(options.timeoutMs);
    request(QStringLiteral("CreateSession"), {},
            {{QStringLiteral("session_handle_token"), newToken()}},
            SLOT(onCreateSessionResponse(uint, QVariantMap)));
}

void ScreenCastSession::request(const QString &method, const QList<QVariant> &args, QVariantMap options,
                                const char *slot)
{
    // Listen on the predictable request path before calling, so a fast answer is not lost.
    const QString token = newToken();
    options.insert(QStringLiteral("handle_token"), token);
    stopWatching();
    m_watchedPath = requestPath(token);
    m_watchedSlot = slot;
    QDBusConnection::sessionBus().connect(kService, m_watchedPath, kRequest, QStringLiteral("Response"), this, slot);

    QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, kInterface, method);
    QList<QVariant> all = args;
    all << options;
    message.setArguments(all);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, method] {
        watcher->deleteLater();
        if (watcher->isError())
            fail(QStringLiteral("%1: %2").arg(method, watcher->error().message()));
    });
}

void ScreenCastSession::stopWatching()
{
    if (m_watchedPath.isEmpty())
        return;
    QDBusConnection::sessionBus().disconnect(kService, m_watchedPath, kRequest, QStringLiteral("Response"), this,
                                             m_watchedSlot);
    m_watchedPath.clear();
}

void ScreenCastSession::onCreateSessionResponse(uint response, const QVariantMap &results)
{
    stopWatching();
    if (!m_active)
        return;
    if (response != 0) {
        fail(QStringLiteral("the desktop refused to start screen sharing"));
        return;
    }
    m_session = results.value(QStringLiteral("session_handle")).toString();
    QVariantMap options{
        {QStringLiteral("types"), m_options.types},
        {QStringLiteral("multiple"), m_options.multiple},
        {QStringLiteral("cursor_mode"), m_options.cursorMode},
    };
    if (m_options.persist)
        options.insert(QStringLiteral("persist_mode"), uint(2));
    if (!m_options.restoreToken.isEmpty())
        options.insert(QStringLiteral("restore_token"), m_options.restoreToken);
    request(QStringLiteral("SelectSources"), {QVariant::fromValue(QDBusObjectPath(m_session))}, options,
            SLOT(onSelectSourcesResponse(uint, QVariantMap)));
}

void ScreenCastSession::onSelectSourcesResponse(uint response, const QVariantMap &)
{
    stopWatching();
    if (!m_active)
        return;
    if (response != 0) {
        fail(QStringLiteral("no screen was selected"));
        return;
    }
    request(QStringLiteral("Start"), {QVariant::fromValue(QDBusObjectPath(m_session)), QString()}, {},
            SLOT(onStartResponse(uint, QVariantMap)));
}

void ScreenCastSession::onStartResponse(uint response, const QVariantMap &results)
{
    stopWatching();
    if (!m_active)
        return;
    if (response != 0) {
        fail(response == 1 ? QStringLiteral("screen sharing was cancelled") : QStringLiteral("screen sharing failed"));
        return;
    }
    m_streams = readStreams(results.value(QStringLiteral("streams")));
    m_restoreToken = results.value(QStringLiteral("restore_token")).toString();
    if (m_streams.isEmpty()) {
        fail(QStringLiteral("the desktop offered no screens"));
        return;
    }
    m_timeout.stop();
    Q_EMIT started();
}

int ScreenCastSession::openPipeWireRemote(QString *error)
{
    QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("OpenPipeWireRemote"));
    message << QVariant::fromValue(QDBusObjectPath(m_session)) << QVariantMap();
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 5000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        if (error)
            *error = reply.errorMessage();
        return -1;
    }
    const auto fd = qvariant_cast<QDBusUnixFileDescriptor>(reply.arguments().constFirst());
    // The descriptor object closes its copy; keep our own.
    return fd.isValid() ? ::fcntl(fd.fileDescriptor(), F_DUPFD_CLOEXEC, 3) : -1;
}

void ScreenCastSession::close()
{
    m_active = false;
    m_timeout.stop();
    stopWatching();
    if (m_session.isEmpty())
        return;
    QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
        kService, m_session, QStringLiteral("org.freedesktop.portal.Session"), QStringLiteral("Close")));
    m_session.clear();
}

void ScreenCastSession::fail(const QString &reason)
{
    if (!m_active)
        return;
    close();
    Q_EMIT failed(reason);
}
