#pragma once

#include <QObject>

class Settings;

// Live zoom (ZoomIt's Ctrl+4): the screen stays interactive while magnified.
// Rather than re-implementing a magnifier, this drives the compositor's own,
// which is the only thing that can magnify a live Wayland desktop: GNOME's
// accessibility zoom and KWin's Zoom effect.
class LiveZoom : public QObject
{
    Q_OBJECT
public:
    explicit LiveZoom(Settings *settings, QObject *parent = nullptr);
    ~LiveZoom() override;

    bool isSupported() const;
    bool isActive() const { return m_active; }

    void toggle();
    void zoomIn();
    void zoomOut();

Q_SIGNALS:
    void activeChanged(bool active);

private:
    void setActive(bool active);
    void applyFactor();

    Settings *m_settings;
    bool m_active = false;
    double m_factor = 2.0;
    int m_kwinSteps = 0;
    // GNOME values to restore when live zoom ends.
    bool m_savedEnabled = false;
    double m_savedFactor = 0;
    QString m_savedTracking;
};
