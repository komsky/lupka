// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class Settings;

// Sends synthetic key presses to whatever window has focus.
class Typist : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    // Get ready to type (the portal may ask the user once); emits ready() or failed().
    virtual void prepare() = 0;
    // Type one character. False if the keyboard layout cannot produce it.
    virtual bool typeText(const QString &character) = 0;
    virtual void pressKey(quint32 keysym, bool withControl = false) = 0;
    // Nothing more will be typed for a while.
    virtual void finish() = 0;
    // Whether the modifiers of the hotkey that started us are still held.
    virtual bool modifiersHeld() const { return false; }

    static Typist *create(Settings *settings, QObject *parent);
    // X11 keysym for a character, as XKB and the portals expect it.
    static quint32 keysymFor(const QString &character);

Q_SIGNALS:
    void ready();
    void failed(const QString &reason);
};
