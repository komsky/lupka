#pragma once

#include <QList>
#include <QString>

// One thing DemoType does: type a character, press a key, wait, or paste.
struct TypeStep {
    enum Kind { Char, Key, Pause, Paste };
    Kind kind = Char;
    QString text;      // Char: one character (or surrogate pair); Paste: the text
    quint32 keysym = 0;  // Key
    int ms = 0;          // Pause
};

using Snippet = QList<TypeStep>;

// A DemoType script in ZoomIt's format: snippets separated by [end], with
// [pause:n], [enter], [up], [down], [left], [right] and [paste]...[/paste].
// Anything else in brackets is typed literally.
namespace demoscript {

// Text of the script to use: the clipboard if it starts with [start]
// (which is removed), otherwise the file contents. Empty if neither.
QString chooseSource(const QString &clipboard, const QString &file);

QList<Snippet> parse(const QString &text);

// Decode a script file (UTF-8, UTF-8 with BOM, UTF-16 LE/BE with BOM).
QString decode(const QByteArray &bytes);

}  // namespace demoscript
