#include "demotype/script.h"

#include <QTest>

namespace {

QString typed(const Snippet &snippet)
{
    QString out;
    for (const TypeStep &step : snippet) {
        switch (step.kind) {
        case TypeStep::Char:
            out += step.text;
            break;
        case TypeStep::Key:
            out += step.keysym == 0xff0d ? QStringLiteral("<ret>") : QStringLiteral("<key:%1>").arg(step.keysym, 0, 16);
            break;
        case TypeStep::Pause:
            out += QStringLiteral("<pause:%1>").arg(step.ms);
            break;
        case TypeStep::Paste:
            out += QStringLiteral("<paste:%1>").arg(step.text);
            break;
        }
    }
    return out;
}

}  // namespace

class TestDemoScript : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void splitsOnEnd()
    {
        const auto s = demoscript::parse(QStringLiteral("one[end]two[end]three"));
        QCOMPARE(s.size(), 3);
        QCOMPARE(typed(s[0]), QStringLiteral("one"));
        QCOMPARE(typed(s[2]), QStringLiteral("three"));
    }

    void trimsNewlinesAroundEnd()
    {
        const auto s = demoscript::parse(QStringLiteral("line1\nline2\n[end]\nnext\n[end]\n"));
        QCOMPARE(s.size(), 2);
        QCOMPARE(typed(s[0]), QStringLiteral("line1<ret>line2"));
        QCOMPARE(typed(s[1]), QStringLiteral("next"));
    }

    void keywords()
    {
        const auto s = demoscript::parse(QStringLiteral("a[enter]b[up][down][left][right][pause:2]c"));
        QCOMPARE(typed(s[0]), QStringLiteral("a<ret>b<key:ff52><key:ff54><key:ff51><key:ff53><pause:2000>c"));
    }

    void pasteBlocks()
    {
        // The line breaks inside the markers go; the one after [/paste] stays (ZoomIt's rule).
        const auto s = demoscript::parse(QStringLiteral("x[paste]\nint main() {}\n[/paste]\ny"));
        QCOMPARE(typed(s[0]), QStringLiteral("x<paste:int main() {}><ret>y"));
    }

    void unknownBracketsAreLiteral()
    {
        const auto s = demoscript::parse(QStringLiteral("arr[0] = [pause:x];"));
        QCOMPARE(typed(s[0]), QStringLiteral("arr[0] = [pause:x];"));
    }

    void clipboardNeedsStart()
    {
        QCOMPARE(demoscript::chooseSource(QStringLiteral("[start]hello"), QStringLiteral("file")), QStringLiteral("hello"));
        QCOMPARE(demoscript::chooseSource(QStringLiteral("hello"), QStringLiteral("file")), QStringLiteral("file"));
    }

    void decodesBoms()
    {
        QCOMPARE(demoscript::decode(QByteArray("\xef\xbb\xbfzaż\xc3\xb3\xc5\x82\xc4\x87")), QStringLiteral(u"zażółć"));
        const QByteArray le("\xff\xfe" "h\0i\0", 6);
        QCOMPARE(demoscript::decode(le), QStringLiteral("hi"));
    }

    void keepsSurrogatePairsTogether()
    {
        const auto s = demoscript::parse(QStringLiteral(u"a😀b"));
        QCOMPARE(s[0].size(), 3);
        QCOMPARE(s[0][1].text, QStringLiteral(u"😀"));
    }
};

QTEST_APPLESS_MAIN(TestDemoScript)
#include "test_demoscript.moc"
