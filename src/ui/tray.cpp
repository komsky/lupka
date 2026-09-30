#include "tray.h"

#include "app.h"
#include "config.h"
#include "settings.h"

#include <QMenu>
#include <QSystemTrayIcon>

Tray::Tray(App *app)
    : QObject(app)
    , m_app(app)
{
    m_menu = new QMenu;
    const QList<Action> entries = {Action::Zoom, Action::Draw, Action::Snip, Action::SnipSave, Action::Break,
                                   Action::LiveZoom};
    for (Action action : entries) {
        QAction *item = m_menu->addAction(actionInfo(action).label);
        connect(item, &QAction::triggered, app, [app, action] { app->trigger(action); });
        m_actions.insert(int(action), item);
    }
    m_actions.value(int(Action::LiveZoom))->setCheckable(true);
    m_menu->addSeparator();
    connect(m_menu->addAction(tr("Settings…")), &QAction::triggered, app, [app] { app->trigger(Action::Settings); });
    connect(m_menu->addAction(tr("Quit")), &QAction::triggered, app, [app] { app->trigger(Action::Quit); });

    m_icon = new QSystemTrayIcon(QIcon(QStringLiteral(":/icons/tray.png")), this);
    m_icon->setToolTip(QStringLiteral(APP_NAME));
    m_icon->setContextMenu(m_menu);
    connect(m_icon, &QSystemTrayIcon::activated, app, [app](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            app->trigger(Action::Settings);
    });
    connect(app, &App::liveZoomChanged, this, &Tray::refresh);
    refresh();
}

Tray::~Tray()
{
    delete m_menu;
}

void Tray::setVisible(bool visible)
{
    m_icon->setVisible(visible && QSystemTrayIcon::isSystemTrayAvailable());
}

void Tray::refresh()
{
    for (auto it = m_actions.constBegin(); it != m_actions.constEnd(); ++it) {
        const auto action = Action(it.key());
        QString text = actionInfo(action).label;
        const QList<QKeySequence> keys = m_app->settings()->shortcuts(action);
        if (!keys.isEmpty())
            text += QLatin1Char('\t') + keys.constFirst().toString(QKeySequence::NativeText);
        it.value()->setText(text);
    }
    QAction *live = m_actions.value(int(Action::LiveZoom));
    live->setEnabled(m_app->liveZoomSupported());
    live->setChecked(m_app->liveZoomActive());
}
