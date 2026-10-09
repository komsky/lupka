// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "platform.h"

#include <QTest>
#include <QVersionNumber>

class TestPlatform : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void wheelHotplugBug_data()
    {
        QTest::addColumn<QString>("version");
        QTest::addColumn<bool>("affected");
        QTest::newRow("6.2.4") << "6.2.4" << true;
        QTest::newRow("6.4.2 (Ubuntu 24.04)") << "6.4.2" << true;
        QTest::newRow("6.5") << "6.5" << true;
        QTest::newRow("6.5.0") << "6.5.0" << true;
        QTest::newRow("6.5.1 (fixed)") << "6.5.1" << false;
        QTest::newRow("6.6.0") << "6.6.0" << false;
        QTest::newRow("6.8.2") << "6.8.2" << false;
    }

    void wheelHotplugBug()
    {
        QFETCH(QString, version);
        QFETCH(bool, affected);
        QCOMPARE(platform::qtDropsWheelAfterHotplug(QVersionNumber::fromString(version)), affected);
    }

    void disablesXi2OnAffectedQt()
    {
        qunsetenv("QT_XCB_NO_XI2");
        platform::prepareEnvironment();
        const bool affected = platform::qtDropsWheelAfterHotplug(QVersionNumber::fromString(QLatin1String(qVersion())));
        QCOMPARE(qEnvironmentVariableIsSet("QT_XCB_NO_XI2"), affected);
        qunsetenv("QT_XCB_NO_XI2");
    }

    void keepsUserChoice()
    {
        qputenv("QT_XCB_NO_XI2", "user");
        platform::prepareEnvironment();
        QCOMPARE(qgetenv("QT_XCB_NO_XI2"), QByteArray("user"));
        qunsetenv("QT_XCB_NO_XI2");
    }
};

QTEST_GUILESS_MAIN(TestPlatform)
#include "test_platform.moc"
