#include "tray.h"

#include "app.h"
#include "config.h"
#include "settings.h"

#include <QMenu>
#include <QPainter>
#include <QSystemTrayIcon>

Tray::Tray(App *app)
    : QObject(app)
    , m_app(app)
{
    m_menu = new QMenu;
    const QList<Action> entries = {Action::Zoom,     Action::Draw,         Action::LiveDraw,
                                   Action::Snip,     Action::SnipSave,     Action::Record,
                                   Action::RecordRegion, Action::RecordWindow, Action::Break,
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
    connect(app, &App::recordingChanged, this, &Tray::refresh);
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

    const bool recording = m_app->isRecording();
    if (recording)
        m_actions.value(int(Action::Record))->setText(tr("Stop recording"));
    m_actions.value(int(Action::RecordRegion))->setVisible(!recording);
    m_actions.value(int(Action::RecordWindow))->setVisible(!recording);
    if (recording != m_showingRecording) {
        m_showingRecording = recording;
        QPixmap pixmap(QStringLiteral(":/icons/tray.png"));
        if (recording) {
            // A red dot tells the presenter the screen is being recorded.
            QPainter p(&pixmap);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(Qt::white, pixmap.width() / 24.0));
            p.setBrush(QColor(224, 27, 36));
            const double r = pixmap.width() * 0.22;
            p.drawEllipse(QPointF(pixmap.width() - r - 2, r + 2), r, r);
        }
        m_icon->setIcon(QIcon(pixmap));
        m_icon->setToolTip(recording ? tr("%1 (recording)").arg(QStringLiteral(APP_NAME)) : QStringLiteral(APP_NAME));
    }
}
