#include "capture/screencapture.h"

#include <QGuiApplication>
#include <QTest>

class TestCapture : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void splitsDesktopPerScreen()
    {
        const QList<QScreen *> screens = QGuiApplication::screens();
        QVERIFY(!screens.isEmpty());
        QRect virtualRect;
        for (QScreen *s : screens)
            virtualRect |= s->geometry();

        // A desktop image at twice the logical resolution, as HiDPI portals return.
        QImage desktop(virtualRect.size() * 2, QImage::Format_ARGB32_Premultiplied);
        desktop.fill(Qt::green);
        const ScreenImages parts = splitDesktopImage(desktop, screens);
        QCOMPARE(parts.size(), screens.size());
        for (const ScreenImage &part : parts) {
            QCOMPARE(part.image.size(), part.screen->geometry().size() * 2);
            QCOMPARE(part.image.devicePixelRatio(), 2.0);
        }
    }

    void emptyInputGivesNothing()
    {
        QVERIFY(splitDesktopImage(QImage(), QGuiApplication::screens()).isEmpty());
        QVERIFY(splitDesktopImage(QImage(10, 10, QImage::Format_RGB32), {}).isEmpty());
    }
};

QTEST_MAIN(TestCapture)
#include "test_capture.moc"
