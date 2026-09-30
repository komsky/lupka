#include "keynames.h"

#include <QHash>
#include <QRegularExpression>

namespace keynames {
namespace {

struct Named {
    Qt::Key key;
    uint32_t sym;
    const char *name;
};

// Keys that are not plain printable ASCII.
const Named kSpecial[] = {
    {Qt::Key_Space, 0x0020, "space"},
    {Qt::Key_Escape, 0xff1b, "Escape"},
    {Qt::Key_Tab, 0xff09, "Tab"},
    {Qt::Key_Backspace, 0xff08, "BackSpace"},
    {Qt::Key_Return, 0xff0d, "Return"},
    {Qt::Key_Enter, 0xff8d, "KP_Enter"},
    {Qt::Key_Insert, 0xff63, "Insert"},
    {Qt::Key_Delete, 0xffff, "Delete"},
    {Qt::Key_Pause, 0xff13, "Pause"},
    {Qt::Key_Print, 0xff61, "Print"},
    {Qt::Key_ScrollLock, 0xff14, "Scroll_Lock"},
    {Qt::Key_Menu, 0xff67, "Menu"},
    {Qt::Key_Home, 0xff50, "Home"},
    {Qt::Key_End, 0xff57, "End"},
    {Qt::Key_Left, 0xff51, "Left"},
    {Qt::Key_Up, 0xff52, "Up"},
    {Qt::Key_Right, 0xff53, "Right"},
    {Qt::Key_Down, 0xff54, "Down"},
    {Qt::Key_PageUp, 0xff55, "Page_Up"},
    {Qt::Key_PageDown, 0xff56, "Page_Down"},
};

// Printable ASCII punctuation, indexed by character.
const QHash<char, const char *> &punctuationNames()
{
    static const QHash<char, const char *> names = {
        {'!', "exclam"}, {'"', "quotedbl"}, {'#', "numbersign"}, {'$', "dollar"},
        {'%', "percent"}, {'&', "ampersand"}, {'\'', "apostrophe"}, {'(', "parenleft"},
        {')', "parenright"}, {'*', "asterisk"}, {'+', "plus"}, {',', "comma"},
        {'-', "minus"}, {'.', "period"}, {'/', "slash"}, {':', "colon"},
        {';', "semicolon"}, {'<', "less"}, {'=', "equal"}, {'>', "greater"},
        {'?', "question"}, {'@', "at"}, {'[', "bracketleft"}, {'\\', "backslash"},
        {']', "bracketright"}, {'^', "asciicircum"}, {'_', "underscore"}, {'`', "grave"},
        {'{', "braceleft"}, {'|', "bar"}, {'}', "braceright"}, {'~', "asciitilde"},
    };
    return names;
}

}  // namespace

uint32_t keysym(Qt::Key key)
{
    for (const Named &n : kSpecial) {
        if (n.key == key)
            return n.sym;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return 0xffbe + (key - Qt::Key_F1);
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return 'a' + (key - Qt::Key_A);
    if (key > 0x20 && key < 0x7f)
        return uint32_t(key);
    return 0;
}

QString keysymName(Qt::Key key)
{
    for (const Named &n : kSpecial) {
        if (n.key == key)
            return QString::fromLatin1(n.name);
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return QStringLiteral("F%1").arg(int(key - Qt::Key_F1) + 1);
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return QString(QChar('a' + (key - Qt::Key_A)));
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return QString(QChar('0' + (key - Qt::Key_0)));
    if (key > 0x20 && key < 0x7f) {
        const auto it = punctuationNames().constFind(char(key));
        if (it != punctuationNames().constEnd())
            return QString::fromLatin1(it.value());
    }
    return {};
}

QString toGtkAccelerator(QKeyCombination combo)
{
    const QString name = keysymName(combo.key());
    if (name.isEmpty())
        return {};
    QString accel;
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    if (mods & Qt::ControlModifier)
        accel += QLatin1String("<Primary>");
    if (mods & Qt::AltModifier)
        accel += QLatin1String("<Alt>");
    if (mods & Qt::MetaModifier)
        accel += QLatin1String("<Super>");
    if (mods & Qt::ShiftModifier)
        accel += QLatin1String("<Shift>");
    return accel + name;
}

QString toGtkAccelerator(const QKeySequence &sequence)
{
    if (sequence.isEmpty())
        return {};
    return toGtkAccelerator(sequence[0]);
}

QKeyCombination fromGtkAccelerator(const QString &accelerator)
{
    static const QRegularExpression modifier(QStringLiteral("^<([A-Za-z0-9_]+)>"));
    QString rest = accelerator.trimmed();
    Qt::KeyboardModifiers mods;
    for (;;) {
        const QRegularExpressionMatch m = modifier.match(rest);
        if (!m.hasMatch())
            break;
        const QString mod = m.captured(1).toLower();
        if (mod == QLatin1String("primary") || mod == QLatin1String("control") || mod == QLatin1String("ctrl"))
            mods |= Qt::ControlModifier;
        else if (mod == QLatin1String("shift"))
            mods |= Qt::ShiftModifier;
        else if (mod == QLatin1String("alt") || mod == QLatin1String("mod1"))
            mods |= Qt::AltModifier;
        else if (mod == QLatin1String("super") || mod == QLatin1String("meta") || mod == QLatin1String("mod4"))
            mods |= Qt::MetaModifier;
        else
            return QKeyCombination(Qt::Key_unknown);
        rest = rest.mid(m.capturedLength());
    }
    if (rest.isEmpty())
        return QKeyCombination(Qt::Key_unknown);

    for (const Named &n : kSpecial) {
        if (rest == QLatin1String(n.name))
            return QKeyCombination(mods, n.key);
    }
    if (rest.size() > 1 && rest.startsWith(QLatin1Char('F'))) {
        bool ok = false;
        const int n = rest.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 24)
            return QKeyCombination(mods, Qt::Key(Qt::Key_F1 + n - 1));
    }
    if (rest.size() == 1) {
        const QChar c = rest.at(0).toUpper();
        if (c.unicode() > 0x20 && c.unicode() < 0x7f)
            return QKeyCombination(mods, Qt::Key(c.unicode()));
    }
    const auto &names = punctuationNames();
    for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
        if (rest == QLatin1String(it.value()))
            return QKeyCombination(mods, Qt::Key(it.key()));
    }
    return QKeyCombination(Qt::Key_unknown);
}

}  // namespace keynames
