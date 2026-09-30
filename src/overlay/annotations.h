#pragma once

#include <QColor>
#include <QFont>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>

class QPainter;

// One thing drawn on the screen, in screen (source) coordinates.
struct Shape {
    enum Kind { Freehand, Line, Rectangle, Ellipse, Arrow, Text };

    Kind kind = Freehand;
    QColor color = Qt::red;
    double width = 5;
    // Marker effect: pixels become (pixel AND lightened colour), like ZoomIt.
    bool highlighter = false;
    // Blur radius in pixels; 0 means a normal pen.
    int blurRadius = 0;
    // Freehand: every sampled point. Line, Rectangle, Ellipse: [start, end].
    // Arrow: [tip, tail] (the head goes where the drag started). Text: [anchor].
    QList<QPointF> points;
    QString text;
    QFont font;
    bool alignRight = false;

    QRectF boundingRect() const;
    bool isEmpty() const;
    // Pixel effects (highlighter, blur) read what lies beneath the shape.
    bool needsPixels() const { return (highlighter || blurRadius > 0) && kind != Arrow && kind != Text; }
};

enum class Board { Screen, Whiteboard, Blackboard };

// Make near-horizontal and near-vertical lines exact (ZoomIt's snap).
void snapLine(Shape &shape);

// The drawing surface: a background image plus a stack of shapes, with undo.
// Coordinates are logical pixels of the screen the background came from.
class Canvas
{
public:
    void setBackground(const QImage &image);
    const QImage &background() const { return m_background; }
    QSizeF logicalSize() const;

    Board board() const { return m_state.board; }
    // Blanking the screen covers everything drawn so far (undoable).
    void setBoard(Board board);

    const QList<Shape> &shapes() const { return m_state.shapes; }
    void add(const Shape &shape);
    // Back to the untouched screenshot (undoable).
    void clear();
    bool undo();
    bool canUndo() const { return !m_history.isEmpty(); }
    bool isPristine() const { return m_state.shapes.isEmpty() && m_state.board == Board::Screen; }

    // Background and committed shapes, cached until something changes.
    const QImage &composed();
    // Composed image with an extra, uncommitted shape on top.
    QImage render(const Shape *extra = nullptr);

    // Paint one shape. `under` is what lies beneath it, in the same
    // coordinate space, which highlighter and blur need.
    static void paintShape(QPainter &painter, const Shape &shape, const QImage &under);
    // A cheap stand-in for a blur shape while it is being dragged.
    static void paintPreview(QPainter &painter, const Shape &shape);
    // Pixel rectangle of `rect` inside `image`, respecting its device pixel ratio.
    static QRect toPixels(const QImage &image, const QRectF &rect);

    static constexpr int kMaxUndo = 32;

private:
    struct State {
        QList<Shape> shapes;
        Board board = Board::Screen;
    };
    void pushHistory();
    void invalidate() { m_composedValid = false; }

    QImage m_background;
    State m_state;
    QList<State> m_history;
    QImage m_composed;
    bool m_composedValid = false;
};
