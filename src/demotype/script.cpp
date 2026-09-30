// SPDX-FileCopyrightText: 2026 Komsky
// SPDX-License-Identifier: GPL-3.0-or-later

#include "script.h"

#include <QRegularExpression>
#include <QStringDecoder>

namespace demoscript {
namespace {

constexpr quint32 kReturn = 0xff0d;
constexpr quint32 kLeft = 0xff51;
constexpr quint32 kUp = 0xff52;
constexpr quint32 kRight = 0xff53;
constexpr quint32 kDown = 0xff54;

// ZoomIt drops one line break right before [end]/[/paste] and right after
// [end]/[paste], so snippets can sit on their own lines in the file.
QString trimAround(QString text)
{
    static const QRegularExpression before(QStringLiteral("\\r?\\n(\\[end\\]|\\[/paste\\])"));
    static const QRegularExpression after(QStringLiteral("(\\[end\\]|\\[paste\\])\\r?\\n"));
    text.replace(before, QStringLiteral("\\1"));
    text.replace(after, QStringLiteral("\\1"));
    return text;
}

void addText(Snippet &snippet, const QString &text)
{
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\r'))
            continue;
        if (c == QLatin1Char('\n')) {
            snippet.append({TypeStep::Key, {}, kReturn, 0});
            continue;
        }
        QString one(c);
        if (c.isHighSurrogate() && i + 1 < text.size())
            one += text.at(++i);
        snippet.append({TypeStep::Char, one, 0, 0});
    }
}

}  // namespace

QString chooseSource(const QString &clipboard, const QString &file)
{
    static const QString start = QStringLiteral("[start]");
    if (clipboard.startsWith(start))
        return clipboard.mid(start.size());
    return file;
}

QString decode(const QByteArray &bytes)
{
    if (bytes.startsWith("\xff\xfe") || bytes.startsWith("\xfe\xff")) {
        QStringDecoder decoder(bytes.startsWith("\xff\xfe") ? QStringDecoder::Utf16LE : QStringDecoder::Utf16BE,
                               QStringDecoder::Flag::Stateless);
        return decoder.decode(bytes.mid(2));
    }
    if (bytes.startsWith("\xef\xbb\xbf"))
        return QString::fromUtf8(bytes.mid(3));
    return QString::fromUtf8(bytes);
}

QList<Snippet> parse(const QString &source)
{
    static const QRegularExpression pause(QStringLiteral("^\\[pause:(\\d{1,3})\\]"));
    const QString text = trimAround(source);
    QList<Snippet> snippets;
    Snippet current;
    int i = 0;
    auto flush = [&] {
        if (!current.isEmpty())
            snippets.append(current);
        current.clear();
    };
    while (i < text.size()) {
        if (text.at(i) != QLatin1Char('[')) {
            const int next = text.indexOf(QLatin1Char('['), i);
            addText(current, text.mid(i, next < 0 ? -1 : next - i));
            i = next < 0 ? int(text.size()) : next;
            continue;
        }
        const QStringView rest = QStringView(text).mid(i);
        auto keyword = [&](const char *word) { return rest.startsWith(QLatin1String(word)); };
        if (keyword("[end]")) {
            flush();
            i += 5;
        } else if (keyword("[enter]")) {
            current.append({TypeStep::Key, {}, kReturn, 0});
            i += 7;
        } else if (keyword("[up]")) {
            current.append({TypeStep::Key, {}, kUp, 0});
            i += 4;
        } else if (keyword("[down]")) {
            current.append({TypeStep::Key, {}, kDown, 0});
            i += 6;
        } else if (keyword("[left]")) {
            current.append({TypeStep::Key, {}, kLeft, 0});
            i += 6;
        } else if (keyword("[right]")) {
            current.append({TypeStep::Key, {}, kRight, 0});
            i += 7;
        } else if (keyword("[paste]")) {
            const int close = text.indexOf(QLatin1String("[/paste]"), i + 7);
            const QString body = close < 0 ? text.mid(i + 7) : text.mid(i + 7, close - i - 7);
            current.append({TypeStep::Paste, body, 0, 0});
            i = close < 0 ? int(text.size()) : close + 8;
        } else if (const auto m = pause.match(rest); m.hasMatch()) {
            current.append({TypeStep::Pause, {}, 0, m.captured(1).toInt() * 1000});
            i += int(m.capturedLength());
        } else {
            addText(current, QStringLiteral("["));
            ++i;
        }
    }
    flush();
    return snippets;
}

}  // namespace demoscript
