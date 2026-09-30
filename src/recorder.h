#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QScreen>
#include <QTimer>

class ScreenCastSession;
class Settings;
struct _GstElement;

// Screen recording (ZoomIt's Ctrl+5): a whole monitor, a region of it, or a
// window, to MP4 (H.264) or, without an H.264 encoder, WebM (VP8). Under
// Wayland the pixels come from a ScreenCast portal session; under X11 from
// the root window. System audio is recorded from the default output's monitor.
class Recorder : public QObject
{
    Q_OBJECT
public:
    enum class Mode { Screen, Region, Window };

    explicit Recorder(Settings *settings, QObject *parent = nullptr);
    ~Recorder() override;

    bool isRecording() const { return m_pipeline != nullptr || m_session != nullptr; }
    // `region` is in global logical coordinates (Region mode only).
    void start(Mode mode, QScreen *screen, const QRect &region = {});
    void stop();

    static bool isAvailable();

Q_SIGNALS:
    void started();
    void finished(const QString &path);
    void failed(const QString &reason);

private:
    void startWayland();
    void onCastStarted();
    bool launch(const QString &videoSource, const QRect &cropPixels, double cropScaleFromStream);
    void pollBus();
    void teardown();
    QString outputPath(const QString &extension) const;

    Settings *m_settings;
    Mode m_mode = Mode::Screen;
    QPointer<QScreen> m_screen;
    QRect m_region;
    ScreenCastSession *m_session = nullptr;
    _GstElement *m_pipeline = nullptr;
    QString m_path;
    QTimer m_bus;
    bool m_stopping = false;
};
