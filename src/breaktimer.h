// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QDateTime>
#include <QImage>
#include <QTimer>
#include <QWidget>

class Settings;

// ZoomIt's break timer (Ctrl+3): a big countdown on one monitor. It is an
// ordinary fullscreen window, so the presenter can switch away and back.
class BreakTimer : public QWidget
{
    Q_OBJECT
public:
    explicit BreakTimer(Settings *settings);

    // `desktop` is a screenshot for the "faded desktop" background, or null.
    void start(QScreen *screen, const QImage &desktop);
    void stop();
    void bringToFront();
    bool isRunning() const { return m_running; }

    // Remaining time in ms after the arrow-key/wheel rules, exposed for tests.
    static qint64 adjustedMinutes(qint64 remainingMs, int minutes);
    static qint64 adjustedSeconds(qint64 remainingMs, int tens);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void setRemaining(qint64 ms);
    qint64 remainingMs() const;
    void tick();

    Settings *m_settings;
    QDateTime m_end;
    QTimer m_tick;
    QImage m_background;
    QImage m_image;  // the background image file, decoded once per break
    int m_wheelRemainder = 0;
    bool m_running = false;
    bool m_alarmPlayed = false;
};
