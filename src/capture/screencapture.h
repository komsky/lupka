// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QScreen>

class Settings;

// A frozen picture of one monitor. The image carries its device pixel ratio,
// so painting it into the screen's logical geometry is pixel exact.
struct ScreenImage {
    QPointer<QScreen> screen;
    QImage image;
};
using ScreenImages = QList<ScreenImage>;

// One way of grabbing the screen. Implementations are asynchronous: they emit
// exactly one of finished() or failed() per capture() call.
class CaptureBackend : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QString name() const = 0;
    virtual void capture(const QList<QScreen *> &screens) = 0;

Q_SIGNALS:
    void finished(const ScreenImages &images);
    void failed(const QString &reason);
};

// Tries the backends that make sense for this session, in order, and sticks
// with the first one that works.
class ScreenCapture : public QObject
{
    Q_OBJECT
public:
    explicit ScreenCapture(Settings *settings, QObject *parent = nullptr);

    // False if a capture is already running.
    bool capture();
    bool isBusy() const { return m_busy; }
    QString backendName() const;

Q_SIGNALS:
    void finished(const ScreenImages &images);
    void failed(const QString &reason);

private:
    void tryBackend(int index);

    QList<CaptureBackend *> m_backends;
    int m_current = -1;
    bool m_busy = false;
    QStringList m_errors;
};

// Cut an image of the whole desktop (all monitors, as returned by the portal)
// into one image per screen.
ScreenImages splitDesktopImage(const QImage &desktop, const QList<QScreen *> &screens);
