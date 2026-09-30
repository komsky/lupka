#pragma once

#include "actions.h"

#include <QColor>
#include <QObject>
#include <QSettings>

// Typed access to ~/.config/<app>/<app>.conf.
class Settings : public QObject
{
    Q_OBJECT
public:
    explicit Settings(QObject *parent = nullptr);
    // For tests: settings stored in an explicit file.
    Settings(const QString &file, QObject *parent);

    QString fileName() const { return m_store.fileName(); }

    // Zoom
    double initialZoom() const;
    void setInitialZoom(double zoom);
    bool animateZoom() const;
    void setAnimateZoom(bool on);
    bool smoothZoom() const;
    void setSmoothZoom(bool on);

    // Drawing
    int penWidth() const;  // in screen pixels before magnification
    void setPenWidth(int width);
    QColor penColor() const;
    void setPenColor(const QColor &color);
    QString fontFamily() const;
    void setFontFamily(const QString &family);
    // Text height is the visible screen height divided by this (ZoomIt's FontScale).
    int fontScale() const;
    void setFontScale(int scale);

    // Hotkeys. An empty list means "disabled".
    QList<QKeySequence> shortcuts(Action action) const;
    void setShortcuts(Action action, const QList<QKeySequence> &keys);
    void resetShortcuts();

    // Snip
    QString saveDirectory() const;
    void setSaveDirectory(const QString &dir);
    QString lastSaveDirectory() const;
    void setLastSaveDirectory(const QString &dir);
    bool snipAlsoSaves() const;
    void setSnipAlsoSaves(bool on);

    // Break timer
    enum class BreakBackground { Plain, FadedDesktop, Image };
    int breakMinutes() const;
    void setBreakMinutes(int minutes);
    bool breakPlaySound() const;
    void setBreakPlaySound(bool on);
    QString breakSoundFile() const;  // empty: the desktop's alarm sound
    void setBreakSoundFile(const QString &file);
    bool breakShowElapsed() const;
    void setBreakShowElapsed(bool on);
    int breakOpacity() const;  // 10..100, opacity of the timer window
    void setBreakOpacity(int percent);
    int breakPosition() const;  // 0..8, a 3x3 grid read left to right, top to bottom
    void setBreakPosition(int position);
    QColor breakTextColor() const;
    void setBreakTextColor(const QColor &color);
    QColor breakBackgroundColor() const;
    void setBreakBackgroundColor(const QColor &color);
    BreakBackground breakBackground() const;
    void setBreakBackground(BreakBackground mode);
    QString breakImageFile() const;
    void setBreakImageFile(const QString &file);
    bool breakScaleImage() const;
    void setBreakScaleImage(bool on);

    // Screen capture: the ScreenCast portal's restore token
    QString screenCastToken() const;
    void setScreenCastToken(const QString &token);

    // Live zoom
    double liveZoomFactor() const;
    void setLiveZoomFactor(double factor);

    // General
    bool startAtLogin() const;
    void setStartAtLogin(bool on);
    bool showTrayIcon() const;
    void setShowTrayIcon(bool on);
    bool firstRun() const;
    void setFirstRunDone();

    void sync() { m_store.sync(); }

Q_SIGNALS:
    void shortcutsChanged();
    void changed();

private:
    QVariant get(const QString &key, const QVariant &fallback) const;
    void put(const QString &key, const QVariant &value);

    QSettings m_store;
};
