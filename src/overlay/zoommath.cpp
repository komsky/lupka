#include "zoommath.h"

#include <QtGlobal>

#include <cmath>

namespace zoommath {
namespace {

double clampOrigin(double origin, double viewSize, double screenSize)
{
    return qBound(0.0, origin, qMax(0.0, screenSize - viewSize));
}

// ZoomIt's LIVEZOOM_MOVE_REGIONS: keep the pointer out of the outer eighth.
double withMargin(double origin, double cursor, double viewSize, double screenSize)
{
    const double margin = viewSize / 8.0;
    if (cursor - origin < margin)
        origin = qMax(0.0, cursor - margin);
    else if (origin + viewSize - cursor < margin)
        origin = qMin(cursor + margin - viewSize, screenSize - viewSize);
    return origin;
}

}  // namespace

QRectF viewport(const QSizeF &screen, const QPointF &cursor, double zoom)
{
    zoom = qBound(kMinZoom, zoom, kMaxZoom);
    const double w = screen.width() / zoom;
    const double h = screen.height() / zoom;
    const double cx = qBound(0.0, cursor.x(), screen.width());
    const double cy = qBound(0.0, cursor.y(), screen.height());
    double x = clampOrigin(cx - cx / zoom, w, screen.width());
    double y = clampOrigin(cy - cy / zoom, h, screen.height());
    x = withMargin(x, cx, w, screen.width());
    y = withMargin(y, cy, h, screen.height());
    return QRectF(x, y, w, h);
}

QRectF anchoredViewport(const QSizeF &screen, const QPointF &source, const QPointF &widget, double zoom)
{
    zoom = qBound(kMinZoom, zoom, kMaxZoom);
    const double w = screen.width() / zoom;
    const double h = screen.height() / zoom;
    const double x = clampOrigin(source.x() - widget.x() / zoom, w, screen.width());
    const double y = clampOrigin(source.y() - widget.y() / zoom, h, screen.height());
    return QRectF(x, y, w, h);
}

QPointF toSource(const QRectF &viewport, const QSizeF &screen, const QPointF &p)
{
    if (screen.isEmpty())
        return p;
    return QPointF(viewport.x() + p.x() * viewport.width() / screen.width(),
                   viewport.y() + p.y() * viewport.height() / screen.height());
}

double zoomIn(double zoom)
{
    if (zoom >= kMaxZoom)
        return kMaxZoom;
    return qMin(kMaxZoom, zoom < 2.0 ? 2.0 : zoom * 2.0);
}

double zoomOut(double zoom)
{
    if (zoom <= kMinZoom)
        return kMinZoom;
    return qMax(kMinZoom, zoom <= 2.0 ? zoom * 0.75 : zoom / 2.0);
}

int animationMs(double from, double to, bool firstStepImmediate)
{
    // ZoomIt's original animation stepped x1.1 in or x0.8 out every 40 ms;
    // the smooth version takes 1.5 times as long as that did.
    int steps = 0;
    double z = from;
    if (to > from) {
        while (z < to && steps < 200) {
            z *= 1.1;
            ++steps;
        }
    } else {
        while (z > to && steps < 200) {
            z *= 0.8;
            ++steps;
        }
    }
    if (firstStepImmediate && steps > 0)
        --steps;
    return steps * 40 * 3 / 2;
}

double interpolate(double from, double to, double t)
{
    t = qBound(0.0, t, 1.0);
    const double eased = t * t * (3.0 - 2.0 * t);
    const double a = std::log(qMax(from, 1e-6));
    const double b = std::log(qMax(to, 1e-6));
    return std::exp(a + (b - a) * eased);
}

}  // namespace zoommath
