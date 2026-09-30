#pragma once

#include "actions.h"
#include "capture/screencapture.h"
#include "overlay/session.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>

#include <optional>

class BreakTimer;
class Hotkeys;
class LiveZoom;
class Settings;
class SettingsDialog;
class Tray;

// The background daemon: owns the settings, hotkeys, tray icon and whatever
// zoom/draw/snip session is running.
class App : public QObject
{
    Q_OBJECT
public:
    explicit App(QObject *parent = nullptr);
    ~App() override;

    // Register on D-Bus and set everything up. Returns false if another
    // instance won the race; the action was then forwarded to it.
    bool start(const QString &initialAction);

    Settings *settings() const { return m_settings; }
    QString hotkeyBackend() const;
    QString hotkeyStatus() const;
    QString captureBackend() const;
    bool liveZoomSupported() const;
    bool liveZoomActive() const;

public Q_SLOTS:
    void trigger(Action action);
    void triggerById(const QString &id);
    void applyShortcuts();
    void applyGeneralSettings();
    void quit();

Q_SIGNALS:
    void liveZoomChanged(bool active);

private:
    void beginCapture(std::optional<Session::Kind> kind);
    void onCaptured(const ScreenImages &images);
    void onCaptureFailed(const QString &reason);
    void onCopyRequested(const QImage &image, bool snip);
    void toggleBreak();
    void showSettings();
    void updateAutostart();
    void ensureDesktopEntry();
    void showWelcome();
    bool debounce(Action action);

    Settings *m_settings = nullptr;
    Hotkeys *m_hotkeys = nullptr;
    ScreenCapture *m_capture = nullptr;
    LiveZoom *m_liveZoom = nullptr;
    Tray *m_tray = nullptr;
    BreakTimer *m_break = nullptr;
    QPointer<Session> m_session;
    QPointer<SettingsDialog> m_dialog;
    std::optional<Session::Kind> m_pending;
    QPointer<QScreen> m_pendingBreakScreen;
    QHash<int, QElapsedTimer> m_lastTrigger;
    bool m_quitting = false;
};
