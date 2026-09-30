// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hotkeys/keynames.h"

#include <QTest>

class TestKeyNames : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void gtkAccelerators_data()
    {
        QTest::addColumn<QString>("sequence");
        QTest::addColumn<QString>("accel");
        QTest::newRow("ctrl+1") << "Ctrl+1" << "<Primary>1";
        QTest::newRow("super+shift+s") << "Meta+Shift+S" << "<Super><Shift>s";
        QTest::newRow("ctrl+shift+6") << "Ctrl+Shift+6" << "<Primary><Shift>6";
        QTest::newRow("print") << "Print" << "Print";
        QTest::newRow("alt+f12") << "Alt+F12" << "<Alt>F12";
        QTest::newRow("ctrl+up") << "Ctrl+Up" << "<Primary>Up";
        QTest::newRow("ctrl+alt+space") << "Ctrl+Alt+Space" << "<Primary><Alt>space";
        QTest::newRow("ctrl+minus") << "Ctrl+-" << "<Primary>minus";
    }

    void gtkAccelerators()
    {
        QFETCH(QString, sequence);
        QFETCH(QString, accel);
        const QKeySequence seq = QKeySequence::fromString(sequence, QKeySequence::PortableText);
        QCOMPARE(keynames::toGtkAccelerator(seq), accel);
        QCOMPARE(keynames::fromGtkAccelerator(accel), seq[0]);
    }

    void acceptsControlSpelling()
    {
        QCOMPARE(keynames::fromGtkAccelerator("<Control><Alt>t"),
                 QKeyCombination(Qt::ControlModifier | Qt::AltModifier, Qt::Key_T));
    }

    void keysyms()
    {
        QCOMPARE(keynames::keysym(Qt::Key_1), 0x31u);
        QCOMPARE(keynames::keysym(Qt::Key_S), uint32_t('s'));
        QCOMPARE(keynames::keysym(Qt::Key_Print), 0xff61u);
        QCOMPARE(keynames::keysym(Qt::Key_F1), 0xffbeu);
        QCOMPARE(keynames::keysym(Qt::Key_F12), 0xffc9u);
        QCOMPARE(keynames::keysym(Qt::Key_unknown), 0u);
    }

    void rejectsGarbage()
    {
        QCOMPARE(keynames::fromGtkAccelerator("<Hyper>x").key(), Qt::Key_unknown);
        QCOMPARE(keynames::fromGtkAccelerator("<Primary>").key(), Qt::Key_unknown);
        QVERIFY(keynames::toGtkAccelerator(QKeySequence()).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestKeyNames)
#include "test_keynames.moc"
