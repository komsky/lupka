#pragma once

#include "actions.h"
#include "capture/screencapture.h"
#include "overlaywindow.h"

#include <QImage>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>

class Settings;

// One zoom, draw, snip or live-draw, from the hotkey until the overlays
// close. Owns an overlay per monitor and decides which one the user is on.
class Session : public QObject
{
    Q_OBJECT
public:
    using Kind = OverlayWindow::Kind;

    Session(Kind kind, const ScreenImages &images, Settings *settings, QObject *parent = nullptr);
    ~Session() override;

    Kind kind() const { return m_kind; }
    void start();
    // The hotkey for this session was pressed again: finish gracefully.
    void leave();
    // What pressing `action` does while this session runs (see App::trigger).
    enum class Response { Ignore, Leave, CropCopy, CropSave };
    Response respondTo(Action action) const;
    void crop(OverlayWindow::CropPurpose purpose);

Q_SIGNALS:
    void finished();
    void hotkeyPressed(Action action);
    void copyRequested(const QImage &image, bool snip);
    void saved(const QString &path);
    void regionSelected(QScreen *screen, const QRect &region);

private:
    void activate(OverlayWindow *window, const QPointF &pos);
    void finish();

    Kind m_kind;
    QList<QPointer<OverlayWindow>> m_windows;
    QPointer<OverlayWindow> m_active;
    QTimer m_pointerTimeout;
    bool m_finished = false;
};
