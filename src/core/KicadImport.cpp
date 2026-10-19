#include "KicadImport.h"

#include "SExpression.h"

#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QTextStream>
#include <QUuid>

#include <cmath>
#include <QtGlobal>

namespace eda {
namespace {

// Warnings are collected once each and in the order they came up: a board with
// forty silkscreen drawings must not produce forty identical lines.
class WarningLog
{
public:
    void add(const QString &message)
    {
        if (m_seen.contains(message))
            return;
        m_seen.insert(message);
        m_messages.append(message);
    }

    // <count> <= 0 says nothing at all, which is what "nothing was skipped"
    // means.
    void addCounted(int count, const QString &one, const QString &many)
    {
        if (count <= 0)
            return;
        add(count == 1 ? one : many.arg(count));
    }

    QStringList messages() const { return m_messages; }

private:
    QSet<QString> m_seen;
    QStringList m_messages;
};

struct Bounds
{
    bool valid = false;
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;

    void add(double x, double y)
    {
        if (!valid) {
            minX = maxX = x;
            minY = maxY = y;
            valid = true;
            return;
        }
        minX = qMin(minX, x);
        minY = qMin(minY, y);
        maxX = qMax(maxX, x);
        maxY = qMax(maxY, y);
    }

    void add(const Point2D &p) { add(p.x, p.y); }

    Rect2D toRect() const
    {
        if (!valid)
            return Rect2D();
        Rect2D r;
        r.x = minX;
        r.y = minY;
        r.width = maxX - minX;
        r.height = maxY - minY;
        return r;
    }
};

// How finely a circle or an arc is turned into a polyline. The board outline is
// drawn from these points, and at any sane zoom 32 steps around a circle are
// below one pixel.
constexpr int kCircleSegments = 32;
// kPi is not part of strict ISO C++, and this file is compiled with -std=c++20.
constexpr double kPi = 3.14159265358979323846;


// "(layer "F.Cu")" of a list -> "F.Cu".
QString layerNameOf(const SExpression *node)
{
    const SExpression *layer = node ? node->child(QStringLiteral("layer")) : nullptr;
    return layer ? layer->atomAt(1) : QString();
}

// "(start 1 2)" / "(at 1 2 90)" / "(center 1 2)" -> point; the optional third
// number is a rotation and is returned through <rotation> when given.
bool pointOf(const SExpression *node, const char *head, Point2D *out, double *rotation = nullptr)
{
    if (!node)
        return false;
    const SExpression *child = node->child(QString::fromLatin1(head));
    if (!child)
        return false;
    double x = 0.0;
    double y = 0.0;
    if (!child->numberAt(1, &x) || !child->numberAt(2, &y))
        return false;
    out->x = x;
    out->y = y;
    if (rotation)
        *rotation = child->numberAtOrZero(3);
    return true;
}

QVector<Point2D> pointsOfPolygon(const SExpression *node)
{
    QVector<Point2D> points;
    if (!node)
        return points;
    const SExpression *pts = node->child(QStringLiteral("pts"));
    if (!pts)
        return points;
    for (const SExpression *xy : pts->children(QStringLiteral("xy"))) {
        Point2D p;
        if (xy->numberAt(1, &p.x) && xy->numberAt(2, &p.y))
            points.append(p);
    }
    return points;
}

QVector<Point2D> circleToPolygon(const Point2D &center, double radius)
{
    QVector<Point2D> points;
    for (int i = 0; i <= kCircleSegments; ++i) { // closed: the last point repeats the first
        const double angle = 2.0 * kPi * double(i) / double(kCircleSegments);
        points.append({ center.x + radius * std::cos(angle),
                        center.y + radius * std::sin(angle) });
    }
    return points;
}

// Board outlines arrive as loose pieces -- a KiCad board outline is a handful of
// separate segments, and an arc or a circle is one piece on its own. This joins
// them end to end into closed loops, which is what the board outline is. Pieces
// that do not meet anything are kept as they are.
QVector<QVector<Point2D>> chainOutlines(const QVector<QVector<Point2D>> &pieces,
                                        double tolerance = 1e-4)
{
    auto same = [tolerance](const Point2D &a, const Point2D &b) {
        return std::abs(a.x - b.x) <= tolerance && std::abs(a.y - b.y) <= tolerance;
    };
    // A gap this small is where two pieces were meant to meet; KiCad files
    // routinely differ in the last decimal.
    auto meets = [](const Point2D &a, const Point2D &b) {
        return std::abs(a.x - b.x) <= 0.05 && std::abs(a.y - b.y) <= 0.05;
    };
    auto reversedCopy = [](const QVector<Point2D> &points) {
        QVector<Point2D> copy(points);
        std::reverse(copy.begin(), copy.end());
        return copy;
    };

    QVector<QVector<Point2D>> loops;
    QVector<bool> used(pieces.size(), false);

    // Starting from each piece in the order the file lists them keeps the
    // outline in the file's order.
    for (int start = 0; start < pieces.size(); ++start) {
        if (used.at(start) || pieces.at(start).size() < 2)
            continue;
        used[start] = true;
        QVector<Point2D> current = pieces.at(start);
        if (same(current.first(), current.last())) {
            loops.append(current); // already closed: a circle or a closed polygon
            continue;
        }

        bool extended = true;
        while (extended) {
            extended = false;
            for (int i = 0; i < pieces.size(); ++i) {
                if (used.at(i) || pieces.at(i).size() < 2)
                    continue;
                const QVector<Point2D> &candidate = pieces.at(i);
                QVector<Point2D> joined = candidate;
                bool atEnd = false;
                if (meets(current.last(), candidate.first())) {
                    atEnd = true;
                } else if (meets(current.last(), candidate.last())) {
                    joined = reversedCopy(candidate);
                    atEnd = true;
                } else if (meets(current.first(), candidate.last())) {
                    joined = candidate;
                } else if (meets(current.first(), candidate.first())) {
                    joined = reversedCopy(candidate);
                } else {
                    continue;
                }
                if (atEnd) {
                    for (int k = 1; k < joined.size(); ++k)
                        current.append(joined.at(k));
                } else {
                    QVector<Point2D> prefix;
                    for (int k = 0; k < joined.size() - 1; ++k)
                        prefix.append(joined.at(k));
                    current = prefix + current;
                }
                used[i] = true;
                extended = true;
                break;
            }
        }

        // A chain whose ends meet is a closed loop. One that does not is left
        // open, so nothing claims the outline is closed when it is not.
        if (current.size() >= 3 && !same(current.first(), current.last())
            && meets(current.first(), current.last())) {
            current.append(current.first());
        }
        loops.append(current);
    }
    return loops;
}

QVector<Point2D> arcToPolygon(const Point2D &start, const Point2D &mid, const Point2D &end)
{
    // Circle through three points; the arc runs from start to end through mid.
    const double ax = start.x;
    const double ay = start.y;
    const double bx = mid.x;
    const double by = mid.y;
    const double cx = end.x;
    const double cy = end.y;
    const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::abs(d) < 1e-9) { // collinear: a straight line is an honest answer
        return { start, end };
    }
    const double ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay)
                       + (cx * cx + cy * cy) * (ay - by)) / d;
    const double uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx)
                       + (cx * cx + cy * cy) * (bx - ax)) / d;
    const double radius = std::hypot(ax - ux, ay - uy);

    double startAngle = std::atan2(ay - uy, ax - ux);
    const double midAngle = std::atan2(by - uy, bx - ux);
    double endAngle = std::atan2(cy - uy, cx - ux);

    double sweep = endAngle - startAngle;
    // Take the direction that passes through the mid point.
    while (std::abs(sweep) < 1e-9)
        sweep = 2.0 * kPi;
    double throughMid = midAngle - startAngle;
    while (throughMid < 0.0)
        throughMid += 2.0 * kPi;
    while (sweep < 0.0)
        sweep += 2.0 * kPi;
    if (throughMid > sweep)
        sweep -= 2.0 * kPi;

    const int steps = qMax(2, int(std::ceil(std::abs(sweep) / (2.0 * kPi) * kCircleSegments)));
    QVector<Point2D> points;
    for (int i = 0; i <= steps; ++i) {
        const double angle = startAngle + sweep * double(i) / double(steps);
        points.append({ ux + radius * std::cos(angle), uy + radius * std::sin(angle) });
    }
    return points;
}

QVector<Point2D> angleArcToPolygon(const Point2D &start, const Point2D &end, double angleDegrees)
{
    // The older form of an arc: its two ends and the angle it covers. A positive
    // angle runs counter-clockwise in the file's own coordinates (x to the right,
    // y downwards), which is what KiCad wrote.
    const double sweep = angleDegrees * kPi / 180.0;
    const double chord = std::hypot(end.x - start.x, end.y - start.y);
    const double sinHalf = std::sin(sweep / 2.0);
    if (chord <= 0.0 || std::abs(sinHalf) < 1e-9)
        return { start, end };

    // Both the radius and the distance of the centre from the middle of the
    // chord are signed, so the sign of the angle alone decides on which side of
    // the chord the centre lies.
    const double radius = chord / (2.0 * sinHalf);
    const double height = radius * std::cos(sweep / 2.0);
    const Point2D middle{ (start.x + end.x) / 2.0, (start.y + end.y) / 2.0 };
    const double dx = (end.x - start.x) / chord;
    const double dy = (end.y - start.y) / chord;
    const Point2D center{ middle.x - dy * height, middle.y + dx * height };

    const double startAngle = std::atan2(start.y - center.y, start.x - center.x);
    const int steps = qMax(2, int(std::ceil(std::abs(sweep) / (2.0 * kPi) * kCircleSegments)));
    QVector<Point2D> points;
    for (int i = 0; i <= steps; ++i) {
        const double a = startAngle + sweep * double(i) / double(steps);
        points.append({ center.x + std::abs(radius) * std::cos(a),
                        center.y + std::abs(radius) * std::sin(a) });
    }
    return points;
}

// Which side of the board a layer name means, and what kind of layer it is.
// Both are read from the name KiCad uses, which is the only place the file says
// it; nothing is guessed from the layer number.
BoardSide sideOfLayerName(const QString &name, LayerType *type = nullptr)
{
    QString canonical = name.trimmed();
    LayerType detected = LayerType::Other;
    BoardSide side = BoardSide::Unknown;

    if (canonical.compare(QStringLiteral("Edge.Cuts"), Qt::CaseInsensitive) == 0) {
        detected = LayerType::Edge;
    } else if (canonical.endsWith(QLatin1String(".Cu"), Qt::CaseInsensitive)) {
        detected = LayerType::Copper;
    } else if (canonical.contains(QLatin1String("SilkS"), Qt::CaseInsensitive)
               || canonical.contains(QLatin1String("Silkscreen"), Qt::CaseInsensitive)) {
        detected = LayerType::Silkscreen;
    } else if (canonical.contains(QLatin1String("Mask"), Qt::CaseInsensitive)) {
        detected = LayerType::SolderMask;
    } else if (canonical.contains(QLatin1String("Paste"), Qt::CaseInsensitive)) {
        detected = LayerType::Paste;
    } else if (canonical.contains(QLatin1String("Fab"), Qt::CaseInsensitive)) {
        detected = LayerType::Fabrication;
    } else if (canonical.contains(QLatin1String("CrtYd"), Qt::CaseInsensitive)
               || canonical.contains(QLatin1String("Adhes"), Qt::CaseInsensitive)
               || canonical.contains(QLatin1String("User"), Qt::CaseInsensitive)
               || canonical.contains(QLatin1String("Eco"), Qt::CaseInsensitive)
               || canonical.contains(QLatin1String("Margin"), Qt::CaseInsensitive)
               || canonical.contains(QLatin1String("Comment"), Qt::CaseInsensitive)
               || canonical.contains(QLatin1String("Drawing"), Qt::CaseInsensitive)) {
        detected = LayerType::Other;
    }

    if (canonical.startsWith(QLatin1String("F."), Qt::CaseInsensitive))
        side = BoardSide::Top;
    else if (canonical.startsWith(QLatin1String("B."), Qt::CaseInsensitive))
        side = BoardSide::Bottom;

    if (type)
        *type = detected;
    return side;
}

BoardSide sideOfPadLayers(const SExpression *pad)
{
    const SExpression *layers = pad ? pad->child(QStringLiteral("layers")) : nullptr;
    if (!layers)
        return BoardSide::Unknown;
    bool front = false;
    bool back = false;
    for (int i = 1; i < layers->children().size(); ++i) {
        const QString name = layers->atomAt(i);
        if (name.startsWith(QLatin1String("F."), Qt::CaseInsensitive)
            || name.compare(QLatin1String("*.Cu"), Qt::CaseInsensitive) == 0)
            front = true;
        else if (name.startsWith(QLatin1String("B."), Qt::CaseInsensitive))
            back = true;
    }
    if (front)
        return BoardSide::Top;
    if (back)
        return BoardSide::Bottom;
    return BoardSide::Unknown;
}

QString netNameOf(const QHash<int, QString> &netTable, const SExpression *node)
{
    const SExpression *net = node ? node->child(QStringLiteral("net")) : nullptr;
    if (!net)
        return QString();
    const QString name = net->atomAt(2);
    if (!name.isEmpty())
        return name;
    bool ok = false;
    const int index = net->atomAt(1).toInt(&ok);
    if (!ok)
        return QString();
    return netTable.value(index);
}

Pad parsePad(const SExpression *node, const QHash<int, QString> &netTable, int fallbackIndex)
{
    Pad pad;
    if (!node)
        return pad;

    const QString number = node->atomAt(1);
    const SExpression *uuid = node->child(QStringLiteral("uuid"));
    // The id is our own handle; the pin is what the file calls the pin. A pad
    // without a number (a thermal pad, for example) keeps an empty pin and is
    // still addressable through the id.
    pad.id = number.isEmpty()
                 ? (uuid ? uuid->atomAt(1) : QStringLiteral("pad-%1").arg(fallbackIndex))
                 : number;
    pad.pin = number;

    double rotation = 0.0;
    pointOf(node, "at", &pad.position, &rotation);
    pad.rotation = rotation;

    const SExpression *size = node->child(QStringLiteral("size"));
    if (size) {
        size->numberAt(1, &pad.size.width);
        size->numberAt(2, &pad.size.height);
    }

    // The shape is the second or third atom: (pad "1" smd rect ...) in KiCad 7
    // and later, (pad 1 smd rect ...) before that.
    for (int i = 2; i <= 4 && i < node->children().size(); ++i) {
        const PadShape shape = padShapeFromString(node->atomAt(i));
        if (shape != PadShape::Unknown) {
            pad.shape = shape;
            break;
        }
    }

    pad.side = sideOfPadLayers(node);
    pad.netName = netNameOf(netTable, node);

    const SExpression *pinFunction = node->child(QStringLiteral("pinfunction"));
    if (pinFunction)
        pad.pinName = pinFunction->atomAt(1);

    return pad;
}

// What a whole board's footprints contained but could not be imported. Counted
// over the board, so the user gets one line per kind instead of one per part.
struct FootprintTallies
{
    int drawings = 0;   // silkscreen, courtyard, fabrication outlines
    int models = 0;     // 3D models
};

Component parseFootprint(const SExpression *node, const QHash<int, QString> &netTable,
                         const Provenance &provenance, int fallbackIndex,
                         FootprintTallies &tallies)
{
    Component component;
    component.provenance = provenance;
    component.hasGeometry = false;

    component.footprint = node->atomAt(1);

    // KiCad 9 calls it (uuid ...), KiCad 6 to 8 call it (tstamp ...). Both are
    // the file's own identity for the footprint, which is what we keep so that
    // notes attached to a component survive a re-import.
    const SExpression *uuid = node->child(QStringLiteral("uuid"));
    if (!uuid)
        uuid = node->child(QStringLiteral("tstamp"));
    component.uid = uuid ? uuid->atomAt(1) : QStringLiteral("footprint-%1").arg(fallbackIndex);
    if (component.uid.isEmpty())
        component.uid = QStringLiteral("footprint-%1").arg(fallbackIndex);

    const QString layer = layerNameOf(node);
    if (!layer.isEmpty())
        component.side = sideOfLayerName(layer);
    else
        component.side = BoardSide::Unknown;

    double rotation = 0.0;
    if (pointOf(node, "at", &component.position, &rotation))
        component.hasGeometry = true;
    component.rotation = rotation;

    for (const SExpression *property : node->children(QStringLiteral("property"))) {
        const QString name = property->atomAt(1);
        const QString value = property->atomAt(2);
        if (name.compare(QLatin1String("Reference"), Qt::CaseInsensitive) == 0)
            component.reference = value;
        else if (name.compare(QLatin1String("Value"), Qt::CaseInsensitive) == 0)
            component.value = value;
        else if (name.compare(QLatin1String("Description"), Qt::CaseInsensitive) == 0
                 || name.compare(QLatin1String("descr"), Qt::CaseInsensitive) == 0)
            component.description = value;
        else if (name.compare(QLatin1String("Manufacturer"), Qt::CaseInsensitive) == 0)
            component.manufacturer = value;
        else if (name.compare(QLatin1String("MPN"), Qt::CaseInsensitive) == 0
                 || name.compare(QLatin1String("PartNumber"), Qt::CaseInsensitive) == 0
                 || name.compare(QLatin1String("Manufacturer_Part_Number"), Qt::CaseInsensitive) == 0)
            component.manufacturerPartNumber = value;
        else if (name.compare(QLatin1String("Datasheet"), Qt::CaseInsensitive) == 0)
            component.datasheetUrl = value;
    }

    const SExpression *descr = node->child(QStringLiteral("descr"));
    if (descr && component.description.isEmpty())
        component.description = descr->atomAt(1);

    // KiCad 6, 7 and 8 print the reference and the value as text on the
    // footprint; KiCad 9 and later moved them into properties. Both are read,
    // and a property wins when both are there.
    for (const SExpression *text : node->children(QStringLiteral("fp_text"))) {
        const QString kind = text->atomAt(1);
        const QString value = text->atomAt(2);
        if (kind.compare(QLatin1String("reference"), Qt::CaseInsensitive) == 0) {
            if (component.reference.isEmpty())
                component.reference = value;
        } else if (kind.compare(QLatin1String("value"), Qt::CaseInsensitive) == 0) {
            if (component.value.isEmpty())
                component.value = value;
        }
    }

    int padIndex = 0;
    for (const SExpression *padNode : node->children(QStringLiteral("pad"))) {
        component.pads.append(parsePad(padNode, netTable, padIndex));
        ++padIndex;
    }
    component.pinCount = component.pads.size();

    // Nets this component is connected to, in the order the pads appear.
    QStringList nets;
    for (const Pad &pad : component.pads) {
        if (!pad.netName.isEmpty() && !nets.contains(pad.netName))
            nets.append(pad.netName);
    }
    component.nets = nets;

    tallies.models += node->children(QStringLiteral("model")).size();

    // The drawings of a footprint are its silkscreen, courtyard and fabrication
    // outlines. The model describes a component by its pads, so they are
    // counted here and reported once for the whole board.
    for (const char *head : { "fp_line", "fp_rect", "fp_circle", "fp_arc", "fp_poly",
                              "fp_text", "fp_text_box", "fp_curve" }) {
        tallies.drawings += node->children(QString::fromLatin1(head)).size();
    }

    return component;
}

} // namespace

KicadImportResult importKicadPcbText(const QString &text, const QString &sourcePath,
                                     QString *error)
{
    KicadImportResult result;

    SExpression root;
    QString parseError;
    if (!parseSExpression(text, &root, &parseError)) {
        if (error)
            *error = parseError;
        return result;
    }
    if (!root.isList() || root.head() != QLatin1String("kicad_pcb")) {
        if (error)
            *error = QStringLiteral("This is not a KiCad board file: the first expression is "
                                    "\"%1\" where \"kicad_pcb\" was expected.")
                         .arg(root.head().isEmpty() ? QStringLiteral("(empty)")
                                                    : root.head());
        return result;
    }

    Board board;
    board.uid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    board.sourcePath = sourcePath;
    board.sourceFormat = QStringLiteral("kicad-pcb");
    // The board is called what the file's title block calls it; the file name is
    // only a fallback, because a file can be renamed and a board cannot.
    board.name = QFileInfo(sourcePath).completeBaseName();
    const SExpression *titleBlock = root.child(QStringLiteral("title_block"));
    const SExpression *title = titleBlock ? titleBlock->child(QStringLiteral("title"))
                                          : root.child(QStringLiteral("title"));
    if (title) {
        const QString given = title->atomAt(1).trimmed();
        if (!given.isEmpty())
            board.name = given;
    }

    WarningLog warnings;
    Bounds bounds;
    Bounds outlineBounds;

    // --- format version and generator --------------------------------------
    const SExpression *version = root.child(QStringLiteral("version"));
    if (version)
        result.formatVersion = version->atomAt(1);
    const SExpression *generator = root.child(QStringLiteral("generator"));
    if (generator) {
        QStringList parts;
        for (int i = 1; i < generator->children().size(); ++i) {
            const QString atom = generator->atomAt(i);
            if (!atom.isEmpty())
                parts.append(atom);
        }
        result.generator = parts.join(QLatin1Char(' ')).trimmed();
    }

    // --- layers -------------------------------------------------------------
    const SExpression *layers = root.child(QStringLiteral("layers"));
    if (layers) {
        for (const SExpression &entry : layers->children()) {
            if (!entry.isList())
                continue;
            // "(0 \"F.Cu\" signal)": the layer number is the head of the list.
            const QString name = entry.atomAt(1);
            if (name.isEmpty())
                continue;
            Layer layer;
            layer.sourceName = name;
            layer.name = name;
            bool ok = false;
            const int index = entry.atomAt(0).toInt(&ok);
            layer.index = ok ? index : int(board.layers.size());
            LayerType type = LayerType::Other;
            layer.side = sideOfLayerName(name, &type);
            layer.type = type;
            board.layers.append(layer);
        }
    } else {
        warnings.add(QStringLiteral("The file declares no layers."));
    }

    // --- net table ----------------------------------------------------------
    QHash<int, QString> netTable;
    for (const SExpression *net : root.children(QStringLiteral("net"))) {
        bool ok = false;
        const int index = net->atomAt(1).toInt(&ok);
        if (!ok)
            continue;
        const QString name = net->atomAt(2);
        netTable.insert(index, name);
        if (name.isEmpty())
            continue; // index 0 is "not connected"; it is not a net
        Net modelNet;
        modelNet.name = name;
        modelNet.confirmed = true; // read from the board's own net list
        modelNet.provenance.status = VerificationStatus::ExtractedFromSource;
        modelNet.provenance.valueSource = ValueSource::ExtractedFromSource;
        modelNet.provenance.localPath = sourcePath;
        modelNet.provenance.location = QStringLiteral("net %1").arg(index);
        board.nets.append(modelNet);
    }

    // --- footprints ---------------------------------------------------------
    Provenance componentProvenance;
    componentProvenance.title = board.name;
    componentProvenance.localPath = sourcePath;
    componentProvenance.status = VerificationStatus::ExtractedFromSource;
    componentProvenance.valueSource = ValueSource::ExtractedFromSource;
    componentProvenance.retrieved = QDateTime::currentDateTimeUtc();

    QSet<QString> usedUids;
    FootprintTallies tallies;
    int footprintIndex = 0;
    int renamedFootprints = 0;
    for (const SExpression *footprint : root.children(QStringLiteral("footprint"))) {
        Component component = parseFootprint(footprint, netTable, componentProvenance,
                                             footprintIndex, tallies);
        if (usedUids.contains(component.uid)) {
            component.uid = QStringLiteral("%1-%2").arg(component.uid).arg(footprintIndex);
            ++renamedFootprints;
        }
        usedUids.insert(component.uid);
        board.components.append(component);
        result.padCount += component.pads.size();
        ++footprintIndex;
    }
    result.componentCount = board.components.size();
    if (tallies.models > 0) {
        warnings.addCounted(tallies.models,
                            QStringLiteral("A 3D model attached to a footprint was ignored."),
                            QStringLiteral("%1 3D models attached to footprints were ignored."));
    }
    if (tallies.drawings > 0) {
        warnings.addCounted(tallies.drawings,
                            QStringLiteral("1 drawing element of a footprint (silkscreen, "
                                           "courtyard or fabrication outline) was skipped: the "
                                           "model describes a component by its pads, not by the "
                                           "drawings around them."),
                            QStringLiteral("%1 drawing elements of footprints (silkscreen, "
                                           "courtyard and fabrication outlines) were skipped: the "
                                           "model describes a component by its pads, not by the "
                                           "drawings around them."));
    }
    if (renamedFootprints > 0) {
        warnings.addCounted(renamedFootprints,
                            QStringLiteral("One footprint had an identifier that was already in "
                                           "use; it was given a new one."),
                            QStringLiteral("%1 footprints had identifiers that were already in "
                                           "use; they were given new ones."));
    }

    // --- tracks and vias ----------------------------------------------------
    for (const SExpression *segment : root.children(QStringLiteral("segment"))) {
        Point2D start;
        Point2D end;
        if (!pointOf(segment, "start", &start) || !pointOf(segment, "end", &end))
            continue;
        Track track;
        track.points = { start, end };
        const SExpression *width = segment->child(QStringLiteral("width"));
        if (width)
            width->numberAt(1, &track.width);
        track.layerName = layerNameOf(segment);
        track.netName = netNameOf(netTable, segment);
        board.tracks.append(track);
        bounds.add(start);
        bounds.add(end);
    }
    result.trackCount = board.tracks.size();

    for (const SExpression *viaNode : root.children(QStringLiteral("via"))) {
        Via via;
        if (!pointOf(viaNode, "at", &via.position))
            continue;
        const SExpression *size = viaNode->child(QStringLiteral("size"));
        if (size)
            size->numberAt(1, &via.diameter);
        const SExpression *drill = viaNode->child(QStringLiteral("drill"));
        if (drill)
            drill->numberAt(1, &via.drill);
        via.netName = netNameOf(netTable, viaNode);
        board.vias.append(via);
        bounds.add(via.position);
    }
    result.viaCount = board.vias.size();

    // --- graphics: the board outline, and everything else -------------------
    // One pass over the file, so the board outline comes out in the order the
    // file lists it.
    int edgeCuts = 0;
    int approximated = 0;
    int skippedGraphics = 0;
    int zones = 0;
    QMap<QString, int> unknownElements;

    QVector<QVector<Point2D>> outlinePieces;
    auto outline = [&](const QVector<Point2D> &points) {
        if (points.size() < 2)
            return;
        outlinePieces.append(points);
        ++edgeCuts;
        for (const Point2D &p : points) {
            outlineBounds.add(p);
            bounds.add(p);
        }
    };

    const auto isEdge = [](const QString &layer) {
        return layer.compare(QLatin1String("Edge.Cuts"), Qt::CaseInsensitive) == 0;
    };

    // Everything that was already dealt with above; it must not show up as
    // "unknown" below.
    static const QSet<QString> alreadyHandled = {
        QStringLiteral("version"), QStringLiteral("generator"), QStringLiteral("general"),
        QStringLiteral("paper"), QStringLiteral("setup"), QStringLiteral("properties"),
        QStringLiteral("title_block"), QStringLiteral("uuid"), QStringLiteral("layers"),
        QStringLiteral("net"), QStringLiteral("footprint"), QStringLiteral("segment"),
        QStringLiteral("via"), QStringLiteral("title")
    };

    for (const SExpression &child : root.children()) {
        if (!child.isList())
            continue;
        const QString head = child.head();
        const QString layer = layerNameOf(&child);

        if (head == QLatin1String("gr_line")) {
            Point2D start;
            Point2D end;
            if (pointOf(&child, "start", &start) && pointOf(&child, "end", &end)) {
                if (isEdge(layer))
                    outline({ start, end });
                else
                    ++skippedGraphics;
            }
        } else if (head == QLatin1String("gr_rect")) {
            Point2D start;
            Point2D end;
            if (pointOf(&child, "start", &start) && pointOf(&child, "end", &end)) {
                if (isEdge(layer)) {
                    outline({ { start.x, start.y }, { end.x, start.y }, { end.x, end.y },
                              { start.x, end.y }, { start.x, start.y } });
                } else {
                    ++skippedGraphics;
                }
            }
        } else if (head == QLatin1String("gr_circle")) {
            Point2D center;
            Point2D end;
            if (pointOf(&child, "center", &center) && pointOf(&child, "end", &end)) {
                if (isEdge(layer)) {
                    outline(circleToPolygon(center, std::hypot(end.x - center.x, end.y - center.y)));
                    ++approximated;
                } else {
                    ++skippedGraphics;
                }
            }
        } else if (head == QLatin1String("gr_arc")) {
            Point2D start;
            Point2D end;
            Point2D mid;
            if (pointOf(&child, "start", &start) && pointOf(&child, "end", &end)) {
                if (isEdge(layer)) {
                    if (pointOf(&child, "mid", &mid)) {
                        outline(arcToPolygon(start, mid, end));
                    } else {
                        const SExpression *angle = child.child(QStringLiteral("angle"));
                        double degrees = 0.0;
                        if (angle && angle->numberAt(1, &degrees))
                            outline(angleArcToPolygon(start, end, degrees));
                        else
                            outline({ start, end });
                    }
                    ++approximated;
                } else {
                    ++skippedGraphics;
                }
            }
        } else if (head == QLatin1String("gr_poly")) {
            const QVector<Point2D> points = pointsOfPolygon(&child);
            if (isEdge(layer))
                outline(points);
            else
                ++skippedGraphics;
        } else if (head == QLatin1String("zone")) {
            ++zones;
        } else if (alreadyHandled.contains(head)) {
            continue;
        } else {
            ++unknownElements[head];
        }
    }

    // The outline pieces are joined into loops before they are handed on, so
    // that the board can be drawn as one filled shape.
    board.outlines = chainOutlines(outlinePieces);

    // --- bounding box -------------------------------------------------------
    if (outlineBounds.valid) {
        board.boundingBox = outlineBounds.toRect();
    } else if (bounds.valid) {
        board.boundingBox = bounds.toRect();
        warnings.add(QStringLiteral(
            "The board has no Edge.Cuts outline, so its outline is unknown. The view falls "
            "back to the extent of the imported copper and footprints."));
    } else {
        warnings.add(QStringLiteral("The file contains no board geometry at all."));
    }

    // --- warnings, in plain language ---------------------------------------
    if (zones > 0) {
        warnings.addCounted(zones,
                            QStringLiteral("1 copper zone (a filled area) was skipped: the model "
                                           "does not carry copper pours yet."),
                            QStringLiteral("%1 copper zones (filled areas) were skipped: the "
                                           "model does not carry copper pours yet."));
    }
    if (skippedGraphics > 0) {
        warnings.add(QStringLiteral(
                         "%1 board drawing(s) outside the board outline (silkscreen, fabrication, "
                         "courtyard, user drawings) were skipped; they carry no connectivity.")
                         .arg(skippedGraphics));
    }
    if (approximated > 0) {
        warnings.addCounted(approximated,
                            QStringLiteral("1 arc or circle of the board outline is drawn as a "
                                           "polyline with %2 segments.")
                                .arg(kCircleSegments),
                            QStringLiteral("%1 arcs and circles of the board outline are drawn as "
                                           "polylines with %2 segments each.")
                                .arg(kCircleSegments));
    }
    for (auto it = unknownElements.cbegin(); it != unknownElements.cend(); ++it) {
        if (it.value() == 1) {
            warnings.add(QStringLiteral("The element \"%1\" was skipped: it is not imported.")
                             .arg(it.key()));
        } else {
            warnings.add(QStringLiteral("%2 elements of type \"%1\" were skipped: they are not "
                                        "imported.")
                             .arg(it.key())
                             .arg(it.value()));
        }
    }

    board.warnings = warnings.messages();
    result.warnings = board.warnings;
    result.board = board;
    result.ok = true;
    return result;
}

QString kicadBoardFileFilter()
{
    return QStringLiteral("KiCad board files (*.kicad_pcb);;All files (*)");
}

KicadImportResult importKicadPcb(const QString &path, QString *error)
{
    KicadImportResult result;

    QFile file(path);
    if (!file.exists()) {
        if (error)
            *error = QStringLiteral("The file %1 does not exist.").arg(path);
        return result;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("The file %1 cannot be opened for reading.").arg(path);
        return result;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString text = stream.readAll();
    file.close();

    if (text.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("The file %1 is empty.").arg(path);
        return result;
    }

    result = importKicadPcbText(text, QFileInfo(path).absoluteFilePath(), error);
    return result;
}

} // namespace eda
