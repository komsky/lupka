#include "app.h"

#include "breaktimer.h"
#include "config.h"
#include "dbusservice.h"
#include "hotkeys/hotkeys.h"
#include "imageoutput.h"
#include "livezoom.h"
#include "notify.h"
#include "platform.h"
#include "recorder.h"
#include "settings.h"
#include "ui/settingsdialog.h"
#include "ui/tray.h"

#include <QApplication>
#include <QCursor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QProcess>
#include <QSaveFile>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcApp, "app.main")

namespace {

// Presses closer together than this are key repeat or the same key reaching
// us twice (desktop hotkey and our own keyboard grab).
constexpr int kDebounceMs = 300;

QString autostartPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart/")
           + QStringLiteral(APP_ID) + QStringLiteral(".desktop");
}

QString quotedExec()
{
    QString path = QCoreApplication::applicationFilePath();
    if (path.contains(QLatin1Char(' ')))
        path = QLatin1Char('"') + path + QLatin1Char('"');
    return path;
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(contents);
    return file.commit();
}

}  // namespace

App::App(QObject *parent)
    : QObject(parent)
{
}

App::~App()
{
    delete m_break;
}

bool App::start(const QString &initialAction)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    auto *service = new DBusService(this);
    bus.registerObject(QStringLiteral("/"), service, QDBusConnection::ExportScriptableSlots);
    if (!bus.registerService(QStringLiteral(APP_ID))) {
        // Another instance started in the meantime; hand it our action.
        QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral(APP_ID), QStringLiteral("/"),
                                                           QStringLiteral(APP_ID), QStringLiteral("Trigger"));
        call << (initialAction.isEmpty() ? QStringLiteral("settings") : initialAction);
        bus.call(call, QDBus::Block, 2000);
        return false;
    }

    m_settings = new Settings(this);
    m_capture = new ScreenCapture(m_settings, this);
    connect(m_capture, &ScreenCapture::finished, this, &App::onCaptured);
    connect(m_capture, &ScreenCapture::failed, this, &App::onCaptureFailed);

    m_liveZoom = new LiveZoom(m_settings, this);
    connect(m_liveZoom, &LiveZoom::activeChanged, this, [this](bool active) {
        // While live zoom is on, Ctrl+Up/Down change its magnification.
        Bindings temporary;
        if (active) {
            temporary.insert(Action::LiveZoomIn, actionInfo(Action::LiveZoomIn).defaults);
            temporary.insert(Action::LiveZoomOut, actionInfo(Action::LiveZoomOut).defaults);
        }
        m_hotkeys->setTemporary(temporary);
        Q_EMIT liveZoomChanged(active);
    });

    // GStreamer scans its plugins on first use, which can take a second or
    // two; do it now rather than when the user presses the record key.
    (void)QtConcurrent::run([] { Recorder::isAvailable(); });

    m_recorder = new Recorder(m_settings, this);
    connect(m_recorder, &Recorder::started, this, [this] { Q_EMIT recordingChanged(true); });
    connect(m_recorder, &Recorder::finished, this, [this](const QString &path) {
        Q_EMIT recordingChanged(false);
        notify::show(tr("Recording saved"), path);
    });
    connect(m_recorder, &Recorder::failed, this, [this](const QString &reason) {
        Q_EMIT recordingChanged(false);
        notify::show(tr("Recording failed"), reason);
    });

    m_hotkeys = new Hotkeys(m_settings, this);
    connect(m_hotkeys, &Hotkeys::activated, this, qOverload<Action>(&App::trigger));
    m_hotkeys->apply();

    m_tray = new Tray(this);
    connect(m_settings, &Settings::shortcutsChanged, m_tray, &Tray::refresh);

    ensureDesktopEntry();
    applyGeneralSettings();

    qCInfo(lcApp).noquote() << APP_NAME << APP_VERSION << "on" << platform::desktopName()
                            << (platform::isWayland() ? "Wayland" : "X11") << "| capture:" << captureBackend()
                            << "| hotkeys:" << hotkeyBackend();

    if (m_settings->firstRun()) {
        m_settings->setFirstRunDone();
        showWelcome();
    }
    if (!initialAction.isEmpty())
        QMetaObject::invokeMethod(this, [this, initialAction] { triggerById(initialAction); }, Qt::QueuedConnection);
    return true;
}

QString App::hotkeyBackend() const
{
    return m_hotkeys ? m_hotkeys->backendName() : QString();
}

QString App::hotkeyStatus() const
{
    return m_hotkeys ? m_hotkeys->status() : QString();
}

QString App::captureBackend() const
{
    return m_capture ? m_capture->backendName() : QString();
}

bool App::liveZoomSupported() const
{
    return m_liveZoom && m_liveZoom->isSupported();
}

bool App::liveZoomActive() const
{
    return m_liveZoom && m_liveZoom->isActive();
}

bool App::isRecording() const
{
    return m_recorder && m_recorder->isRecording();
}

void App::triggerById(const QString &id)
{
    if (const auto action = actionFromId(id))
        trigger(*action);
    else
        qCWarning(lcApp) << "unknown action" << id;
}

bool App::debounce(Action action)
{
    QElapsedTimer &timer = m_lastTrigger[int(action)];
    if (timer.isValid() && timer.elapsed() < kDebounceMs)
        return true;
    timer.start();
    return false;
}

void App::trigger(Action action)
{
    if (m_quitting)
        return;
    switch (action) {
    case Action::Zoom:
    case Action::Draw:
    case Action::LiveDraw:
    case Action::Snip:
    case Action::SnipSave: {
        if (debounce(action))
            return;
        if (m_session) {
            switch (m_session->respondTo(action)) {
            case Session::Response::Leave:
                m_session->leave();
                break;
            case Session::Response::CropCopy:
                m_session->crop(OverlayWindow::CropPurpose::Copy);
                break;
            case Session::Response::CropSave:
                m_session->crop(OverlayWindow::CropPurpose::Save);
                break;
            case Session::Response::Ignore:
                break;
            }
            return;
        }
        if (m_break && m_break->isRunning()) {
            // As in ZoomIt, the zoom hotkey ends a running break.
            if (action == Action::Zoom)
                m_break->stop();
            return;
        }
        const Session::Kind kind = action == Action::Zoom       ? Session::Kind::Zoom
                                   : action == Action::Draw     ? Session::Kind::Draw
                                   : action == Action::LiveDraw ? Session::Kind::LiveDraw
                                   : action == Action::Snip     ? Session::Kind::Snip
                                                                : Session::Kind::SnipSave;
        beginCapture(kind);
        return;
    }
    case Action::Break:
        if (!debounce(action))
            toggleBreak();
        return;
    case Action::Record:
    case Action::RecordRegion:
    case Action::RecordWindow: {
        if (debounce(action))
            return;
        // Any record hotkey stops a running recording, as in ZoomIt.
        if (m_recorder->isRecording()) {
            m_recorder->stop();
            return;
        }
        if (action == Action::RecordRegion) {
            if (!m_session)
                beginCapture(Session::Kind::RecordRegion);
            return;
        }
        QScreen *screen = platform::isX11() ? QGuiApplication::screenAt(QCursor::pos()) : nullptr;
        m_recorder->start(action == Action::RecordWindow ? Recorder::Mode::Window : Recorder::Mode::Screen,
                          screen ? screen : QGuiApplication::primaryScreen());
        return;
    }
    case Action::LiveZoom:
        if (debounce(action))
            return;
        if (!m_liveZoom->isSupported()) {
            notify::show(tr("Live zoom is not available"),
                         tr("Live zoom uses the desktop's magnifier, which is available on GNOME and KDE Plasma."));
            return;
        }
        m_liveZoom->toggle();
        return;
    case Action::LiveZoomIn:
        m_liveZoom->zoomIn();
        return;
    case Action::LiveZoomOut:
        m_liveZoom->zoomOut();
        return;
    case Action::Settings:
        showSettings();
        return;
    case Action::Quit:
        quit();
        return;
    }
}

void App::beginCapture(std::optional<Session::Kind> kind)
{
    if (m_capture->isBusy())
        return;
    m_pending = kind;
    m_capture->capture();
}

void App::onCaptured(const ScreenImages &images)
{
    if (!m_pending) {
        // A capture for the break timer's faded-desktop background.
        if (m_pendingBreakScreen && m_break && !m_break->isRunning()) {
            QImage background;
            for (const ScreenImage &shot : images) {
                if (shot.screen == m_pendingBreakScreen)
                    background = shot.image;
            }
            m_break->start(m_pendingBreakScreen, background);
        }
        m_pendingBreakScreen = nullptr;
        return;
    }
    if (m_session)
        return;
    const Session::Kind kind = *m_pending;
    m_pending.reset();

    m_session = new Session(kind, images, m_settings, this);
    connect(m_session, &Session::hotkeyPressed, this, qOverload<Action>(&App::trigger));
    connect(m_session, &Session::copyRequested, this, &App::onCopyRequested);
    connect(m_session, &Session::saved, this, [](const QString &path) {
        notify::show(tr("Picture saved"), path, path);
    });
    connect(m_session, &Session::regionSelected, this, [this](QScreen *screen, const QRect &region) {
        m_recordScreen = screen;
        m_recordRegion = region;
    });
    connect(m_session, &Session::finished, this, [this] {
        if (m_recordRegion.isEmpty())
            return;
        // Start once the overlay is gone, so its dimming is not recorded.
        const QRect region = std::exchange(m_recordRegion, QRect());
        QTimer::singleShot(250, this, [this, region] {
            m_recorder->start(Recorder::Mode::Region, m_recordScreen, region);
        });
    });
    m_session->start();
}

void App::onCaptureFailed(const QString &reason)
{
    m_pending.reset();
    if (m_pendingBreakScreen && m_break && !m_break->isRunning())
        m_break->start(m_pendingBreakScreen, QImage());
    m_pendingBreakScreen = nullptr;
    notify::show(tr("Could not capture the screen"), reason);
}

void App::onCopyRequested(const QImage &image, bool snip)
{
    imageoutput::copyToClipboard(image);
    if (!snip)
        return;
    QString path;
    if (m_settings->snipAlsoSaves())
        path = imageoutput::saveToDirectory(image, m_settings->saveDirectory(), QStringLiteral(APP_NAME));
    notify::show(path.isEmpty() ? tr("Snip copied to the clipboard") : tr("Snip saved and copied"),
                 path.isEmpty() ? tr("%1 × %2 pixels").arg(image.width()).arg(image.height()) : path, path);
}

void App::toggleBreak()
{
    if (!m_break)
        m_break = new BreakTimer(m_settings);
    if (m_break->isRunning()) {
        // Bring a timer the user switched away from back to the front.
        m_break->bringToFront();
        return;
    }
    if (m_session)
        return;
    QScreen *screen = platform::isX11() ? QGuiApplication::screenAt(QCursor::pos()) : nullptr;
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (m_settings->breakBackground() == Settings::BreakBackground::FadedDesktop) {
        // The faded desktop needs a fresh screenshot first.
        m_pendingBreakScreen = screen;
        beginCapture(std::nullopt);
        return;
    }
    m_break->start(screen, QImage());
}

void App::showSettings()
{
    if (!m_dialog) {
        m_dialog = new SettingsDialog(this);
        m_dialog->setWindowIcon(QApplication::windowIcon());
    }
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
}

void App::applyShortcuts()
{
    m_hotkeys->apply();
    m_tray->refresh();
}

void App::applyGeneralSettings()
{
    m_tray->setVisible(m_settings->showTrayIcon());
    updateAutostart();
}

void App::updateAutostart()
{
    const QString path = autostartPath();
    if (!m_settings->startAtLogin()) {
        QFile::remove(path);
        return;
    }
    const QByteArray contents = QStringLiteral("[Desktop Entry]\n"
                                               "Type=Application\n"
                                               "Name=%1\n"
                                               "Comment=%2\n"
                                               "Exec=%3 --background\n"
                                               "Icon=%4\n"
                                               "Terminal=false\n"
                                               "NoDisplay=true\n"
                                               "X-GNOME-Autostart-enabled=true\n"
                                               "X-KDE-autostart-phase=2\n")
                                    .arg(QStringLiteral(APP_NAME), QStringLiteral(APP_SUMMARY), quotedExec(),
                                         QStringLiteral(APP_ID))
                                    .toUtf8();
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly) && existing.readAll() == contents)
        return;
    existing.close();
    if (!writeFile(path, contents))
        qCWarning(lcApp) << "could not write" << path;
}

void App::ensureDesktopEntry()
{
    // KWin only answers ScreenShot2 requests from programs whose .desktop file
    // lists the interface, matched by executable path. A packaged install has
    // one; a build run from elsewhere gets a user-level entry pointing at it.
    if (platform::desktop() != platform::Desktop::Kde || !platform::isWayland())
        return;
    const QString self = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    const QString fileName = QStringLiteral(APP_ID ".desktop");
    const QString userPath =
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + QLatin1Char('/') + fileName;

    for (const QString &path : QStandardPaths::locateAll(QStandardPaths::ApplicationsLocation, fileName)) {
        QSettings entry(path, QSettings::IniFormat);
        // KWin compares the first token of Exec= literally (after canonicalising),
        // so only an absolute path can match.
        QString exec = entry.value(QStringLiteral("Desktop Entry/Exec")).toString().section(QLatin1Char(' '), 0, 0);
        exec.remove(QLatin1Char('"'));
        const QString resolved = QFileInfo(exec).isAbsolute() ? QFileInfo(exec).canonicalFilePath() : QString();
        if (!resolved.isEmpty() && resolved == self)
            return;
    }

    const QByteArray contents = QStringLiteral("[Desktop Entry]\n"
                                               "Type=Application\n"
                                               "Name=%1\n"
                                               "Comment=%2\n"
                                               "Exec=%3\n"
                                               "Icon=%4\n"
                                               "Terminal=false\n"
                                               "Categories=Utility;Graphics;\n"
                                               "X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2\n")
                                    .arg(QStringLiteral(APP_NAME), QStringLiteral(APP_SUMMARY), quotedExec(),
                                         QStringLiteral(APP_ID))
                                    .toUtf8();
    if (!writeFile(userPath, contents))
        return;
    qCInfo(lcApp) << "wrote" << userPath << "so KWin allows screenshots";
    // KWin finds applications through the KService cache; refresh it now.
    for (const QString &tool : {QStringLiteral("kbuildsycoca6"), QStringLiteral("kbuildsycoca5")}) {
        if (platform::hasProgram(tool)) {
            QProcess::execute(tool, {});
            break;
        }
    }
}

void App::showWelcome()
{
    const auto keyText = [this](Action action) {
        const QList<QKeySequence> keys = m_settings->shortcuts(action);
        return keys.isEmpty() ? tr("(none)") : keys.constFirst().toString(QKeySequence::NativeText);
    };
    notify::show(tr("%1 is running").arg(QStringLiteral(APP_NAME)),
                 tr("%1 zoom · %2 draw · %3 snip · %4 break timer")
                     .arg(keyText(Action::Zoom), keyText(Action::Draw), keyText(Action::Snip), keyText(Action::Break)));
}

void App::quit()
{
    if (m_quitting)
        return;
    m_quitting = true;
    if (m_session)
        m_session->leave();
    if (m_liveZoom->isActive())
        m_liveZoom->toggle();
    if (m_recorder->isRecording())
        m_recorder->stop();
    // An explicit quit hands the keys back to other applications.
    m_hotkeys->unregisterAll();
    QCoreApplication::quit();
}
