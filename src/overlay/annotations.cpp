#include "annotations.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <vector>

namespace {

QRectF shapeRect(const Shape &shape)
{
    if (shape.points.size() < 2)
        return {};
    return QRectF(shape.points.first(), shape.points.last()).normalized();
}

QPainterPath freehandPath(const QList<QPointF> &points)
{
    QPainterPath path;
    if (points.isEmpty())
        return path;
    path.moveTo(points.first());
    if (points.size() == 2) {
        path.lineTo(points.last());
        return path;
    }
    // Quadratic curves through the midpoints smooth out mouse jitter.
    for (int i = 1; i + 1 < points.size(); ++i) {
        const QPointF mid = (points.at(i) + points.at(i + 1)) / 2.0;
        path.quadTo(points.at(i), mid);
    }
    path.lineTo(points.last());
    return path;
}

// ZoomIt's arrow: the head sits at the tip (where the drag started), 2.5 pen
// widths long and 3 wide, with its base pulled in to half length.
struct ArrowGeometry {
    QPointF tail, tip, left, right, notch;
};

ArrowGeometry arrowGeometry(const Shape &shape)
{
    ArrowGeometry g;
    g.tip = shape.points.first();
    g.tail = shape.points.last();
    const double length = shape.width * 2.5;
    const double half = shape.width * 1.5;
    QPointF d = g.tip - g.tail;
    const double len = std::hypot(d.x(), d.y());
    d = len > 0 ? d / len : QPointF(1, 0);
    const QPointF base = g.tip - d * length;
    const QPointF normal(-d.y(), d.x());
    g.left = base + normal * half;
    g.right = base - normal * half;
    g.notch = g.tip - d * (length / 2.0);
    return g;
}

QPainterPath arrowPath(const Shape &shape)
{
    const ArrowGeometry g = arrowGeometry(shape);
    QPainterPath path;
    path.moveTo(g.tail);
    path.lineTo(g.tip);
    path.lineTo(g.notch);
    path.lineTo(g.left);
    path.lineTo(g.tip);
    path.lineTo(g.right);
    path.lineTo(g.notch);
    return path;
}

// Where a highlighter or blur shape has its effect: the stroke of lines,
// the whole interior of rectangles and ellipses.
QPainterPath effectArea(const Shape &shape)
{
    QPainterPath path;
    switch (shape.kind) {
    case Shape::Rectangle:
        path.addRect(shapeRect(shape));
        return path;
    case Shape::Ellipse:
        path.addEllipse(shapeRect(shape));
        return path;
    case Shape::Line:
        path.moveTo(shape.points.first());
        path.lineTo(shape.points.last());
        break;
    default:
        path = freehandPath(shape.points);
        break;
    }
    QPainterPathStroker stroker;
    stroker.setWidth(qMax(1.0, shape.width));
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    QPainterPath area = stroker.createStroke(path);
    if (shape.points.size() == 1) {
        area = QPainterPath();
        area.addEllipse(shape.points.first(), shape.width / 2.0, shape.width / 2.0);
    }
    return area;
}

// Three box-blur passes approximate a Gaussian of the given radius.
void boxBlur(QImage &image, int radius)
{
    const int r = qMax(1, radius / 3);
    const int w = image.width();
    const int h = image.height();
    if (w == 0 || h == 0)
        return;
    std::vector<quint32> line(size_t(qMax(w, h)));
    auto pass = [&](bool horizontal) {
        const int outer = horizontal ? h : w;
        const int inner = horizontal ? w : h;
        for (int o = 0; o < outer; ++o) {
            auto at = [&](int i) -> quint32 & {
                return horizontal ? reinterpret_cast<quint32 *>(image.scanLine(o))[i]
                                  : reinterpret_cast<quint32 *>(image.scanLine(i))[o];
            };
            for (int i = 0; i < inner; ++i)
                line[size_t(i)] = at(i);
            quint32 sum[4] = {0, 0, 0, 0};
            auto add = [&](quint32 px, int sign) {
                for (int c = 0; c < 4; ++c)
                    sum[c] += quint32(sign) * ((px >> (c * 8)) & 0xff);
            };
            const int window = 2 * r + 1;
            for (int i = -r; i <= r; ++i)
                add(line[size_t(qBound(0, i, inner - 1))], 1);
            for (int i = 0; i < inner; ++i) {
                quint32 out = 0;
                for (int c = 0; c < 4; ++c)
                    out |= ((sum[c] / quint32(window)) & 0xff) << (c * 8);
                at(i) = out;
                add(line[size_t(qBound(0, i - r, inner - 1))], -1);
                add(line[size_t(qBound(0, i + r + 1, inner - 1))], 1);
            }
        }
    };
    for (int n = 0; n < 3; ++n) {
        pass(true);
        pass(false);
    }
}

quint8 lighten(int c)
{
    return c ? quint8(qMin(255, c + 0x40)) : quint8(0x80);
}

// Apply a highlighter or blur shape to a copy of the pixels beneath it.
// Returns the changed patch and where it goes (in `under`'s pixels).
QImage applyPixelEffect(const Shape &shape, const QImage &under, QRect *where)
{
    const double dpr = under.devicePixelRatio();
    const QPainterPath area = effectArea(shape);
    const int pad = shape.blurRadius > 0 ? qCeil(shape.blurRadius * dpr) : 0;
    const QRect px = Canvas::toPixels(under, area.boundingRect().adjusted(-1, -1, 1, 1));
    if (px.isEmpty())
        return {};
    *where = px;

    // Coverage mask of the shape at pixel resolution (antialiased edges).
    QImage mask(px.size(), QImage::Format_Alpha8);
    mask.fill(0);
    {
        QPainter p(&mask);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(-px.topLeft());
        p.scale(dpr, dpr);
        p.fillPath(area, Qt::black);
    }

    QImage source = under.copy(px).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage effect;
    if (shape.blurRadius > 0) {
        // Blur a larger region so the edges of the shape blend in naturally.
        const QRect wide = px.adjusted(-pad, -pad, pad, pad).intersected(under.rect());
        QImage big = under.copy(wide).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        boxBlur(big, qMax(1, int(shape.blurRadius * dpr)));
        effect = big.copy(px.translated(-wide.topLeft()));
    } else {
        effect = source;
        const quint8 hr = lighten(shape.color.red());
        const quint8 hg = lighten(shape.color.green());
        const quint8 hb = lighten(shape.color.blue());
        for (int y = 0; y < effect.height(); ++y) {
            auto *row = reinterpret_cast<QRgb *>(effect.scanLine(y));
            for (int x = 0; x < effect.width(); ++x) {
                const QRgb p = row[x];
                row[x] = qRgba(qRed(p) & hr, qGreen(p) & hg, qBlue(p) & hb, qAlpha(p));
            }
        }
    }

    // Mix the effect in by coverage.
    for (int y = 0; y < source.height(); ++y) {
        auto *out = reinterpret_cast<QRgb *>(source.scanLine(y));
        const auto *fx = reinterpret_cast<const QRgb *>(effect.constScanLine(y));
        const uchar *m = mask.constScanLine(y);
        for (int x = 0; x < source.width(); ++x) {
            const int a = m[x];
            if (a == 0)
                continue;
            if (a == 255) {
                out[x] = fx[x];
                continue;
            }
            const QRgb s = out[x];
            const QRgb e = fx[x];
            auto mix = [a](int from, int to) { return from + (to - from) * a / 255; };
            out[x] = qRgba(mix(qRed(s), qRed(e)), mix(qGreen(s), qGreen(e)), mix(qBlue(s), qBlue(e)),
                           mix(qAlpha(s), qAlpha(e)));
        }
    }
    source.setDevicePixelRatio(dpr);
    return source;
}

}  // namespace

void snapLine(Shape &shape)
{
    if ((shape.kind != Shape::Line && shape.kind != Shape::Arrow) || shape.points.size() < 2)
        return;
    const QPointF a = shape.points.first();
    QPointF &b = shape.points.last();
    const double dx = std::abs(b.x() - a.x());
    const double dy = std::abs(b.y() - a.y());
    if (dy < dx / 10.0)
        b.setY(a.y());
    else if (dx < dy / 10.0)
        b.setX(a.x());
}

QRectF Shape::boundingRect() const
{
    const double pad = width + 2;
    switch (kind) {
    case Text: {
        if (points.isEmpty())
            return {};
        const QFontMetricsF fm(font);
        double w = 0;
        const QStringList lines = text.split(QLatin1Char('\n'));
        for (const QString &line : lines)
            w = qMax(w, fm.horizontalAdvance(line));
        const double h = fm.lineSpacing() * lines.size();
        const QPointF a = points.first();
        const double x = alignRight ? a.x() - w : a.x();
        return QRectF(x, a.y(), w, h).adjusted(-pad, -pad, pad, pad);
    }
    case Arrow: {
        if (points.size() < 2)
            return {};
        return arrowPath(*this).boundingRect().adjusted(-pad, -pad, pad, pad);
    }
    default: {
        if (points.isEmpty())
            return {};
        QRectF r(points.first(), QSizeF(0, 0));
        for (const QPointF &p : points)
            r |= QRectF(p, QSizeF(0, 0));
        return r.normalized().adjusted(-pad, -pad, pad, pad);
    }
    }
}

bool Shape::isEmpty() const
{
    switch (kind) {
    case Text:
        return text.trimmed().isEmpty();
    case Freehand:
        return points.isEmpty();
    default:
        return points.size() < 2 || points.first() == points.last();
    }
}

void Canvas::setBackground(const QImage &image)
{
    m_background = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    m_background.setDevicePixelRatio(image.devicePixelRatio());
    m_state = State();
    m_history.clear();
    invalidate();
}

QSizeF Canvas::logicalSize() const
{
    return QSizeF(m_background.size()) / m_background.devicePixelRatio();
}

void Canvas::pushHistory()
{
    m_history.append(m_state);
    if (m_history.size() > kMaxUndo)
        m_history.removeFirst();
}

void Canvas::setBoard(Board board)
{
    if (board == m_state.board && m_state.shapes.isEmpty())
        return;
    pushHistory();
    m_state.board = board;
    m_state.shapes.clear();
    invalidate();
}

void Canvas::add(const Shape &shape)
{
    if (shape.isEmpty())
        return;
    pushHistory();
    m_state.shapes.append(shape);
    // Paint just the new shape onto the cached image rather than redoing
    // every earlier one (blurs in particular are expensive).
    if (m_composedValid) {
        QPainter p(&m_composed);
        paintShape(p, shape, m_composed);
    }
}

void Canvas::clear()
{
    if (isPristine())
        return;
    pushHistory();
    m_state = State();
    invalidate();
}

bool Canvas::undo()
{
    if (m_history.isEmpty())
        return false;
    m_state = m_history.takeLast();
    invalidate();
    return true;
}

const QImage &Canvas::composed()
{
    if (m_composedValid)
        return m_composed;

    m_composed = QImage(m_background.size(), QImage::Format_ARGB32_Premultiplied);
    m_composed.setDevicePixelRatio(m_background.devicePixelRatio());
    switch (m_state.board) {
    case Board::Screen: {
        QPainter p(&m_composed);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(QPointF(0, 0), m_background);
        break;
    }
    case Board::Whiteboard:
        m_composed.fill(Qt::white);
        break;
    case Board::Blackboard:
        m_composed.fill(Qt::black);
        break;
    }
    for (const Shape &shape : std::as_const(m_state.shapes)) {
        QPainter p(&m_composed);
        paintShape(p, shape, m_composed);
    }
    m_composedValid = true;
    return m_composed;
}

QImage Canvas::render(const Shape *extra)
{
    QImage out = composed().copy();
    out.setDevicePixelRatio(m_composed.devicePixelRatio());
    if (extra && !extra->isEmpty()) {
        QPainter p(&out);
        paintShape(p, *extra, m_composed);
    }
    return out;
}

QRect Canvas::toPixels(const QImage &image, const QRectF &rect)
{
    const double dpr = image.devicePixelRatio();
    const QRectF px(rect.x() * dpr, rect.y() * dpr, rect.width() * dpr, rect.height() * dpr);
    return px.toAlignedRect().intersected(image.rect());
}

void Canvas::paintPreview(QPainter &painter, const Shape &shape)
{
    if (shape.points.isEmpty())
        return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const QPainterPath area = effectArea(shape);
    painter.fillPath(area, QColor(128, 128, 128, 110));
    QPen outline(QColor(255, 255, 255, 200), 0);
    outline.setStyle(Qt::DashLine);
    painter.strokePath(area, outline);
    painter.restore();
}

void Canvas::paintShape(QPainter &painter, const Shape &shape, const QImage &under)
{
    if (shape.needsPixels()) {
        if (shape.points.isEmpty() || under.isNull())
            return;
        QRect where;
        const QImage patch = applyPixelEffect(shape, under, &where);
        if (patch.isNull())
            return;
        const double dpr = under.devicePixelRatio();
        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(QPointF(where.x() / dpr, where.y() / dpr), patch);
        painter.restore();
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QColor color = shape.color;
    if (shape.blurRadius > 0)
        color = QColor(112, 112, 112);  // ZoomIt draws blur-pen arrows grey
    else if (shape.highlighter)
        color.setAlpha(0x80);
    QPen pen(color, shape.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (shape.kind) {
    case Shape::Freehand:
        if (shape.points.size() == 1) {
            // A click without movement leaves a dot.
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawEllipse(shape.points.first(), shape.width / 2.0, shape.width / 2.0);
        } else {
            painter.drawPath(freehandPath(shape.points));
        }
        break;
    case Shape::Line:
        if (shape.points.size() >= 2)
            painter.drawLine(shape.points.first(), shape.points.last());
        break;
    case Shape::Rectangle:
        if (shape.points.size() >= 2) {
            pen.setJoinStyle(Qt::MiterJoin);
            painter.setPen(pen);
            painter.drawRect(shapeRect(shape));
        }
        break;
    case Shape::Ellipse:
        if (shape.points.size() >= 2)
            painter.drawEllipse(shapeRect(shape));
        break;
    case Shape::Arrow:
        if (shape.points.size() >= 2) {
            const ArrowGeometry g = arrowGeometry(shape);
            painter.setBrush(color);
            painter.drawPolygon(QPolygonF({g.tip, g.left, g.notch, g.right}));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(arrowPath(shape));
        }
        break;
    case Shape::Text: {
        if (shape.points.isEmpty())
            break;
        QColor textColor = shape.color;
        textColor.setAlpha(255);
        painter.setFont(shape.font);
        painter.setPen(textColor);
        const QFontMetricsF fm(shape.font);
        const QPointF anchor = shape.points.first();
        const QStringList lines = shape.text.split(QLatin1Char('\n'));
        double y = anchor.y() + fm.ascent();
        for (const QString &line : lines) {
            const double x = shape.alignRight ? anchor.x() - fm.horizontalAdvance(line) : anchor.x();
            painter.drawText(QPointF(x, y), line);
            y += fm.lineSpacing();
        }
        break;
    }
    }
    painter.restore();
}
