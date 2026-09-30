#include "overlay/zoommath.h"

#include <QTest>

class TestZoomMath : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void noZoomShowsWholeScreen()
    {
        const QSizeF screen(1920, 1080);
        QCOMPARE(zoommath::viewport(screen, QPointF(500, 300), 1.0), QRectF(0, 0, 1920, 1080));
    }

    void viewportHasScreenSizeOverZoom()
    {
        const QRectF v = zoommath::viewport(QSizeF(1920, 1080), QPointF(960, 540), 4.0);
        QCOMPARE(v.size(), QSizeF(480, 270));
    }

    void centreStaysUnderCursor()
    {
        // Away from the edges the pixel under the pointer stays under it.
        const QSizeF screen(1920, 1080);
        for (double zoom : {1.5, 2.0, 4.0, 8.0}) {
            const QPointF cursor(960, 540);
            const QRectF v = zoommath::viewport(screen, cursor, zoom);
            const QPointF shown = zoommath::toSource(v, screen, cursor);
            QVERIFY(qAbs(shown.x() - cursor.x()) < 1e-6);
            QVERIFY(qAbs(shown.y() - cursor.y()) < 1e-6);
        }
    }

    void edgesAreReachable()
    {
        const QSizeF screen(1920, 1080);
        QCOMPARE(zoommath::viewport(screen, QPointF(0, 0), 4.0).topLeft(), QPointF(0, 0));
        QCOMPARE(zoommath::viewport(screen, QPointF(1920, 1080), 4.0).bottomRight(), QPointF(1920, 1080));
    }

    void pointerKeepsAMarginFromTheViewEdge()
    {
        // ZoomIt keeps the pointer out of the outer eighth of the view.
        const QSizeF screen(1920, 1080);
        const QRectF v = zoommath::viewport(screen, QPointF(100, 540), 2.0);
        QCOMPARE(v.x(), 0.0);
        const QRectF w = zoommath::viewport(screen, QPointF(1800, 540), 2.0);
        QCOMPARE(w.right(), 1920.0);
    }

    void cursorOutsideIsClamped()
    {
        const QSizeF screen(1920, 1080);
        const QRectF v = zoommath::viewport(screen, QPointF(-500, 5000), 2.0);
        QVERIFY(QRectF(QPointF(0, 0), screen).contains(v));
    }

    void anchoredViewportKeepsThePoint()
    {
        const QSizeF screen(1920, 1080);
        const QRectF v = zoommath::anchoredViewport(screen, QPointF(700, 400), QPointF(800, 500), 4.0);
        const QPointF shown = zoommath::toSource(v, screen, QPointF(800, 500));
        QVERIFY(qAbs(shown.x() - 700) < 1e-6);
        QVERIFY(qAbs(shown.y() - 400) < 1e-6);
        // Clamped when the anchor would push the view off screen.
        const QRectF c = zoommath::anchoredViewport(screen, QPointF(5, 5), QPointF(1900, 1000), 2.0);
        QCOMPARE(c.topLeft(), QPointF(0, 0));
    }

    void zoomItSteps()
    {
        QCOMPARE(zoommath::zoomIn(1.0), 2.0);
        QCOMPARE(zoommath::zoomIn(1.25), 2.0);
        QCOMPARE(zoommath::zoomIn(2.0), 4.0);
        QCOMPARE(zoommath::zoomIn(3.0), 6.0);
        QCOMPARE(zoommath::zoomIn(256.0), 256.0);
        QCOMPARE(zoommath::zoomOut(2.0), 1.5);
        QCOMPARE(zoommath::zoomOut(1.5), 1.125);
        QCOMPARE(zoommath::zoomOut(1.125), 1.0);
        QCOMPARE(zoommath::zoomOut(8.0), 4.0);
        QCOMPARE(zoommath::zoomOut(1.0), 1.0);
    }

    void animationTimingMatchesZoomIt()
    {
        QCOMPARE(zoommath::animationMs(1.0, 2.0, true), 420);
        QCOMPARE(zoommath::animationMs(2.0, 4.0, true), 420);
        QCOMPARE(zoommath::animationMs(2.0, 1.0, false), 240);
        QCOMPARE(zoommath::animationMs(2.0, 2.0, false), 0);
    }

    void interpolationHitsTheEnds()
    {
        QCOMPARE(zoommath::interpolate(1.0, 4.0, 0.0), 1.0);
        QVERIFY(qAbs(zoommath::interpolate(1.0, 4.0, 1.0) - 4.0) < 1e-9);
        // Log space: halfway between 1x and 4x is 2x.
        QVERIFY(qAbs(zoommath::interpolate(1.0, 4.0, 0.5) - 2.0) < 1e-9);
    }
};

QTEST_APPLESS_MAIN(TestZoomMath)
#include "test_zoommath.moc"
