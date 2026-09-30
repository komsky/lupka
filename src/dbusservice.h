#pragma once

#include "config.h"

#include <QObject>

class App;

// The daemon's D-Bus interface. "<app> zoom" and the desktop's hotkeys call
// Trigger("zoom") on the running instance.
class DBusService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", APP_ID)
public:
    explicit DBusService(App *app);

public Q_SLOTS:
    Q_SCRIPTABLE void Trigger(const QString &action);
    Q_SCRIPTABLE QString Version() const;

private:
    App *m_app;
};
