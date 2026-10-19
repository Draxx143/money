#include "ProjectModel.h"

#include <QJsonArray>

namespace eda {
namespace {

QJsonObject pointToJson(const Point2D &p)
{
    QJsonObject o;
    o.insert(QStringLiteral("x"), p.x);
    o.insert(QStringLiteral("y"), p.y);
    return o;
}

Point2D pointFromJson(const QJsonObject &o)
{
    Point2D p;
    p.x = o.value(QStringLiteral("x")).toDouble();
    p.y = o.value(QStringLiteral("y")).toDouble();
    return p;
}

QJsonObject sizeToJson(const Size2D &s)
{
    QJsonObject o;
    o.insert(QStringLiteral("width"), s.width);
    o.insert(QStringLiteral("height"), s.height);
    return o;
}

Size2D sizeFromJson(const QJsonObject &o)
{
    Size2D s;
    s.width = o.value(QStringLiteral("width")).toDouble();
    s.height = o.value(QStringLiteral("height")).toDouble();
    return s;
}

QJsonArray stringListToJson(const QStringList &list) { return QJsonArray::fromStringList(list); }

QStringList stringListFromJson(const QJsonValue &value)
{
    QStringList list;
    const QJsonArray array = value.toArray();
    list.reserve(array.size());
    for (const QJsonValue &item : array)
        list.append(item.toString());
    return list;
}

template <typename T>
QJsonArray arrayToJson(const QVector<T> &items)
{
    QJsonArray array;
    for (const T &item : items)
        array.append(item.toJson());
    return array;
}

template <typename T>
QVector<T> arrayFromJson(const QJsonValue &value)
{
    QVector<T> items;
    const QJsonArray array = value.toArray();
    items.reserve(array.size());
    for (const QJsonValue &item : array)
        items.append(T::fromJson(item.toObject()));
    return items;
}

} // namespace

// ---------------------------------------------------------------------------
// enum <-> string
// ---------------------------------------------------------------------------
QString toString(BoardSide side)
{
    switch (side) {
    case BoardSide::Top: return QStringLiteral("top");
    case BoardSide::Bottom: return QStringLiteral("bottom");
    case BoardSide::Unknown: break;
    }
    return QStringLiteral("unknown");
}

BoardSide boardSideFromString(const QString &text)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("top") || t == QLatin1String("front") || t == QLatin1String("f.cu"))
        return BoardSide::Top;
    if (t == QLatin1String("bottom") || t == QLatin1String("back") || t == QLatin1String("b.cu"))
        return BoardSide::Bottom;
    return BoardSide::Unknown;
}

QString toString(ComponentCategory category)
{
    switch (category) {
    case ComponentCategory::Resistor: return QStringLiteral("resistor");
    case ComponentCategory::Capacitor: return QStringLiteral("capacitor");
    case ComponentCategory::Inductor: return QStringLiteral("inductor");
    case ComponentCategory::Diode: return QStringLiteral("diode");
    case ComponentCategory::Transistor: return QStringLiteral("transistor");
    case ComponentCategory::IntegratedCircuit: return QStringLiteral("ic");
    case ComponentCategory::Connector: return QStringLiteral("connector");
    case ComponentCategory::Crystal: return QStringLiteral("crystal");
    case ComponentCategory::Fuse: return QStringLiteral("fuse");
    case ComponentCategory::Relay: return QStringLiteral("relay");
    case ComponentCategory::Transformer: return QStringLiteral("transformer");
    case ComponentCategory::Led: return QStringLiteral("led");
    case ComponentCategory::Switch: return QStringLiteral("switch");
    case ComponentCategory::TestPoint: return QStringLiteral("testpoint");
    case ComponentCategory::Mechanical: return QStringLiteral("mechanical");
    case ComponentCategory::Other: return QStringLiteral("other");
    case ComponentCategory::Unknown: break;
    }
    return QStringLiteral("unknown");
}

ComponentCategory componentCategoryFromString(const QString &text)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("resistor") || t == QLatin1String("r")) return ComponentCategory::Resistor;
    if (t == QLatin1String("capacitor") || t == QLatin1String("c")) return ComponentCategory::Capacitor;
    if (t == QLatin1String("inductor") || t == QLatin1String("l")) return ComponentCategory::Inductor;
    if (t == QLatin1String("diode") || t == QLatin1String("d")) return ComponentCategory::Diode;
    if (t == QLatin1String("transistor") || t == QLatin1String("q")) return ComponentCategory::Transistor;
    if (t == QLatin1String("ic") || t == QLatin1String("u") || t.startsWith(QLatin1String("integrated")))
        return ComponentCategory::IntegratedCircuit;
    if (t == QLatin1String("connector") || t == QLatin1String("j") || t == QLatin1String("p"))
        return ComponentCategory::Connector;
    if (t == QLatin1String("crystal") || t == QLatin1String("y")) return ComponentCategory::Crystal;
    if (t == QLatin1String("fuse") || t == QLatin1String("f")) return ComponentCategory::Fuse;
    if (t == QLatin1String("relay") || t == QLatin1String("k")) return ComponentCategory::Relay;
    if (t == QLatin1String("transformer") || t == QLatin1String("t")) return ComponentCategory::Transformer;
    if (t == QLatin1String("led")) return ComponentCategory::Led;
    if (t == QLatin1String("switch") || t == QLatin1String("s")) return ComponentCategory::Switch;
    if (t == QLatin1String("testpoint") || t == QLatin1String("tp")) return ComponentCategory::TestPoint;
    if (t == QLatin1String("mechanical")) return ComponentCategory::Mechanical;
    if (t == QLatin1String("other")) return ComponentCategory::Other;
    return ComponentCategory::Unknown;
}

QString toString(LayerType type)
{
    switch (type) {
    case LayerType::Copper: return QStringLiteral("copper");
    case LayerType::Silkscreen: return QStringLiteral("silkscreen");
    case LayerType::SolderMask: return QStringLiteral("soldermask");
    case LayerType::Paste: return QStringLiteral("paste");
    case LayerType::Edge: return QStringLiteral("edge");
    case LayerType::Fabrication: return QStringLiteral("fabrication");
    case LayerType::Other: return QStringLiteral("other");
    case LayerType::Unknown: break;
    }
    return QStringLiteral("unknown");
}

LayerType layerTypeFromString(const QString &text)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("copper")) return LayerType::Copper;
    if (t == QLatin1String("silkscreen")) return LayerType::Silkscreen;
    if (t == QLatin1String("soldermask")) return LayerType::SolderMask;
    if (t == QLatin1String("paste")) return LayerType::Paste;
    if (t == QLatin1String("edge")) return LayerType::Edge;
    if (t == QLatin1String("fabrication")) return LayerType::Fabrication;
    if (t == QLatin1String("other")) return LayerType::Other;
    return LayerType::Unknown;
}

QString toString(VerificationStatus status)
{
    switch (status) {
    case VerificationStatus::NeedsReview: return QStringLiteral("needs-review");
    case VerificationStatus::Unverified: return QStringLiteral("unverified");
    case VerificationStatus::AiSuggested: return QStringLiteral("ai-suggested");
    case VerificationStatus::Inferred: return QStringLiteral("inferred");
    case VerificationStatus::ExtractedFromSource: return QStringLiteral("extracted");
    case VerificationStatus::UserVerified: return QStringLiteral("user-verified");
    case VerificationStatus::ExperimentallyVerified: return QStringLiteral("experimentally-verified");
    case VerificationStatus::OfficialSourceVerified: return QStringLiteral("official-source");
    case VerificationStatus::Corroborated: return QStringLiteral("corroborated");
    case VerificationStatus::ConflictingSources: return QStringLiteral("conflicting");
    case VerificationStatus::Unknown: break;
    }
    return QStringLiteral("unknown");
}

VerificationStatus verificationStatusFromString(const QString &text)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("needs-review")) return VerificationStatus::NeedsReview;
    if (t == QLatin1String("unverified")) return VerificationStatus::Unverified;
    if (t == QLatin1String("ai-suggested")) return VerificationStatus::AiSuggested;
    if (t == QLatin1String("inferred")) return VerificationStatus::Inferred;
    if (t == QLatin1String("extracted")) return VerificationStatus::ExtractedFromSource;
    if (t == QLatin1String("user-verified")) return VerificationStatus::UserVerified;
    if (t == QLatin1String("experimentally-verified")) return VerificationStatus::ExperimentallyVerified;
    if (t == QLatin1String("official-source")) return VerificationStatus::OfficialSourceVerified;
    if (t == QLatin1String("corroborated")) return VerificationStatus::Corroborated;
    if (t == QLatin1String("conflicting")) return VerificationStatus::ConflictingSources;
    return VerificationStatus::Unknown;
}

QString toString(ValueSource source)
{
    switch (source) {
    case ValueSource::ExtractedFromSource: return QStringLiteral("extracted");
    case ValueSource::ManualEntry: return QStringLiteral("manual");
    case ValueSource::Measured: return QStringLiteral("measured");
    case ValueSource::AiSuggested: return QStringLiteral("ai-suggested");
    case ValueSource::Inferred: return QStringLiteral("inferred");
    case ValueSource::Unknown: break;
    }
    return QStringLiteral("unknown");
}

ValueSource valueSourceFromString(const QString &text)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("extracted")) return ValueSource::ExtractedFromSource;
    if (t == QLatin1String("manual")) return ValueSource::ManualEntry;
    if (t == QLatin1String("measured")) return ValueSource::Measured;
    if (t == QLatin1String("ai-suggested")) return ValueSource::AiSuggested;
    if (t == QLatin1String("inferred")) return ValueSource::Inferred;
    return ValueSource::Unknown;
}

QString toString(MeasurementStatus status)
{
    switch (status) {
    case MeasurementStatus::NotTested: return QStringLiteral("not-tested");
    case MeasurementStatus::Pass: return QStringLiteral("pass");
    case MeasurementStatus::Fail: return QStringLiteral("fail");
    case MeasurementStatus::Unknown: break;
    }
    return QStringLiteral("unknown");
}

MeasurementStatus measurementStatusFromString(const QString &text)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("not-tested")) return MeasurementStatus::NotTested;
    if (t == QLatin1String("pass")) return MeasurementStatus::Pass;
    if (t == QLatin1String("fail")) return MeasurementStatus::Fail;
    return MeasurementStatus::Unknown;
}

QString toString(HypothesisStatus status)
{
    switch (status) {
    case HypothesisStatus::Possible: return QStringLiteral("possible");
    case HypothesisStatus::Plausible: return QStringLiteral("plausible");
    case HypothesisStatus::StronglySupported: return QStringLiteral("strongly-supported");
    case HypothesisStatus::ConfirmedByMeasurement: return QStringLiteral("confirmed-measurement");
    case HypothesisStatus::ConfirmedByRepair: return QStringLiteral("confirmed-repair");
    case HypothesisStatus::RuledOut: return QStringLiteral("ruled-out");
    case HypothesisStatus::Unresolved: break;
    }
    return QStringLiteral("unresolved");
}

HypothesisStatus hypothesisStatusFromString(const QString &text)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("possible")) return HypothesisStatus::Possible;
    if (t == QLatin1String("plausible")) return HypothesisStatus::Plausible;
    if (t == QLatin1String("strongly-supported")) return HypothesisStatus::StronglySupported;
    if (t == QLatin1String("confirmed-measurement")) return HypothesisStatus::ConfirmedByMeasurement;
    if (t == QLatin1String("confirmed-repair")) return HypothesisStatus::ConfirmedByRepair;
    if (t == QLatin1String("ruled-out")) return HypothesisStatus::RuledOut;
    return HypothesisStatus::Unresolved;
}

// ---------------------------------------------------------------------------
// Provenance
// ---------------------------------------------------------------------------
bool Provenance::isEmpty() const
{
    return sourceId.isEmpty() && title.isEmpty() && url.isEmpty() && localPath.isEmpty()
        && statement.isEmpty() && notes.isEmpty();
}

QJsonObject Provenance::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("sourceId"), sourceId);
    o.insert(QStringLiteral("title"), title);
    o.insert(QStringLiteral("publisher"), publisher);
    o.insert(QStringLiteral("url"), url);
    o.insert(QStringLiteral("localPath"), localPath);
    o.insert(QStringLiteral("location"), location);
    o.insert(QStringLiteral("statement"), statement);
    o.insert(QStringLiteral("retrieved"), retrieved.toString(Qt::ISODate));
    o.insert(QStringLiteral("status"), eda::toString(status));
    o.insert(QStringLiteral("valueSource"), eda::toString(valueSource));
    o.insert(QStringLiteral("notes"), notes);
    return o;
}

Provenance Provenance::fromJson(const QJsonObject &json)
{
    Provenance p;
    p.sourceId = json.value(QStringLiteral("sourceId")).toString();
    p.title = json.value(QStringLiteral("title")).toString();
    p.publisher = json.value(QStringLiteral("publisher")).toString();
    p.url = json.value(QStringLiteral("url")).toString();
    p.localPath = json.value(QStringLiteral("localPath")).toString();
    p.location = json.value(QStringLiteral("location")).toString();
    p.statement = json.value(QStringLiteral("statement")).toString();
    p.retrieved = QDateTime::fromString(json.value(QStringLiteral("retrieved")).toString(), Qt::ISODate);
    p.status = verificationStatusFromString(json.value(QStringLiteral("status")).toString());
    p.valueSource = valueSourceFromString(json.value(QStringLiteral("valueSource")).toString());
    p.notes = json.value(QStringLiteral("notes")).toString();
    return p;
}

// ---------------------------------------------------------------------------
// Pad
// ---------------------------------------------------------------------------
QJsonObject Pad::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), id);
    o.insert(QStringLiteral("pin"), pin);
    o.insert(QStringLiteral("pinName"), pinName);
    o.insert(QStringLiteral("position"), pointToJson(position));
    o.insert(QStringLiteral("size"), sizeToJson(size));
    o.insert(QStringLiteral("side"), eda::toString(side));
    o.insert(QStringLiteral("net"), netName);
    return o;
}

Pad Pad::fromJson(const QJsonObject &json)
{
    Pad p;
    p.id = json.value(QStringLiteral("id")).toString();
    p.pin = json.value(QStringLiteral("pin")).toString();
    p.pinName = json.value(QStringLiteral("pinName")).toString();
    p.position = pointFromJson(json.value(QStringLiteral("position")).toObject());
    p.size = sizeFromJson(json.value(QStringLiteral("size")).toObject());
    p.side = boardSideFromString(json.value(QStringLiteral("side")).toString());
    p.netName = json.value(QStringLiteral("net")).toString();
    return p;
}

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------
QJsonObject Component::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("uid"), uid);
    o.insert(QStringLiteral("reference"), reference);
    o.insert(QStringLiteral("value"), value);
    o.insert(QStringLiteral("description"), description);
    o.insert(QStringLiteral("category"), eda::toString(category));
    o.insert(QStringLiteral("manufacturer"), manufacturer);
    o.insert(QStringLiteral("mpn"), manufacturerPartNumber);
    o.insert(QStringLiteral("package"), package);
    o.insert(QStringLiteral("footprint"), footprint);
    o.insert(QStringLiteral("position"), pointToJson(position));
    o.insert(QStringLiteral("rotation"), rotation);
    o.insert(QStringLiteral("side"), eda::toString(side));
    o.insert(QStringLiteral("pinCount"), pinCount);
    o.insert(QStringLiteral("pads"), arrayToJson(pads));
    o.insert(QStringLiteral("nets"), stringListToJson(nets));
    o.insert(QStringLiteral("datasheetUrl"), datasheetUrl);
    o.insert(QStringLiteral("datasheetPath"), datasheetPath);
    o.insert(QStringLiteral("notes"), notes);
    o.insert(QStringLiteral("provenance"), provenance.toJson());
    o.insert(QStringLiteral("hasGeometry"), hasGeometry);
    return o;
}

Component Component::fromJson(const QJsonObject &json)
{
    Component c;
    c.uid = json.value(QStringLiteral("uid")).toString();
    c.reference = json.value(QStringLiteral("reference")).toString();
    c.value = json.value(QStringLiteral("value")).toString();
    c.description = json.value(QStringLiteral("description")).toString();
    c.category = componentCategoryFromString(json.value(QStringLiteral("category")).toString());
    c.manufacturer = json.value(QStringLiteral("manufacturer")).toString();
    c.manufacturerPartNumber = json.value(QStringLiteral("mpn")).toString();
    c.package = json.value(QStringLiteral("package")).toString();
    c.footprint = json.value(QStringLiteral("footprint")).toString();
    c.position = pointFromJson(json.value(QStringLiteral("position")).toObject());
    c.rotation = json.value(QStringLiteral("rotation")).toDouble();
    c.side = boardSideFromString(json.value(QStringLiteral("side")).toString());
    c.pinCount = json.value(QStringLiteral("pinCount")).toInt();
    c.pads = arrayFromJson<Pad>(json.value(QStringLiteral("pads")));
    c.nets = stringListFromJson(json.value(QStringLiteral("nets")));
    c.datasheetUrl = json.value(QStringLiteral("datasheetUrl")).toString();
    c.datasheetPath = json.value(QStringLiteral("datasheetPath")).toString();
    c.notes = json.value(QStringLiteral("notes")).toString();
    c.provenance = Provenance::fromJson(json.value(QStringLiteral("provenance")).toObject());
    c.hasGeometry = json.value(QStringLiteral("hasGeometry")).toBool(true);
    return c;
}

// ---------------------------------------------------------------------------
// Net
// ---------------------------------------------------------------------------
QJsonObject Net::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("pads"), stringListToJson(padIds));
    o.insert(QStringLiteral("confirmed"), confirmed);
    o.insert(QStringLiteral("provenance"), provenance.toJson());
    return o;
}

Net Net::fromJson(const QJsonObject &json)
{
    Net n;
    n.name = json.value(QStringLiteral("name")).toString();
    n.padIds = stringListFromJson(json.value(QStringLiteral("pads")));
    n.confirmed = json.value(QStringLiteral("confirmed")).toBool();
    n.provenance = Provenance::fromJson(json.value(QStringLiteral("provenance")).toObject());
    return n;
}

// ---------------------------------------------------------------------------
// Track / Via
// ---------------------------------------------------------------------------
QJsonObject Track::toJson() const
{
    QJsonObject o;
    QJsonArray path;
    for (const Point2D &p : points)
        path.append(pointToJson(p));
    o.insert(QStringLiteral("points"), path);
    o.insert(QStringLiteral("width"), width);
    o.insert(QStringLiteral("layer"), layerName);
    o.insert(QStringLiteral("net"), netName);
    return o;
}

Track Track::fromJson(const QJsonObject &json)
{
    Track t;
    const QJsonArray path = json.value(QStringLiteral("points")).toArray();
    t.points.reserve(path.size());
    for (const QJsonValue &v : path)
        t.points.append(pointFromJson(v.toObject()));
    t.width = json.value(QStringLiteral("width")).toDouble();
    t.layerName = json.value(QStringLiteral("layer")).toString();
    t.netName = json.value(QStringLiteral("net")).toString();
    return t;
}

QJsonObject Via::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("position"), pointToJson(position));
    o.insert(QStringLiteral("diameter"), diameter);
    o.insert(QStringLiteral("drill"), drill);
    o.insert(QStringLiteral("net"), netName);
    return o;
}

Via Via::fromJson(const QJsonObject &json)
{
    Via v;
    v.position = pointFromJson(json.value(QStringLiteral("position")).toObject());
    v.diameter = json.value(QStringLiteral("diameter")).toDouble();
    v.drill = json.value(QStringLiteral("drill")).toDouble();
    v.netName = json.value(QStringLiteral("net")).toString();
    return v;
}

// ---------------------------------------------------------------------------
// Layer
// ---------------------------------------------------------------------------
QJsonObject Layer::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("sourceName"), sourceName);
    o.insert(QStringLiteral("index"), index);
    o.insert(QStringLiteral("type"), eda::toString(type));
    o.insert(QStringLiteral("side"), eda::toString(side));
    o.insert(QStringLiteral("color"), color.name(QColor::HexArgb));
    o.insert(QStringLiteral("visible"), visible);
    return o;
}

Layer Layer::fromJson(const QJsonObject &json)
{
    Layer l;
    l.name = json.value(QStringLiteral("name")).toString();
    l.sourceName = json.value(QStringLiteral("sourceName")).toString();
    l.index = json.value(QStringLiteral("index")).toInt();
    l.type = layerTypeFromString(json.value(QStringLiteral("type")).toString());
    l.side = boardSideFromString(json.value(QStringLiteral("side")).toString());
    l.color = QColor(json.value(QStringLiteral("color")).toString());
    l.visible = json.value(QStringLiteral("visible")).toBool(true);
    return l;
}

// ---------------------------------------------------------------------------
// Board
// ---------------------------------------------------------------------------
const Component *Board::componentByUid(const QString &uid) const
{
    for (const Component &c : components)
        if (c.uid == uid)
            return &c;
    return nullptr;
}

Component *Board::componentByUid(const QString &uid)
{
    for (Component &c : components)
        if (c.uid == uid)
            return &c;
    return nullptr;
}

const Component *Board::componentByReference(const QString &reference) const
{
    for (const Component &c : components)
        if (c.reference.compare(reference, Qt::CaseInsensitive) == 0)
            return &c;
    return nullptr;
}

Component *Board::componentByReference(const QString &reference)
{
    for (Component &c : components)
        if (c.reference.compare(reference, Qt::CaseInsensitive) == 0)
            return &c;
    return nullptr;
}

int Board::indexOfComponent(const QString &uid) const
{
    for (int i = 0; i < components.size(); ++i)
        if (components.at(i).uid == uid)
            return i;
    return -1;
}

QJsonObject Board::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("uid"), uid);
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("sourcePath"), sourcePath);
    o.insert(QStringLiteral("sourceFormat"), sourceFormat);
    o.insert(QStringLiteral("warnings"), stringListToJson(warnings));
    QJsonObject box;
    box.insert(QStringLiteral("x"), boundingBox.x);
    box.insert(QStringLiteral("y"), boundingBox.y);
    box.insert(QStringLiteral("width"), boundingBox.width);
    box.insert(QStringLiteral("height"), boundingBox.height);
    o.insert(QStringLiteral("boundingBox"), box);
    o.insert(QStringLiteral("layers"), arrayToJson(layers));
    o.insert(QStringLiteral("components"), arrayToJson(components));
    o.insert(QStringLiteral("nets"), arrayToJson(nets));
    o.insert(QStringLiteral("tracks"), arrayToJson(tracks));
    o.insert(QStringLiteral("vias"), arrayToJson(vias));
    QJsonArray outlineArray;
    for (const QVector<Point2D> &outline : outlines) {
        QJsonArray poly;
        for (const Point2D &p : outline)
            poly.append(pointToJson(p));
        outlineArray.append(poly);
    }
    o.insert(QStringLiteral("outlines"), outlineArray);
    return o;
}

Board Board::fromJson(const QJsonObject &json)
{
    Board b;
    b.uid = json.value(QStringLiteral("uid")).toString();
    b.name = json.value(QStringLiteral("name")).toString();
    b.sourcePath = json.value(QStringLiteral("sourcePath")).toString();
    b.sourceFormat = json.value(QStringLiteral("sourceFormat")).toString();
    b.warnings = stringListFromJson(json.value(QStringLiteral("warnings")));
    const QJsonObject box = json.value(QStringLiteral("boundingBox")).toObject();
    b.boundingBox.x = box.value(QStringLiteral("x")).toDouble();
    b.boundingBox.y = box.value(QStringLiteral("y")).toDouble();
    b.boundingBox.width = box.value(QStringLiteral("width")).toDouble();
    b.boundingBox.height = box.value(QStringLiteral("height")).toDouble();
    b.layers = arrayFromJson<Layer>(json.value(QStringLiteral("layers")));
    b.components = arrayFromJson<Component>(json.value(QStringLiteral("components")));
    b.nets = arrayFromJson<Net>(json.value(QStringLiteral("nets")));
    b.tracks = arrayFromJson<Track>(json.value(QStringLiteral("tracks")));
    b.vias = arrayFromJson<Via>(json.value(QStringLiteral("vias")));
    const QJsonArray outlineArray = json.value(QStringLiteral("outlines")).toArray();
    for (const QJsonValue &polyValue : outlineArray) {
        QVector<Point2D> outline;
        const QJsonArray poly = polyValue.toArray();
        outline.reserve(poly.size());
        for (const QJsonValue &v : poly)
            outline.append(pointFromJson(v.toObject()));
        b.outlines.append(outline);
    }
    return b;
}

// ---------------------------------------------------------------------------
// TestPoint / Measurement
// ---------------------------------------------------------------------------
QJsonObject TestPoint::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("uid"), uid);
    o.insert(QStringLiteral("identifier"), identifier);
    o.insert(QStringLiteral("componentUid"), componentUid);
    o.insert(QStringLiteral("pinOrPad"), pinOrPad);
    o.insert(QStringLiteral("net"), netName);
    o.insert(QStringLiteral("position"), pointToJson(position));
    o.insert(QStringLiteral("hasPosition"), hasPosition);
    o.insert(QStringLiteral("category"), category);
    o.insert(QStringLiteral("expectedValue"), expectedValue);
    o.insert(QStringLiteral("hasExpectedValue"), hasExpectedValue);
    o.insert(QStringLiteral("tolerance"), tolerance);
    o.insert(QStringLiteral("hasTolerance"), hasTolerance);
    o.insert(QStringLiteral("unit"), unit);
    o.insert(QStringLiteral("testConditions"), testConditions);
    o.insert(QStringLiteral("instrument"), instrument);
    o.insert(QStringLiteral("probeNotes"), probeNotes);
    o.insert(QStringLiteral("notes"), notes);
    o.insert(QStringLiteral("provenance"), provenance.toJson());
    return o;
}

TestPoint TestPoint::fromJson(const QJsonObject &json)
{
    TestPoint t;
    t.uid = json.value(QStringLiteral("uid")).toString();
    t.identifier = json.value(QStringLiteral("identifier")).toString();
    t.componentUid = json.value(QStringLiteral("componentUid")).toString();
    t.pinOrPad = json.value(QStringLiteral("pinOrPad")).toString();
    t.netName = json.value(QStringLiteral("net")).toString();
    t.position = pointFromJson(json.value(QStringLiteral("position")).toObject());
    t.hasPosition = json.value(QStringLiteral("hasPosition")).toBool();
    t.category = json.value(QStringLiteral("category")).toString();
    t.expectedValue = json.value(QStringLiteral("expectedValue")).toDouble();
    t.hasExpectedValue = json.value(QStringLiteral("hasExpectedValue")).toBool();
    t.tolerance = json.value(QStringLiteral("tolerance")).toDouble();
    t.hasTolerance = json.value(QStringLiteral("hasTolerance")).toBool();
    t.unit = json.value(QStringLiteral("unit")).toString();
    t.testConditions = json.value(QStringLiteral("testConditions")).toString();
    t.instrument = json.value(QStringLiteral("instrument")).toString();
    t.probeNotes = json.value(QStringLiteral("probeNotes")).toString();
    t.notes = json.value(QStringLiteral("notes")).toString();
    t.provenance = Provenance::fromJson(json.value(QStringLiteral("provenance")).toObject());
    return t;
}

QJsonObject Measurement::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("uid"), uid);
    o.insert(QStringLiteral("testPointUid"), testPointUid);
    o.insert(QStringLiteral("timestamp"), timestamp.toString(Qt::ISODate));
    o.insert(QStringLiteral("value"), value);
    o.insert(QStringLiteral("unit"), unit);
    o.insert(QStringLiteral("expectedValue"), expectedValue);
    o.insert(QStringLiteral("hasExpectedValue"), hasExpectedValue);
    o.insert(QStringLiteral("tolerance"), tolerance);
    o.insert(QStringLiteral("hasTolerance"), hasTolerance);
    o.insert(QStringLiteral("status"), eda::toString(status));
    o.insert(QStringLiteral("instrument"), instrument);
    o.insert(QStringLiteral("conditions"), conditions);
    o.insert(QStringLiteral("source"), eda::toString(source));
    o.insert(QStringLiteral("sessionId"), sessionId);
    o.insert(QStringLiteral("notes"), notes);
    return o;
}

Measurement Measurement::fromJson(const QJsonObject &json)
{
    Measurement m;
    m.uid = json.value(QStringLiteral("uid")).toString();
    m.testPointUid = json.value(QStringLiteral("testPointUid")).toString();
    m.timestamp = QDateTime::fromString(json.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
    m.value = json.value(QStringLiteral("value")).toDouble();
    m.unit = json.value(QStringLiteral("unit")).toString();
    m.expectedValue = json.value(QStringLiteral("expectedValue")).toDouble();
    m.hasExpectedValue = json.value(QStringLiteral("hasExpectedValue")).toBool();
    m.tolerance = json.value(QStringLiteral("tolerance")).toDouble();
    m.hasTolerance = json.value(QStringLiteral("hasTolerance")).toBool();
    m.status = measurementStatusFromString(json.value(QStringLiteral("status")).toString());
    m.instrument = json.value(QStringLiteral("instrument")).toString();
    m.conditions = json.value(QStringLiteral("conditions")).toString();
    m.source = valueSourceFromString(json.value(QStringLiteral("source")).toString());
    m.sessionId = json.value(QStringLiteral("sessionId")).toString();
    m.notes = json.value(QStringLiteral("notes")).toString();
    return m;
}

// ---------------------------------------------------------------------------
// Hypothesis / session / document
// ---------------------------------------------------------------------------
QJsonObject Hypothesis::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("uid"), uid);
    o.insert(QStringLiteral("description"), description);
    o.insert(QStringLiteral("components"), stringListToJson(componentUids));
    o.insert(QStringLiteral("supporting"), stringListToJson(supportingEvidence));
    o.insert(QStringLiteral("contradicting"), stringListToJson(contradictingEvidence));
    o.insert(QStringLiteral("followUp"), stringListToJson(followUpTests));
    o.insert(QStringLiteral("provenanceIds"), stringListToJson(provenanceIds));
    o.insert(QStringLiteral("status"), eda::toString(status));
    o.insert(QStringLiteral("notes"), notes);
    return o;
}

Hypothesis Hypothesis::fromJson(const QJsonObject &json)
{
    Hypothesis h;
    h.uid = json.value(QStringLiteral("uid")).toString();
    h.description = json.value(QStringLiteral("description")).toString();
    h.componentUids = stringListFromJson(json.value(QStringLiteral("components")));
    h.supportingEvidence = stringListFromJson(json.value(QStringLiteral("supporting")));
    h.contradictingEvidence = stringListFromJson(json.value(QStringLiteral("contradicting")));
    h.followUpTests = stringListFromJson(json.value(QStringLiteral("followUp")));
    h.provenanceIds = stringListFromJson(json.value(QStringLiteral("provenanceIds")));
    h.status = hypothesisStatusFromString(json.value(QStringLiteral("status")).toString());
    h.notes = json.value(QStringLiteral("notes")).toString();
    return h;
}

QJsonObject DiagnosticSession::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("uid"), uid);
    o.insert(QStringLiteral("title"), title);
    o.insert(QStringLiteral("started"), started.toString(Qt::ISODate));
    o.insert(QStringLiteral("updated"), updated.toString(Qt::ISODate));
    o.insert(QStringLiteral("symptoms"), stringListToJson(symptoms));
    o.insert(QStringLiteral("hypotheses"), stringListToJson(hypothesisUids));
    o.insert(QStringLiteral("closed"), closed);
    o.insert(QStringLiteral("outcome"), outcome);
    return o;
}

DiagnosticSession DiagnosticSession::fromJson(const QJsonObject &json)
{
    DiagnosticSession s;
    s.uid = json.value(QStringLiteral("uid")).toString();
    s.title = json.value(QStringLiteral("title")).toString();
    s.started = QDateTime::fromString(json.value(QStringLiteral("started")).toString(), Qt::ISODate);
    s.updated = QDateTime::fromString(json.value(QStringLiteral("updated")).toString(), Qt::ISODate);
    s.symptoms = stringListFromJson(json.value(QStringLiteral("symptoms")));
    s.hypothesisUids = stringListFromJson(json.value(QStringLiteral("hypotheses")));
    s.closed = json.value(QStringLiteral("closed")).toBool();
    s.outcome = json.value(QStringLiteral("outcome")).toString();
    return s;
}

QJsonObject ImportedDocument::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("uid"), uid);
    o.insert(QStringLiteral("path"), path);
    o.insert(QStringLiteral("format"), format);
    o.insert(QStringLiteral("title"), title);
    o.insert(QStringLiteral("pageCount"), pageCount);
    o.insert(QStringLiteral("hash"), hash);
    o.insert(QStringLiteral("imported"), imported.toString(Qt::ISODate));
    o.insert(QStringLiteral("warnings"), stringListToJson(warnings));
    return o;
}

ImportedDocument ImportedDocument::fromJson(const QJsonObject &json)
{
    ImportedDocument d;
    d.uid = json.value(QStringLiteral("uid")).toString();
    d.path = json.value(QStringLiteral("path")).toString();
    d.format = json.value(QStringLiteral("format")).toString();
    d.title = json.value(QStringLiteral("title")).toString();
    d.pageCount = json.value(QStringLiteral("pageCount")).toInt();
    d.hash = json.value(QStringLiteral("hash")).toString();
    d.imported = QDateTime::fromString(json.value(QStringLiteral("imported")).toString(), Qt::ISODate);
    d.warnings = stringListFromJson(json.value(QStringLiteral("warnings")));
    return d;
}

// ---------------------------------------------------------------------------
// Project
// ---------------------------------------------------------------------------
QJsonObject ProjectMetadata::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("description"), description);
    o.insert(QStringLiteral("boardModel"), boardModel);
    o.insert(QStringLiteral("createdBy"), createdBy);
    o.insert(QStringLiteral("created"), created.toString(Qt::ISODate));
    o.insert(QStringLiteral("modified"), modified.toString(Qt::ISODate));
    o.insert(QStringLiteral("applicationVersion"), applicationVersion);
    o.insert(QStringLiteral("schemaVersion"), schemaVersion);
    return o;
}

ProjectMetadata ProjectMetadata::fromJson(const QJsonObject &json)
{
    ProjectMetadata m;
    m.name = json.value(QStringLiteral("name")).toString();
    m.description = json.value(QStringLiteral("description")).toString();
    m.boardModel = json.value(QStringLiteral("boardModel")).toString();
    m.createdBy = json.value(QStringLiteral("createdBy")).toString();
    m.created = QDateTime::fromString(json.value(QStringLiteral("created")).toString(), Qt::ISODate);
    m.modified = QDateTime::fromString(json.value(QStringLiteral("modified")).toString(), Qt::ISODate);
    m.applicationVersion = json.value(QStringLiteral("applicationVersion")).toString();
    m.schemaVersion = json.value(QStringLiteral("schemaVersion")).toInt(1);
    return m;
}

QJsonObject ProjectData::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("metadata"), metadata.toJson());
    o.insert(QStringLiteral("boards"), arrayToJson(boards));
    o.insert(QStringLiteral("testPoints"), arrayToJson(testPoints));
    o.insert(QStringLiteral("measurements"), arrayToJson(measurements));
    o.insert(QStringLiteral("hypotheses"), arrayToJson(hypotheses));
    o.insert(QStringLiteral("sessions"), arrayToJson(sessions));
    o.insert(QStringLiteral("documents"), arrayToJson(documents));
    o.insert(QStringLiteral("notes"), stringListToJson(notes));
    return o;
}

ProjectData ProjectData::fromJson(const QJsonObject &json)
{
    ProjectData p;
    p.metadata = ProjectMetadata::fromJson(json.value(QStringLiteral("metadata")).toObject());
    p.boards = arrayFromJson<Board>(json.value(QStringLiteral("boards")));
    p.testPoints = arrayFromJson<TestPoint>(json.value(QStringLiteral("testPoints")));
    p.measurements = arrayFromJson<Measurement>(json.value(QStringLiteral("measurements")));
    p.hypotheses = arrayFromJson<Hypothesis>(json.value(QStringLiteral("hypotheses")));
    p.sessions = arrayFromJson<DiagnosticSession>(json.value(QStringLiteral("sessions")));
    p.documents = arrayFromJson<ImportedDocument>(json.value(QStringLiteral("documents")));
    p.notes = stringListFromJson(json.value(QStringLiteral("notes")));
    return p;
}

} // namespace eda
