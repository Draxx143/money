#include "SExpression.h"

#include <QChar>
#include <QStringList>

#include <cmath>

namespace eda {
namespace {

// Deeper nesting than this is not a board file.
constexpr int kMaxDepth = 128;

bool isDelimiter(QChar c)
{
    return c == QLatin1Char('(') || c == QLatin1Char(')') || c == QLatin1Char('"')
        || c == QLatin1Char(';') || c.isSpace();
}

class Reader
{
public:
    Reader(const QString &text) : m_text(text) {}

    bool parse(SExpression *out, QString *error)
    {
        // Parsing into a fresh node, and only handing it over when the whole
        // input was understood: a caller that reuses one node for several files
        // never sees half of one file mixed into another, and a failed parse
        // leaves the node it was given alone.
        SExpression root;
        skipSpaceAndComments();
        if (m_pos >= m_text.size()) {
            *error = QStringLiteral("the file contains no s-expression");
            return false;
        }
        if (!parseNode(&root, 0, error))
            return false;
        skipSpaceAndComments();
        if (m_pos < m_text.size()) {
            *error = QStringLiteral("line %1, column %2: unexpected content after the "
                                    "top level expression")
                         .arg(m_line)
                         .arg(m_column);
            return false;
        }
        *out = root;
        return true;
    }

private:
    bool parseNode(SExpression *out, int depth, QString *error)
    {
        if (depth > kMaxDepth) {
            *error = QStringLiteral("line %1, column %2: the expression is nested more "
                                    "than %3 levels deep")
                         .arg(m_line)
                         .arg(m_column)
                         .arg(kMaxDepth);
            return false;
        }

        const QChar c = m_text.at(m_pos);
        if (c == QLatin1Char('('))
            return parseList(out, depth, error);
        if (c == QLatin1Char(')')) {
            *error = QStringLiteral("line %1, column %2: ')' without a matching '('")
                         .arg(m_line)
                         .arg(m_column);
            return false;
        }
        if (c == QLatin1Char('"'))
            return parseString(out, error);

        out->setType(SExpression::Type::Atom);
        out->setPosition(m_line, m_column);
        QString token;
        while (m_pos < m_text.size() && !isDelimiter(m_text.at(m_pos))) {
            token.append(m_text.at(m_pos));
            advance();
        }
        out->setText(token);
        if (token.isEmpty()) {
            *error = QStringLiteral("line %1, column %2: empty token")
                         .arg(m_line)
                         .arg(m_column);
            return false;
        }
        return true;
    }

    bool parseList(SExpression *out, int depth, QString *error)
    {
        out->setType(SExpression::Type::List);
        out->setPosition(m_line, m_column);
        advance(); // '('
        for (;;) {
            skipSpaceAndComments();
            if (m_pos >= m_text.size()) {
                *error = QStringLiteral("line %1, column %2: end of file inside a list that "
                                        "was opened on line %3, column %4")
                             .arg(m_line)
                             .arg(m_column)
                             .arg(out->line())
                             .arg(out->column());
                return false;
            }
            if (m_text.at(m_pos) == QLatin1Char(')')) {
                advance();
                return true;
            }
            SExpression child;
            if (!parseNode(&child, depth + 1, error))
                return false;
            out->append(child);
        }
    }

    bool parseString(SExpression *out, QString *error)
    {
        out->setType(SExpression::Type::Atom);
        out->setPosition(m_line, m_column);
        advance(); // opening quote
        QString value;
        while (m_pos < m_text.size()) {
            const QChar c = m_text.at(m_pos);
            if (c == QLatin1Char('\\')) {
                advance();
                if (m_pos >= m_text.size())
                    break;
                const QChar escaped = m_text.at(m_pos);
                switch (escaped.unicode()) {
                case 'n': value.append(QLatin1Char('\n')); break;
                case 'r': value.append(QLatin1Char('\r')); break;
                case 't': value.append(QLatin1Char('\t')); break;
                default:
                    // KiCad escapes the quote; anything else is kept verbatim.
                    value.append(escaped);
                    break;
                }
                advance();
                continue;
            }
            if (c == QLatin1Char('"')) {
                advance();
                out->setText(value);
                return true;
            }
            value.append(c);
            advance();
        }
        *error = QStringLiteral("line %1, column %2: unterminated string")
                     .arg(m_line)
                     .arg(m_column);
        return false;
    }

    void skipSpaceAndComments()
    {
        for (;;) {
            while (m_pos < m_text.size() && m_text.at(m_pos).isSpace())
                advance();
            // KiCad itself has no comments, but hand edited files do. A ';' is
            // only a comment when it starts a token, so a ';' inside a quoted
            // string or in the middle of a token is left alone.
            if (m_pos < m_text.size() && m_text.at(m_pos) == QLatin1Char(';')) {
                while (m_pos < m_text.size() && m_text.at(m_pos) != QLatin1Char('\n'))
                    advance();
                continue;
            }
            return;
        }
    }

    void advance()
    {
        if (m_text.at(m_pos) == QLatin1Char('\n')) {
            ++m_line;
            m_column = 1;
        } else {
            ++m_column;
        }
        ++m_pos;
    }

    const QString &m_text;
    int m_pos = 0;
    int m_line = 1;
    int m_column = 1;
};

QString escapeForOutput(const QString &value)
{
    QString out;
    out.reserve(value.size() + 2);
    for (const QChar c : value) {
        switch (c.unicode()) {
        case '"': out.append(QStringLiteral("\\\"")); break;
        case '\\': out.append(QStringLiteral("\\\\")); break;
        case '\n': out.append(QStringLiteral("\\n")); break;
        case '\r': out.append(QStringLiteral("\\r")); break;
        case '\t': out.append(QStringLiteral("\\t")); break;
        default: out.append(c);
        }
    }
    return out;
}

bool needsQuotes(const QString &value)
{
    if (value.isEmpty())
        return true;
    for (const QChar c : value) {
        if (isDelimiter(c))
            return true;
    }
    return false;
}

} // namespace

QString SExpression::head() const { return m_children.isEmpty() ? m_text : m_children.first().text(); }

bool SExpression::hasHead(const QString &name) const { return isList() && head() == name; }

const SExpression *SExpression::child(const QString &name) const
{
    for (const SExpression &candidate : m_children) {
        if (candidate.isList() && candidate.head() == name)
            return &candidate;
    }
    return nullptr;
}

QVector<const SExpression *> SExpression::children(const QString &name) const
{
    QVector<const SExpression *> found;
    for (const SExpression &candidate : m_children) {
        if (candidate.isList() && candidate.head() == name)
            found.append(&candidate);
    }
    return found;
}

QString SExpression::atomAt(int index) const
{
    if (index < 0 || index >= m_children.size())
        return QString();
    const SExpression &child = m_children.at(index);
    return child.isAtom() ? child.text() : QString();
}

bool SExpression::numberAt(int index, double *value) const
{
    const QString text = atomAt(index);
    if (text.isEmpty())
        return false;
    bool ok = false;
    const double parsed = text.toDouble(&ok);
    if (!ok || !std::isfinite(parsed))
        return false;
    *value = parsed;
    return true;
}

double SExpression::numberAtOrZero(int index) const
{
    double value = 0.0;
    numberAt(index, &value);
    return value;
}

QString SExpression::toString() const
{
    if (m_type == Type::Atom) {
        if (needsQuotes(m_text))
            return QLatin1Char('"') + escapeForOutput(m_text) + QLatin1Char('"');
        return m_text;
    }
    QStringList parts;
    parts.reserve(m_children.size());
    for (const SExpression &child : m_children)
        parts.append(child.toString());
    return QLatin1Char('(') + parts.join(QLatin1Char(' ')) + QLatin1Char(')');
}

bool parseSExpression(const QString &text, SExpression *out, QString *error)
{
    if (!out)
        return false;
    Reader reader(text);
    QString message;
    const bool ok = reader.parse(out, &message);
    if (!ok && error)
        *error = message;
    return ok;
}

} // namespace eda
