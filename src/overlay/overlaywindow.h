// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "actions.h"
#include "annotations.h"

#include <QImage>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

class Settings;

// A frameless fullscreen window over one monitor showing a frozen screenshot,
// with ZoomIt's behaviour: zoom and pan, pen mode for drawing (on the 1:1
// screenshot, so drawings survive zooming), typing, and region selection for
// copy, save and snip.
class OverlayWindow : public QWidget
{
    Q_OBJECT
public:
    enum class Kind { Zoom, Draw, Snip, SnipSave, LiveDraw, RecordRegion };
    enum class CropPurpose { Copy, Save, Record };

    OverlayWindow(QScreen *screen, const QImage &shot, Settings *settings, Kind kind);
    ~OverlayWindow() override;

    Kind kind() const { return m_kind; }
    QScreen *targetScreen() const { return m_screen; }

    void showOnScreen();
    // Start the session's behaviour with the pointer at `cursor` (window coordinates).
    void begin(const QPointF &cursor);
    // Zoom back out if zoomed, then emit finished().
    void leave();
    // Pick a region of what is shown and copy or save it (snip hotkeys while zoomed).
    void startCrop(CropPurpose purpose);

    // Public so the session can forward keys that reached another monitor's overlay.
    void handleKeyPress(QKeyEvent *event);
    void handleKeyRelease(QKeyEvent *event);
    void setKeyTarget(OverlayWindow *target) { m_keyTarget = target; }

Q_SIGNALS:
    void pointerSeen(OverlayWindow *window, const QPointF &pos);
    void pointerEntered(OverlayWindow *window);
    void finished();
    void hotkeyPressed(Action action);
    // `snip` is true for a region picked by a snip hotkey (worth a notification).
    void copyRequested(const QImage &image, bool snip);
    void saved(const QString &path);
    // A region to record, in global logical coordinates.
    void regionSelected(QScreen *screen, const QRect &region);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool focusNextPrevChild(bool) override { return false; }

private:
    void notePointer(const QPointF &pos);

    // View and zoom
    QPointF toSource(const QPointF &widget) const;
    void updateView();
    void setZoomTarget(double target, bool firstStepImmediate);
    void stepZoom(int notches);
    void freezeView();

    // Pen mode, shapes, typing
    void enterPenMode();
    void leavePenMode();
    Shape::Kind kindForModifiers(Qt::KeyboardModifiers mods) const;
    void beginShape(const QPointF &source, Qt::KeyboardModifiers mods);
    void commitShape();
    void setPenColor(const QColor &color, bool highlighter);
    void setBlur(int radius);
    void changePenWidth(int delta);
    void startTyping(bool alignRight);
    void endTyping();
    void updateTextFont();
    void updateCursor();

    // Region selection
    void startCrop(CropPurpose purpose, bool exitAfter);
    void finishCrop(const QRectF &area);
    void cancelCrop();
    void paintCrop(QPainter &painter) const;

    // Output
    QImage renderView(const QRectF &area) const;
    QImage renderActual(const QRectF &area);
    void copyArea(const QRectF &area, bool snip);
    void saveArea(const QRectF &area);
    void writeImage(const QString &path, const QImage &image);

    std::optional<Action> hotkeyFor(QKeyEvent *event) const;
    void showHint(const QString &text);

    Kind m_kind;
    QPointer<OverlayWindow> m_keyTarget;
    QPointer<QScreen> m_screen;
    Settings *m_settings;
    Canvas m_canvas;
    bool m_started = false;
    bool m_pointerKnown = false;
    bool m_leaving = false;
    bool m_modalOpen = false;       // the save dialog is running its event loop
    bool m_leaveRequested = false;  // leave() was called meanwhile
    QPointF m_cursor;

    // Zoom: the view is the part of the screenshot (in screen coordinates)
    // stretched over the window. It follows the pointer, except while
    // drawing, typing or selecting, when it stays put.
    double m_zoom = 1.0;
    double m_zoomFrom = 1.0;
    double m_zoomTo = 1.0;
    int m_wheelRemainder = 0;
    QVariantAnimation m_zoomAnimation;
    QRectF m_view;
    bool m_viewFrozen = false;
    QPointF m_anchorSource;
    QPointF m_anchorWidget;

    // Pen
    bool m_penMode = false;
    bool m_drawing = false;
    Shape m_current;
    QColor m_penColor;
    int m_penWidth = 5;
    bool m_highlighter = false;
    int m_blurRadius = 0;
    bool m_tabDown = false;

    // Typing
    bool m_typing = false;
    bool m_textPlaced = false;
    Shape m_text;
    int m_fontScale = 10;
    bool m_caretVisible = true;
    QTimer m_caretTimer;

    // Region selection
    bool m_cropping = false;
    CropPurpose m_cropPurpose = CropPurpose::Copy;
    bool m_exitAfterCrop = false;
    bool m_selecting = false;
    QPointF m_selectStart;
    QPointF m_selectEnd;

    QString m_hint;
    QTimer m_hintTimer;
};
