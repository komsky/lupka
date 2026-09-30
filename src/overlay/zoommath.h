#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>

// Zoom geometry and timing, following ZoomIt's source so the feel matches.
namespace zoommath {

constexpr double kMinZoom = 1.0;
constexpr double kMaxZoom = 256.0;

// The part of the screen (in screen coordinates) shown when it is magnified
// by `zoom` with the pointer at `cursor`. The view slides linearly with the
// pointer, so every edge is reachable, and keeps the pointer at least 1/8 of
// the view away from the view's edges.
QRectF viewport(const QSizeF &screen, const QPointF &cursor, double zoom);

// A view of size screen/zoom with its origin clamped inside the screen, such
// that `source` appears at widget position `widget`.
QRectF anchoredViewport(const QSizeF &screen, const QPointF &source, const QPointF &widget, double zoom);

// Screen position shown at widget position `p` for the given viewport.
QPointF toSource(const QRectF &viewport, const QSizeF &screen, const QPointF &p);

// One wheel notch or arrow key: x2 in (straight to 2 from below 2); out
// halves above 2 and takes 3/4 at or below 2, never below 1.
double zoomIn(double zoom);
double zoomOut(double zoom);

// Animation length in ms for going from one zoom to another, as ZoomIt
// computes it (1x to 2x takes 420 ms, leaving 2x takes 240 ms).
int animationMs(double from, double to, bool firstStepImmediate);

// Zoom at progress t (0..1) of an animation, interpolated in log space with
// an ease-in-out curve.
double interpolate(double from, double to, double t);

}  // namespace zoommath
