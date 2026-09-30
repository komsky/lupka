#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>

#include <optional>

// Everything the user can trigger, from a hotkey, the tray menu or the command line.
enum class Action {
    Zoom,
    Draw,
    Break,
    LiveZoom,
    LiveDraw,
    Record,
    RecordRegion,
    RecordWindow,
    Snip,
    SnipSave,
    // Used while LiveZoom is active; bound temporarily, never shown in settings.
    LiveZoomIn,
    LiveZoomOut,
    // Command line / tray only.
    Settings,
    Quit,
};

struct ActionInfo {
    Action action;
    QString id;     // command line verb and settings key
    QString label;  // shown in menus and the settings dialog
    QList<QKeySequence> defaults;
    bool configurable;  // has a user-editable hotkey
};

const QList<ActionInfo> &actionTable();
const ActionInfo &actionInfo(Action action);
std::optional<Action> actionFromId(const QString &id);
QList<Action> configurableActions();
