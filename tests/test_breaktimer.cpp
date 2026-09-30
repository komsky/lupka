#include "breaktimer.h"

#include <QTest>

class TestBreakTimer : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void minutesSnapToWholeMinutesFirst()
    {
        const qint64 m = 60000;
        QCOMPARE(BreakTimer::adjustedMinutes(10 * m, 1), 11 * m);
        QCOMPARE(BreakTimer::adjustedMinutes(10 * m, -1), 9 * m);
        QCOMPARE(BreakTimer::adjustedMinutes(9 * m + 30000, 1), 10 * m);
        QCOMPARE(BreakTimer::adjustedMinutes(9 * m + 30000, -1), 9 * m);
        QCOMPARE(BreakTimer::adjustedMinutes(9 * m + 30000, 2), 11 * m);
        QCOMPARE(BreakTimer::adjustedMinutes(30000, -1), 0);
        QCOMPARE(BreakTimer::adjustedMinutes(0, -1), 0);
    }

    void expiredTimerRestartsFromZero()
    {
        QCOMPARE(BreakTimer::adjustedMinutes(-90000, 1), 60000);
    }

    void secondsMoveInTens()
    {
        QCOMPARE(BreakTimer::adjustedSeconds(60000, 1), 70000);
        QCOMPARE(BreakTimer::adjustedSeconds(60000, -1), 50000);
        QCOMPARE(BreakTimer::adjustedSeconds(64000, 1), 70000);
        QCOMPARE(BreakTimer::adjustedSeconds(64000, -1), 60000);
        QCOMPARE(BreakTimer::adjustedSeconds(4000, -1), 0);
    }
};

QTEST_APPLESS_MAIN(TestBreakTimer)
#include "test_breaktimer.moc"
