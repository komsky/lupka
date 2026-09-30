#pragma once

#include "script.h"

#include <QDateTime>
#include <QObject>
#include <QTimer>

class Settings;
class Typist;

// ZoomIt's DemoType (Ctrl+7): each press types the next snippet of a prepared
// script into the focused window, so a live demo needs no live typing.
class DemoType : public QObject
{
    Q_OBJECT
public:
    explicit DemoType(Settings *settings, QObject *parent = nullptr);

    bool isTyping() const { return m_typing; }
    void typeNext();
    // Move back to the previous snippet (Ctrl+Shift+7).
    void stepBack();
    void stop();

Q_SIGNALS:
    void typingChanged(bool typing);
    void failed(const QString &reason);

private:
    bool reload();
    void begin();
    void runStep();
    void paste(const QString &text);
    void done();
    int delayMs() const;

    Settings *m_settings;
    Typist *m_typist = nullptr;
    QList<Snippet> m_snippets;
    QString m_loadedSource;
    int m_next = 0;
    Snippet m_current;
    int m_step = 0;
    bool m_typing = false;
    int m_modifierWaits = 0;
    QTimer m_timer;
};
