#include "actions.h"

const QList<ActionInfo> &actionTable()
{
    static const QList<ActionInfo> table = {
        {Action::Zoom, QStringLiteral("zoom"), QStringLiteral("Zoom"),
         {QKeySequence(QStringLiteral("Ctrl+1"))}, true},
        {Action::Draw, QStringLiteral("draw"), QStringLiteral("Draw"),
         {QKeySequence(QStringLiteral("Ctrl+2"))}, true},
        {Action::Break, QStringLiteral("break"), QStringLiteral("Break timer"),
         {QKeySequence(QStringLiteral("Ctrl+3"))}, true},
        {Action::LiveZoom, QStringLiteral("livezoom"), QStringLiteral("Live zoom"),
         {QKeySequence(QStringLiteral("Ctrl+4"))}, true},
        {Action::LiveDraw, QStringLiteral("livedraw"), QStringLiteral("Live draw"),
         {QKeySequence(QStringLiteral("Ctrl+Shift+4"))}, true},
        {Action::Record, QStringLiteral("record"), QStringLiteral("Record screen"),
         {QKeySequence(QStringLiteral("Ctrl+5"))}, true},
        {Action::RecordRegion, QStringLiteral("record-region"), QStringLiteral("Record region"),
         {QKeySequence(QStringLiteral("Ctrl+Shift+5"))}, true},
        {Action::RecordWindow, QStringLiteral("record-window"), QStringLiteral("Record window"),
         {QKeySequence(QStringLiteral("Ctrl+Alt+5"))}, true},
        {Action::Snip, QStringLiteral("snip"), QStringLiteral("Snip region to clipboard"),
         {QKeySequence(QStringLiteral("Ctrl+6")), QKeySequence(QStringLiteral("Meta+Shift+S"))}, true},
        {Action::SnipSave, QStringLiteral("snip-save"), QStringLiteral("Snip region to file"),
         {QKeySequence(QStringLiteral("Ctrl+Shift+6"))}, true},
        {Action::DemoType, QStringLiteral("demotype"), QStringLiteral("DemoType next snippet"),
         {QKeySequence(QStringLiteral("Ctrl+7"))}, true},
        {Action::DemoTypeBack, QStringLiteral("demotype-back"), QStringLiteral("DemoType step back"),
         {QKeySequence(QStringLiteral("Ctrl+Shift+7"))}, true},
        {Action::DemoTypeStop, QStringLiteral("demotype-stop"), QStringLiteral("Stop DemoType"),
         {QKeySequence(QStringLiteral("Esc"))}, false},
        {Action::LiveZoomIn, QStringLiteral("livezoom-in"), QStringLiteral("Live zoom in"),
         {QKeySequence(QStringLiteral("Ctrl+Up"))}, false},
        {Action::LiveZoomOut, QStringLiteral("livezoom-out"), QStringLiteral("Live zoom out"),
         {QKeySequence(QStringLiteral("Ctrl+Down"))}, false},
        {Action::Settings, QStringLiteral("settings"), QStringLiteral("Settings"), {}, false},
        {Action::Quit, QStringLiteral("quit"), QStringLiteral("Quit"), {}, false},
    };
    return table;
}

const ActionInfo &actionInfo(Action action)
{
    for (const ActionInfo &info : actionTable()) {
        if (info.action == action)
            return info;
    }
    Q_UNREACHABLE();
}

std::optional<Action> actionFromId(const QString &id)
{
    for (const ActionInfo &info : actionTable()) {
        if (info.id == id)
            return info.action;
    }
    return std::nullopt;
}

QList<Action> configurableActions()
{
    QList<Action> result;
    for (const ActionInfo &info : actionTable()) {
        if (info.configurable)
            result << info.action;
    }
    return result;
}
