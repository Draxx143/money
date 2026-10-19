// Unit tests for the s-expression reader every KiCad file is written in.
#include <QtTest>

#include "../../src/core/SExpression.h"

class TestSExpression : public QObject
{
    Q_OBJECT

private:
    bool parse(const QString &text, eda::SExpression *out, QString *error = nullptr)
    {
        QString message;
        const bool ok = eda::parseSExpression(text, out, &message);
        if (error)
            *error = message;
        return ok;
    }

private slots:
    void bareAtoms()
    {
        eda::SExpression node;
        QVERIFY(parse(QStringLiteral("kicad_pcb"), &node));
        QVERIFY(node.isAtom());
        QCOMPARE(node.text(), QStringLiteral("kicad_pcb"));
        QVERIFY(node.children().isEmpty());
    }

    void numbersKeepTheirText()
    {
        eda::SExpression node;
        QVERIFY(parse(QStringLiteral("(at 12.5 -3 90)"), &node));
        QVERIFY(node.isList());
        QCOMPARE(node.head(), QStringLiteral("at"));
        QCOMPARE(node.children().size(), 4);
        double value = 0.0;
        QVERIFY(node.numberAt(1, &value));
        QCOMPARE(value, 12.5);
        QVERIFY(node.numberAt(2, &value));
        QCOMPARE(value, -3.0);
        QVERIFY(node.numberAt(3, &value));
        QCOMPARE(value, 90.0);
        QVERIFY(!node.numberAt(4, &value));
        QCOMPARE(node.numberAtOrZero(4), 0.0);
    }

    void quotedStringsAndEscapes()
    {
        eda::SExpression node;
        QVERIFY(parse(QStringLiteral("(property \"Reference\" \"R\\\"1\" (at 0 0))"), &node));
        QCOMPARE(node.atomAt(0), QStringLiteral("property")); // the head is child 0
        QCOMPARE(node.atomAt(1), QStringLiteral("Reference"));
        QCOMPARE(node.atomAt(2), QStringLiteral("R\"1"));
        const eda::SExpression *at = node.child(QStringLiteral("at"));
        QVERIFY(at != nullptr);
        QCOMPARE(at->children().size(), 3);

        // A quoted string keeps spaces and delimiters.
        QVERIFY(parse(QStringLiteral("(\"a b (c)\" x)"), &node));
        QCOMPARE(node.head(), QStringLiteral("a b (c)"));
        QCOMPARE(node.atomAt(1), QStringLiteral("x"));
    }

    void navigationHelpers()
    {
        eda::SExpression node;
        QVERIFY(parse(QStringLiteral("(pad \"1\" smd rect (at 1 2) (size 3 4) (layers \"F.Cu\" \"F.Mask\"))"),
                      &node));
        QVERIFY(node.hasHead(QStringLiteral("pad")));
        QCOMPARE(node.atomAt(1), QStringLiteral("1"));
        QCOMPARE(node.atomAt(2), QStringLiteral("smd"));
        QCOMPARE(node.atomAt(3), QStringLiteral("rect"));

        const eda::SExpression *layers = node.child(QStringLiteral("layers"));
        QVERIFY(layers != nullptr);
        QCOMPARE(layers->atomAt(1), QStringLiteral("F.Cu"));
        QCOMPARE(layers->atomAt(2), QStringLiteral("F.Mask"));

        QVERIFY(node.child(QStringLiteral("nonsense")) == nullptr);
        // A list is not an atom, so asking for its text gives nothing.
        QCOMPARE(node.atomAt(4), QString());
    }

    void repeatedChildren()
    {
        eda::SExpression node;
        QVERIFY(parse(QStringLiteral("(kicad_pcb (net 0 \"\") (net 1 \"GND\") (net 2 \"VCC\"))"), &node));
        QCOMPARE(node.children(QStringLiteral("net")).size(), 3);
        QCOMPARE(node.child(QStringLiteral("net"))->atomAt(2), QString());
    }

    void commentsOutsideStringsAreSkipped()
    {
        eda::SExpression node;
        QVERIFY(parse(QStringLiteral("(kicad_pcb ; a comment (with parens\n (version 1))"), &node));
        QCOMPARE(node.children().size(), 2);
        QCOMPARE(node.child(QStringLiteral("version"))->atomAt(1), QStringLiteral("1"));
    }

    void nestingCanSpanLines()
    {
        eda::SExpression node;
        QVERIFY(parse(QStringLiteral("(a\n  (b\n    (c 1)\n  )\n)"), &node));
        const eda::SExpression *b = node.child(QStringLiteral("b"));
        QVERIFY(b != nullptr);
        QCOMPARE(b->child(QStringLiteral("c"))->atomAt(1), QStringLiteral("1"));
        // The position of a node is the line and column it starts on.
        QCOMPARE(node.line(), 1);
        QCOMPARE(node.column(), 1);
        QCOMPARE(b->line(), 2);
    }

    void serialisationRoundTrips()
    {
        const QString text = QStringLiteral(
            "(kicad_pcb (version 20240101) (layers (0 \"F.Cu\" signal)) "
            "(segment (start 1 2) (end 3 4) (width 0.25)))");
        eda::SExpression node;
        QVERIFY(parse(text, &node));
        const QString again = node.toString();
        eda::SExpression reparsed;
        QVERIFY2(parse(again, &reparsed), qPrintable(again));
        QCOMPARE(reparsed.toString(), again);
    }

    void malformedInputIsRefused()
    {
        eda::SExpression node;
        QString error;

        QVERIFY(!parse(QStringLiteral("(kicad_pcb (version 1)"), &node, &error));
        QVERIFY2(error.contains(QStringLiteral("end of file")), qPrintable(error));
        QVERIFY2(error.contains(QStringLiteral("line 1")), qPrintable(error));

        QVERIFY(!parse(QStringLiteral("(a))"), &node, &error));
        QVERIFY2(error.contains(QStringLiteral("after the top level expression")),
                 qPrintable(error));

        QVERIFY(!parse(QStringLiteral(")"), &node, &error));
        QVERIFY2(error.contains(QStringLiteral("without a matching")), qPrintable(error));

        QVERIFY(!parse(QStringLiteral("(unterminated \"string)"), &node, &error));
        QVERIFY2(error.contains(QStringLiteral("unterminated string")), qPrintable(error));

        QVERIFY(!parse(QString(), &node, &error));
        QVERIFY(!parse(QStringLiteral("   \n  "), &node, &error));
    }

    void deepNestingIsRefusedInsteadOfCrashing()
    {
        QString text;
        text.reserve(2000);
        for (int i = 0; i < 1000; ++i)
            text.append(QLatin1Char('('));
        eda::SExpression node;
        QString error;
        QVERIFY(!parse(text, &node, &error));
        QVERIFY2(error.contains(QStringLiteral("nested")), qPrintable(error));
    }
};

QTEST_APPLESS_MAIN(TestSExpression)
#include "test_sexpression.moc"
