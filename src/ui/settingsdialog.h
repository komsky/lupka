#pragma once

#include "actions.h"

#include <QDialog>
#include <QHash>

class App;
class QKeySequenceEdit;
class QLabel;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(App *app);

private:
    QWidget *buildShortcutsTab();
    QWidget *buildZoomTab();
    QWidget *buildDrawTab();
    QWidget *buildSnipTab();
    QWidget *buildBreakTab();
    QWidget *buildRecordTab();
    QWidget *buildGeneralTab();
    void loadShortcuts();
    void saveShortcut(Action action);
    void updateStatus();

    App *m_app;
    QHash<int, QList<QKeySequenceEdit *>> m_editors;
    QLabel *m_status = nullptr;
};
