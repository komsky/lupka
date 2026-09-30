#include "dbusservice.h"

#include "app.h"

DBusService::DBusService(App *app)
    : QObject(app)
    , m_app(app)
{
}

void DBusService::Trigger(const QString &action)
{
    // Return to the caller first; the action may open windows.
    QMetaObject::invokeMethod(m_app, [app = m_app, action] { app->triggerById(action); }, Qt::QueuedConnection);
}

QString DBusService::Version() const
{
    return QStringLiteral(APP_VERSION);
}
