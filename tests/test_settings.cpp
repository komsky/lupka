// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include <QTemporaryDir>
#include <QTest>

class TestSettings : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultsMatchZoomIt()
    {
        QTemporaryDir dir;
        Settings s(dir.filePath("s.conf"), nullptr);
        QCOMPARE(s.shortcuts(Action::Zoom), QList<QKeySequence>{QKeySequence("Ctrl+1")});
        QCOMPARE(s.shortcuts(Action::Draw), QList<QKeySequence>{QKeySequence("Ctrl+2")});
        QCOMPARE(s.shortcuts(Action::Snip).size(), 2);
        QVERIFY(s.shortcuts(Action::Snip).contains(QKeySequence("Meta+Shift+S")));
        QCOMPARE(s.initialZoom(), 2.0);
    }

    void shortcutsRoundTrip()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("s.conf");
        {
            Settings s(file, nullptr);
            s.setShortcuts(Action::Zoom, {QKeySequence("Ctrl+Alt+Z"), QKeySequence("F9")});
            s.setShortcuts(Action::Break, {});
            s.sync();
        }
        Settings s(file, nullptr);
        QCOMPARE(s.shortcuts(Action::Zoom), (QList<QKeySequence>{QKeySequence("Ctrl+Alt+Z"), QKeySequence("F9")}));
        QVERIFY(s.shortcuts(Action::Break).isEmpty());
        s.resetShortcuts();
        QCOMPARE(s.shortcuts(Action::Break), QList<QKeySequence>{QKeySequence("Ctrl+3")});
    }

    void shortcutWithCommaSurvives()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("s.conf");
        {
            Settings s(file, nullptr);
            s.setShortcuts(Action::Draw, {QKeySequence("Ctrl+,")});
            s.sync();
        }
        Settings s(file, nullptr);
        QCOMPARE(s.shortcuts(Action::Draw), QList<QKeySequence>{QKeySequence("Ctrl+,")});
    }

    void valuesAreClamped()
    {
        QTemporaryDir dir;
        Settings s(dir.filePath("s.conf"), nullptr);
        s.setPenWidth(500);
        QCOMPARE(s.penWidth(), 40);
        s.setInitialZoom(0.1);
        QCOMPARE(s.initialZoom(), 1.25);
    }
};

QTEST_APPLESS_MAIN(TestSettings)
#include "test_settings.moc"
