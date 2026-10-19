// S-expression reader.
//
// KiCad's board, schematic, footprint and worksheet files are all s-expressions:
// nested lists of atoms, where an atom is either a bare token or a quoted
// string. This reader is deliberately small and strict:
//
//   * it knows where every node came from (line and column), so an importer can
//     point a user at the exact spot that could not be understood,
//   * it refuses malformed input instead of guessing where a list ends,
//   * it has a nesting limit, so a file that is not what it claims to be cannot
//     exhaust the stack.
//
// It is a reader, not a formatter: the writer exists for tests and for error
// messages, not to round trip a user's file.
#pragma once

#include <QString>
#include <QVector>

namespace eda {

class SExpression
{
public:
    enum class Type { Atom, List };

    SExpression() = default;

    bool isList() const { return m_type == Type::List; }
    bool isAtom() const { return m_type == Type::Atom; }
    Type type() const { return m_type; }

    // Atom value with the quotes removed and the escapes resolved. Empty for a
    // list.
    QString text() const { return m_text; }
    void setText(const QString &text) { m_text = text; }

    const QVector<SExpression> &children() const { return m_children; }
    void append(const SExpression &child) { m_children.append(child); }
    void setType(Type type) { m_type = type; }

    // --- navigation -------------------------------------------------------
    // For a list, the text of its first child ("(at 1 2)" -> "at"). For an
    // atom, the atom itself.
    QString head() const;
    bool hasHead(const QString &name) const;

    // First child list whose head is <name>, or nullptr.
    const SExpression *child(const QString &name) const;
    // Every child list with that head, in file order.
    QVector<const SExpression *> children(const QString &name) const;

    // Child at <index> as an atom; "" when there is no such child or it is a
    // list.
    QString atomAt(int index) const;
    // Child at <index> converted to a double. Returns false when the child is
    // missing, is a list, or is not a number.
    bool numberAt(int index, double *value) const;
    // Convenience: 0 when the value is not there. Only for cases where a
    // missing number and a zero mean the same thing.
    double numberAtOrZero(int index) const;

    int line() const { return m_line; }
    int column() const { return m_column; }
    void setPosition(int line, int column)
    {
        m_line = line;
        m_column = column;
    }

    // Re-serialises the node. Used by tests and by error messages.
    QString toString() const;

private:
    Type m_type = Type::Atom;
    QString m_text;
    QVector<SExpression> m_children;
    int m_line = 0;
    int m_column = 0;
};

// Parses a complete file. Returns false (with a message that names the line and
// column) when the input is not a single, well formed s-expression.
bool parseSExpression(const QString &text, SExpression *out, QString *error = nullptr);

} // namespace eda
