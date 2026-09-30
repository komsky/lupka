#pragma once

#include "actions.h"

#include <QKeySequence>
#include <QMap>
#include <QObject>

class Settings;

using Bindings = QMap<Action, QList<QKeySequence>>;

// One mechanism for global hotkeys. Backends that receive key presses in this
// process emit activated(); the GNOME backend instead makes the desktop run
// "<app> <action>", which reaches us over D-Bus.
class HotkeyBackend : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QString name() const = 0;
    // Replace everything this backend registered with `bindings`.
    virtual void apply(const Bindings &bindings) = 0;
    // Remove every registration, e.g. when the user quits.
    virtual void unregisterAll() = 0;
    QString lastError() const { return m_error; }

Q_SIGNALS:
    void activated(Action action);

protected:
    void setError(const QString &error) { m_error = error; }

private:
    QString m_error;
};

// Picks a backend for this desktop and keeps it in sync with the settings.
class Hotkeys : public QObject
{
    Q_OBJECT
public:
    Hotkeys(Settings *settings, QObject *parent = nullptr);

    void apply();
    // Extra bindings that only exist for a while (live zoom in/out).
    void setTemporary(const Bindings &bindings);
    void unregisterAll();

    QString backendName() const;
    QString status() const;

Q_SIGNALS:
    void activated(Action action);

private:
    Settings *m_settings;
    HotkeyBackend *m_backend = nullptr;
    Bindings m_temporary;
};

// Command the desktop runs for an action, e.g. "'/usr/bin/app' zoom".
QString commandForAction(Action action);

HotkeyBackend *createGnomeHotkeys(QObject *parent);
HotkeyBackend *createKdeHotkeys(QObject *parent);
HotkeyBackend *createX11Hotkeys(QObject *parent);
HotkeyBackend *createPortalHotkeys(QObject *parent);
