#include "backends.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QtConcurrent>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace {

QByteArray readAll(int fd)
{
    QByteArray data;
    char buffer[65536];
    for (;;) {
        const ssize_t n = ::read(fd, buffer, sizeof buffer);
        if (n > 0) {
            data.append(buffer, int(n));
        } else if (n == 0) {
            break;
        } else if (errno != EINTR) {
            data.clear();
            break;
        }
    }
    ::close(fd);
    return data;
}

}  // namespace

void KWinCapture::capture(const QList<QScreen *> &screens)
{
    ++m_generation;
    m_jobs.clear();
    m_failed = false;
    if (screens.isEmpty()) {
        QMetaObject::invokeMethod(this, [this] { fail(QStringLiteral("no screens")); }, Qt::QueuedConnection);
        return;
    }
    for (QScreen *screen : screens)
        m_jobs.append(Job{screen, {}, {}, false, false});
    for (int i = 0; i < m_jobs.size(); ++i)
        startJob(i);
}

void KWinCapture::startJob(int index)
{
    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        fail(QStringLiteral("pipe() failed"));
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/org/kde/KWin/ScreenShot2"),
        QStringLiteral("org.kde.KWin.ScreenShot2"), QStringLiteral("CaptureScreen"));
    const QVariantMap options{
        {QStringLiteral("native-resolution"), true},
        {QStringLiteral("include-cursor"), false},
    };
    message << m_jobs.at(index).screen->name() << options
            << QVariant::fromValue(QDBusUnixFileDescriptor(fds[1]));
    ::close(fds[1]);  // QDBusUnixFileDescriptor holds its own duplicate

    const int generation = m_generation;
    auto *reader = new QFutureWatcher<QByteArray>(this);
    connect(reader, &QFutureWatcher<QByteArray>::finished, this, [this, reader, index, generation] {
        reader->deleteLater();
        if (generation != m_generation)
            return;
        m_jobs[index].data = reader->result();
        m_jobs[index].dataDone = true;
        jobUpdated();
    });
    reader->setFuture(QtConcurrent::run(readAll, fds[0]));

    auto *call = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 5000), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call, index, generation] {
        call->deleteLater();
        if (generation != m_generation)
            return;
        const QDBusPendingReply<QVariantMap> reply = *call;
        if (reply.isError()) {
            fail(reply.error().name() + QStringLiteral(": ") + reply.error().message());
            return;
        }
        m_jobs[index].meta = reply.value();
        m_jobs[index].metaDone = true;
        jobUpdated();
    });
}

void KWinCapture::jobUpdated()
{
    if (m_failed)
        return;
    for (const Job &job : std::as_const(m_jobs)) {
        if (!job.dataDone || !job.metaDone)
            return;
    }

    ScreenImages images;
    for (const Job &job : std::as_const(m_jobs)) {
        const int width = job.meta.value(QStringLiteral("width")).toInt();
        const int height = job.meta.value(QStringLiteral("height")).toInt();
        const int stride = job.meta.value(QStringLiteral("stride")).toInt();
        const auto format = QImage::Format(job.meta.value(QStringLiteral("format")).toUInt());
        const double scale = job.meta.value(QStringLiteral("scale"), 1.0).toDouble();
        if (width <= 0 || height <= 0 || stride <= 0 || job.data.size() < qsizetype(stride) * height) {
            fail(QStringLiteral("unexpected image data from KWin"));
            return;
        }
        QImage image(reinterpret_cast<const uchar *>(job.data.constData()), width, height, stride, format);
        image = image.copy();  // detach from the QByteArray
        image.setDevicePixelRatio(scale > 0 ? scale : 1.0);
        images.append({job.screen, image});
    }
    ++m_generation;
    Q_EMIT finished(images);
}

void KWinCapture::fail(const QString &reason)
{
    if (m_failed)
        return;
    m_failed = true;
    ++m_generation;
    Q_EMIT failed(reason);
}
