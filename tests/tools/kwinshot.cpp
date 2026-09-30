// Test helper: save KWin's whole workspace to a PNG through
// org.kde.KWin.ScreenShot2 (run KWin with KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1).
#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QImage>

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::fprintf(stderr, "usage: kwinshot OUTPUT.png\n");
        return 2;
    }
    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) != 0)
        return 1;
    QDBusMessage reply;
    {
        // The message holds a duplicate of the write end; it must be gone
        // before reading, or the pipe never reaches end-of-file.
        QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"), QStringLiteral("/org/kde/KWin/ScreenShot2"),
            QStringLiteral("org.kde.KWin.ScreenShot2"), QStringLiteral("CaptureWorkspace"));
        message << QVariantMap{{QStringLiteral("include-cursor"), false}}
                << QVariant::fromValue(QDBusUnixFileDescriptor(fds[1]));
        ::close(fds[1]);
        reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 10000);
    }
    if (reply.type() != QDBusMessage::ReplyMessage) {
        std::fprintf(stderr, "kwinshot: %s\n", qPrintable(reply.errorMessage()));
        return 1;
    }
    QByteArray data;
    char buffer[65536];
    ssize_t n;
    while ((n = ::read(fds[0], buffer, sizeof buffer)) > 0)
        data.append(buffer, int(n));
    ::close(fds[0]);
    const auto meta = qdbus_cast<QVariantMap>(reply.arguments().constFirst());
    const QImage image(reinterpret_cast<const uchar *>(data.constData()), meta.value(QStringLiteral("width")).toInt(),
                       meta.value(QStringLiteral("height")).toInt(), meta.value(QStringLiteral("stride")).toInt(),
                       QImage::Format(meta.value(QStringLiteral("format")).toUInt()));
    return image.copy().save(QString::fromLocal8Bit(argv[1])) ? 0 : 1;
}
