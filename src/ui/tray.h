// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "actions.h"

#include <QHash>
#include <QObject>

class App;
class QAction;
class QMenu;
class QSystemTrayIcon;

class Tray : public QObject
{
    Q_OBJECT
public:
    explicit Tray(App *app);
    ~Tray() override;

    void setVisible(bool visible);
    void refresh();

private:
    App *m_app;
    QSystemTrayIcon *m_icon = nullptr;
    QMenu *m_menu = nullptr;
    QHash<int, QAction *> m_actions;
    bool m_showingRecording = false;
};
