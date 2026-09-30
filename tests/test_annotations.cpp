// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "overlay/annotations.h"

#include <QPainter>
#include <QTest>

namespace {

QImage checker(int w, int h)
{
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            image.setPixel(x, y, ((x / 4 + y / 4) % 2) ? qRgb(255, 255, 255) : qRgb(0, 0, 0));
    return image;
}

Shape line(QPointF a, QPointF b, QColor color = Qt::red)
{
    Shape s;
    s.kind = Shape::Line;
    s.color = color;
    s.width = 6;
    s.points = {a, b};
    return s;
}

}  // namespace

class TestAnnotations : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void addAndUndo()
    {
        Canvas canvas;
        QImage bg(100, 100, QImage::Format_ARGB32_Premultiplied);
        bg.fill(Qt::white);
        canvas.setBackground(bg);
        canvas.add(line({10, 50}, {90, 50}));
        QCOMPARE(canvas.shapes().size(), 1);
        QCOMPARE(QColor(canvas.composed().pixel(50, 50)), QColor(Qt::red));
        QVERIFY(canvas.undo());
        QCOMPARE(canvas.shapes().size(), 0);
        QCOMPARE(QColor(canvas.composed().pixel(50, 50)), QColor(Qt::white));
        QVERIFY(!canvas.undo());
    }

    void emptyShapesAreIgnored()
    {
        Canvas canvas;
        canvas.setBackground(QImage(10, 10, QImage::Format_ARGB32_Premultiplied));
        canvas.add(line({5, 5}, {5, 5}));
        Shape text;
        text.kind = Shape::Text;
        text.points = {{1, 1}};
        text.text = "   ";
        canvas.add(text);
        QVERIFY(canvas.shapes().isEmpty());
        QVERIFY(!canvas.canUndo());
    }

    void eraseIsUndoable()
    {
        Canvas canvas;
        QImage bg(50, 50, QImage::Format_ARGB32_Premultiplied);
        bg.fill(Qt::white);
        canvas.setBackground(bg);
        canvas.add(line({0, 25}, {50, 25}));
        canvas.add(line({25, 0}, {25, 50}, Qt::blue));
        canvas.clear();
        QVERIFY(canvas.shapes().isEmpty());
        QVERIFY(canvas.undo());
        QCOMPARE(canvas.shapes().size(), 2);
    }

    void boardsBlankTheScreenAndAreUndoable()
    {
        Canvas canvas;
        canvas.setBackground(checker(40, 40));
        canvas.add(line({0, 20}, {40, 20}));
        canvas.setBoard(Board::Whiteboard);
        // Ctrl+W covers everything, drawings included.
        QCOMPARE(QColor(canvas.composed().pixel(2, 2)), QColor(Qt::white));
        QCOMPARE(QColor(canvas.composed().pixel(20, 20)), QColor(Qt::white));
        canvas.add(line({0, 30}, {40, 30}, Qt::blue));
        QCOMPARE(QColor(canvas.composed().pixel(20, 30)), QColor(Qt::blue));
        canvas.setBoard(Board::Blackboard);
        QCOMPARE(QColor(canvas.composed().pixel(2, 2)), QColor(Qt::black));
        QVERIFY(canvas.undo());
        QCOMPARE(canvas.board(), Board::Whiteboard);
        QVERIFY(canvas.undo());
        QVERIFY(canvas.undo());
        QCOMPARE(canvas.board(), Board::Screen);
        QCOMPARE(QColor(canvas.composed().pixel(20, 20)), QColor(Qt::red));
    }

    void highlighterIsAMarker()
    {
        // ZoomIt's highlighter ANDs the pixel with a lightened pen colour:
        // white takes the colour, black stays black.
        Canvas canvas;
        QImage bg(60, 60, QImage::Format_ARGB32_Premultiplied);
        bg.fill(Qt::white);
        for (int y = 0; y < 60; ++y)
            for (int x = 30; x < 60; ++x)
                bg.setPixel(x, y, qRgb(0, 0, 0));
        canvas.setBackground(bg);
        Shape s = line({0, 30}, {60, 30}, QColor(255, 255, 0));
        s.highlighter = true;
        s.width = 10;
        canvas.add(s);
        const QColor onWhite(canvas.composed().pixel(10, 30));
        QCOMPARE(onWhite, QColor(255, 255, 0x80));
        const QColor onBlack(canvas.composed().pixel(45, 30));
        QCOMPARE(onBlack, QColor(0, 0, 0));
        // Outside the band nothing changes.
        QCOMPARE(QColor(canvas.composed().pixel(10, 5)), QColor(Qt::white));
    }

    void highlightedRectangleIsFilled()
    {
        Canvas canvas;
        QImage bg(60, 60, QImage::Format_ARGB32_Premultiplied);
        bg.fill(Qt::white);
        canvas.setBackground(bg);
        Shape s = line({10, 10}, {50, 50}, QColor(0, 255, 0));
        s.kind = Shape::Rectangle;
        s.highlighter = true;
        canvas.add(s);
        QCOMPARE(QColor(canvas.composed().pixel(30, 30)), QColor(0x80, 255, 0x80));
    }

    void blurHidesDetail()
    {
        Canvas canvas;
        canvas.setBackground(checker(80, 80));
        Shape blur = line({0, 0}, {80, 80});
        blur.kind = Shape::Rectangle;
        blur.blurRadius = 20;
        canvas.add(blur);
        const QImage out = canvas.composed();
        int minGray = 255, maxGray = 0;
        for (int y = 20; y < 60; ++y)
            for (int x = 20; x < 60; ++x) {
                const int g = qGray(out.pixel(x, y));
                minGray = qMin(minGray, g);
                maxGray = qMax(maxGray, g);
            }
        // A 4px checkerboard blurred with radius 20 is close to uniform grey.
        QVERIFY2(maxGray - minGray < 40, qPrintable(QString("range %1..%2").arg(minGray).arg(maxGray)));
    }

    void blurPenOnlyTouchesTheStroke()
    {
        Canvas canvas;
        canvas.setBackground(checker(100, 100));
        Shape blur = line({0, 50}, {100, 50});
        blur.blurRadius = 20;
        blur.width = 10;
        canvas.add(blur);
        const QImage out = canvas.composed();
        const QImage original = checker(100, 100);
        QCOMPARE(out.pixel(10, 10), original.pixel(10, 10));
        QVERIFY(out.pixel(10, 50) != original.pixel(10, 50) || out.pixel(11, 50) != original.pixel(11, 50));
    }

    void arrowHeadIsAtTheStart()
    {
        Canvas canvas;
        QImage bg(200, 100, QImage::Format_ARGB32_Premultiplied);
        bg.fill(Qt::white);
        canvas.setBackground(bg);
        Shape arrow = line({20, 50}, {180, 50});
        arrow.kind = Shape::Arrow;
        arrow.width = 6;
        canvas.add(arrow);
        const QImage out = canvas.composed();
        // The head is 2.5 x 6 = 15 long and 2 x 1.5 x 6 = 18 wide at its base.
        QCOMPARE(QColor(out.pixel(32, 44)), QColor(Qt::red));
        QCOMPARE(QColor(out.pixel(168, 44)), QColor(Qt::white));
    }

    void linesSnapToAxes()
    {
        Shape s = line({0, 0}, {100, 7});
        snapLine(s);
        QCOMPARE(s.points.last(), QPointF(100, 0));
        Shape v = line({0, 0}, {5, 100});
        snapLine(v);
        QCOMPARE(v.points.last(), QPointF(0, 100));
        Shape d = line({0, 0}, {100, 50});
        snapLine(d);
        QCOMPARE(d.points.last(), QPointF(100, 50));
    }

    void undoDepthIsLimited()
    {
        Canvas canvas;
        canvas.setBackground(QImage(10, 10, QImage::Format_ARGB32_Premultiplied));
        for (int i = 0; i < Canvas::kMaxUndo + 10; ++i)
            canvas.add(line({0, 0}, {double(i + 1), 5}));
        int undos = 0;
        while (canvas.undo())
            ++undos;
        QCOMPARE(undos, Canvas::kMaxUndo);
    }

    void respectsDevicePixelRatio()
    {
        Canvas canvas;
        QImage bg(200, 200, QImage::Format_ARGB32_Premultiplied);
        bg.fill(Qt::white);
        bg.setDevicePixelRatio(2.0);
        canvas.setBackground(bg);
        QCOMPARE(canvas.logicalSize(), QSizeF(100, 100));
        canvas.add(line({0, 50}, {100, 50}));
        // Logical y=50 is physical row 100.
        QCOMPARE(QColor(canvas.composed().pixel(100, 100)), QColor(Qt::red));
        QCOMPARE(Canvas::toPixels(bg, QRectF(10, 10, 20, 20)), QRect(20, 20, 40, 40));
    }

    void arrowAndTextHaveBounds()
    {
        Shape arrow = line({10, 10}, {100, 10});
        arrow.kind = Shape::Arrow;
        QVERIFY(arrow.boundingRect().contains(QPointF(100, 10)));
        QVERIFY(arrow.boundingRect().contains(QPointF(10, 10)));
        Shape text;
        text.kind = Shape::Text;
        text.points = {{50, 50}};
        text.text = "Hello\nworld";
        text.font.setPixelSize(20);
        const QRectF r = text.boundingRect();
        QVERIFY(r.height() > 40);
        text.alignRight = true;
        QVERIFY(text.boundingRect().right() <= 50 + text.width + 3);
    }
};

QTEST_MAIN(TestAnnotations)
#include "test_annotations.moc"
