#pragma once

#include "screencapture.h"

#include <QDBusPendingCallWatcher>
#include <QFutureWatcher>
#include <QTimer>

// X11: read the root window. Instant and silent.
class X11Capture : public CaptureBackend
{
    Q_OBJECT
public:
    using CaptureBackend::CaptureBackend;
    QString name() const override { return QStringLiteral("X11"); }
    void capture(const QList<QScreen *> &screens) override;
};

// KDE Plasma (Wayland): org.kde.KWin.ScreenShot2, one monitor at a time,
// raw pixels through a pipe. Needs X-KDE-DBUS-Restricted-Interfaces in our
// .desktop file, otherwise KWin answers NoAuthorized.
class KWinCapture : public CaptureBackend
{
    Q_OBJECT
public:
    using CaptureBackend::CaptureBackend;
    QString name() const override { return QStringLiteral("KWin ScreenShot2"); }
    void capture(const QList<QScreen *> &screens) override;

private:
    struct Job {
        QPointer<QScreen> screen;
        QByteArray data;
        QVariantMap meta;
        bool dataDone = false;
        bool metaDone = false;
    };
    void startJob(int index);
    void jobUpdated();
    void fail(const QString &reason);

    QList<Job> m_jobs;
    bool m_failed = false;
    int m_generation = 0;
};

// xdg-desktop-portal Screenshot, non-interactive. Works on GNOME, KDE and most
// wlroots desktops; returns one image of the whole desktop.
class PortalCapture : public CaptureBackend
{
    Q_OBJECT
public:
    explicit PortalCapture(QObject *parent = nullptr);
    QString name() const override { return QStringLiteral("xdg-desktop-portal"); }
    void capture(const QList<QScreen *> &screens) override;

private Q_SLOTS:
    void onResponse(uint response, const QVariantMap &results);

private:
    void watchRequest(const QString &path);
    void unwatch();
    void fail(const QString &reason);

    QList<QScreen *> m_screens;
    QString m_requestPath;
    QTimer m_timeout;
    bool m_active = false;
};

class ScreenCastSession;
class Settings;

// GNOME under Wayland: one frame from a ScreenCast portal session. Unlike the
// Screenshot portal it neither flashes the screen nor plays a shutter sound.
// The first time, the desktop asks which screens to share; the answer is
// remembered through a restore token.
class ScreenCastCapture : public CaptureBackend
{
    Q_OBJECT
public:
    ScreenCastCapture(Settings *settings, QObject *parent = nullptr);
    QString name() const override { return QStringLiteral("ScreenCast portal"); }
    void capture(const QList<QScreen *> &screens) override;
    static bool isUsable();

private:
    void onStarted();

    Settings *m_settings;
    ScreenCastSession *m_session = nullptr;
    QList<QScreen *> m_screens;
};

// wlroots compositors (sway, Hyprland, ...): the grim command.
class GrimCapture : public CaptureBackend
{
    Q_OBJECT
public:
    using CaptureBackend::CaptureBackend;
    QString name() const override { return QStringLiteral("grim"); }
    void capture(const QList<QScreen *> &screens) override;

private:
    QFutureWatcher<ScreenImages> m_watcher;
};
