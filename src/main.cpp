#include "actions.h"
#include "app.h"
#include "config.h"
#include "hotkeys/hotkeys.h"
#include "platform.h"
#include "settings.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QIcon>

#include <cstdio>
#include <unistd.h>
#include <vector>

namespace {

void printUsage()
{
    std::printf(
        "Usage: %s [ACTION] [OPTIONS]\n"
        "\n"
        "Without an action, starts %s in the background (or opens its settings if it\n"
        "is already running).\n"
        "\n"
        "Actions (sent to the running instance, which is started if needed):\n"
        "  zoom          zoom into the screen, pan with the mouse, click to draw\n"
        "  draw          draw on the screen without zooming\n"
        "  livedraw      draw over the live desktop\n"
        "  snip          copy a region of the screen to the clipboard\n"
        "  snip-save     save a region of the screen to a file\n"
        "  break         show the break timer\n"
        "  livezoom      toggle live zoom (desktop magnifier)\n"
        "  record        start or stop recording the screen\n"
        "  record-region start or stop recording a region\n"
        "  record-window start or stop recording a window\n"
        "  demotype      type the next DemoType snippet\n"
        "  demotype-back step back one DemoType snippet\n"
        "  settings      open the settings window\n"
        "  quit          quit the running instance and release its shortcuts\n"
        "\n"
        "Options:\n"
        "  --background             start without showing anything (used at login)\n"
        "  --unregister-shortcuts   remove the shortcuts and login entry %s created, then exit\n"
        "  --version                print the version\n"
        "  --help                   print this help\n",
        APP_BIN, APP_NAME, APP_NAME);
}

bool sendToRunningInstance(const QString &action)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()->isServiceRegistered(QStringLiteral(APP_ID)))
        return false;
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral(APP_ID), QStringLiteral("/"),
                                                       QStringLiteral(APP_ID), QStringLiteral("Trigger"));
    call << action;
    const QDBusMessage reply = bus.call(call, QDBus::Block, 3000);
    return reply.type() != QDBusMessage::ErrorMessage;
}

}  // namespace

int main(int argc, char *argv[])
{
    QString action;
    bool background = false;
    bool unregister = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("--help") || arg == QLatin1String("-h")) {
            printUsage();
            return 0;
        }
        if (arg == QLatin1String("--version") || arg == QLatin1String("-v")) {
            std::printf("%s %s\n", APP_BIN, APP_VERSION);
            return 0;
        }
        if (arg == QLatin1String("--background") || arg == QLatin1String("--daemon")) {
            background = true;
        } else if (arg == QLatin1String("--unregister-shortcuts")) {
            unregister = true;
        } else if (!arg.startsWith(QLatin1Char('-')) && action.isEmpty()) {
            if (!actionFromId(arg)) {
                std::fprintf(stderr, "%s: unknown action '%s'. See --help.\n", APP_BIN, argv[i]);
                return 2;
            }
            action = arg;
        } else {
            std::fprintf(stderr, "%s: unknown option '%s'. See --help.\n", APP_BIN, argv[i]);
            return 2;
        }
    }

    // The icons live in a resource compiled into the static core library,
    // which the linker would otherwise leave out.
    Q_INIT_RESOURCE(resources);
    platform::prepareEnvironment();

    // Fast path, used by every desktop hotkey: hand the action to the running
    // daemon without loading any GUI code.
    if (!background) {
        {
            QCoreApplication client(argc, argv);
            const QString forward = unregister ? QStringLiteral("quit")
                                               : (action.isEmpty() ? QStringLiteral("settings") : action);
            const bool delivered = sendToRunningInstance(forward);
            if (delivered && !unregister)
                return 0;
            if (!delivered && action == QLatin1String("quit"))
                return 0;  // nothing to quit
        }
        // Become the daemon. A fresh process image avoids creating a second
        // application object (and D-Bus connection manager) in this one.
        std::vector<QByteArray> storage = {QByteArray(argv[0]), QByteArrayLiteral("--background")};
        if (unregister)
            storage.push_back(QByteArrayLiteral("--unregister-shortcuts"));
        if (!action.isEmpty())
            storage.push_back(action.toLocal8Bit());
        std::vector<char *> args;
        for (QByteArray &arg : storage)
            args.push_back(arg.data());
        args.push_back(nullptr);
        ::execv("/proc/self/exe", args.data());
        std::perror("execv");
        return 1;
    }

    if (unregister) {
        QApplication app(argc, argv);
        app.setApplicationName(QStringLiteral(APP_BIN));
        Settings settings;
        Hotkeys hotkeys(&settings);
        hotkeys.unregisterAll();
        std::printf("Removed %s shortcuts registered through %s.\n", APP_NAME, qPrintable(hotkeys.backendName()));
        for (const QString &path : App::removeUserEntries())
            std::printf("Removed %s\n", qPrintable(path));
        return 0;
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral(APP_BIN));
    app.setApplicationDisplayName(QStringLiteral(APP_NAME));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));
    app.setDesktopFileName(QStringLiteral(APP_ID));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    app.setQuitOnLastWindowClosed(false);

    App daemon;
    if (!daemon.start(action))
        return 0;
    return app.exec();
}
