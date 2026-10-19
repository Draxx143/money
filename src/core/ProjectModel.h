// Unified project model.
//
// Every importer, viewer, diagnostic and report in the application works on
// this model, never on a file format. The model is intentionally explicit about
// *how much* is known about a piece of data: a value carries the source it came
// from and a verification status, and a missing value stays missing instead of
// being guessed. Nothing in here is ever filled in with plausible looking
// defaults by an importer or by the AI layer.
//
// Units: board geometry is stored in millimetres, board coordinates (X to the
// right, Y downwards for the top side, as in KiCad/Gerber). Electrical values
// are stored in base units (volt, ampere, ohm, farad, degree Celsius) together
// with the unit they were entered in.
#pragma once

#include <QColor>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace eda {

enum class BoardSide { Unknown = 0, Top, Bottom };
enum class ComponentCategory { Unknown = 0, Resistor, Capacitor, Inductor, Diode, Transistor,
                               IntegratedCircuit, Connector, Crystal, Fuse, Relay, Transformer,
                               Led, Switch, TestPoint, Mechanical, Other };
enum class LayerType { Unknown = 0, Copper, Silkscreen, SolderMask, Paste, Edge, Fabrication, Other };
// Physical shape of a pad, exactly as named by the source format.
enum class PadShape { Unknown = 0, Rectangle, Circle, Oval, RoundedRectangle, Trapezoid, Custom };

// How trustworthy a piece of information is.
enum class VerificationStatus {
    Unknown = 0,
    NeedsReview,          // recorded, not yet looked at
    Unverified,           // no source that could confirm it
    AiSuggested,          // proposed by the AI layer, not confirmed
    Inferred,             // derived from other data, not directly confirmed
    ExtractedFromSource,  // copied out of an imported file
    UserVerified,         // checked by the user
    ExperimentallyVerified, // confirmed by a measurement
    OfficialSourceVerified, // confirmed by an official datasheet / document
    Corroborated,         // confirmed by more than one independent source
    ConflictingSources
};

// Where a value came from.
enum class ValueSource {
    Unknown = 0,
    ExtractedFromSource,
    ManualEntry,
    Measured,
    AiSuggested,
    Inferred
};

// Result of comparing a measurement against an expectation.
enum class MeasurementStatus { NotTested = 0, Pass, Fail, Unknown };

QString toString(BoardSide side);
BoardSide boardSideFromString(const QString &text);
QString toString(ComponentCategory category);
ComponentCategory componentCategoryFromString(const QString &text);
QString toString(LayerType type);
LayerType layerTypeFromString(const QString &text);
QString toString(PadShape shape);
PadShape padShapeFromString(const QString &text);
QString toString(VerificationStatus status);
VerificationStatus verificationStatusFromString(const QString &text);
QString toString(ValueSource source);
ValueSource valueSourceFromString(const QString &text);
QString toString(MeasurementStatus status);
MeasurementStatus measurementStatusFromString(const QString &text);

struct Point2D
{
    double x = 0.0;
    double y = 0.0;
};

struct Size2D
{
    double width = 0.0;
    double height = 0.0;
};

struct Rect2D
{
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    bool isEmpty() const { return width <= 0.0 || height <= 0.0; }
};

// Where a fact came from. Kept with the fact, never separately.
struct Provenance
{
    QString sourceId;              // id of the imported document / datasheet
    QString title;
    QString publisher;
    QString url;                   // empty for local documents
    QString localPath;             // empty for online sources
    QString location;              // page, section or "pin 5"
    QString statement;             // the extracted text, if any
    QDateTime retrieved;
    VerificationStatus status = VerificationStatus::Unverified;
    ValueSource valueSource = ValueSource::Unknown;
    QString notes;

    bool isEmpty() const;
    QJsonObject toJson() const;
    static Provenance fromJson(const QJsonObject &json);
};

struct Pad
{
    QString id;                    // stable inside the component
    QString pin;                   // pin number / designator as printed
    QString pinName;               // electrical name if known (e.g. "VCC")
    Point2D position;              // relative to the component origin, mm
    Size2D size;                   // mm
    double rotation = 0.0;         // degrees, in the component's frame
    PadShape shape = PadShape::Unknown;
    BoardSide side = BoardSide::Unknown;
    QString netName;               // empty when unknown

    QJsonObject toJson() const;
    static Pad fromJson(const QJsonObject &json);
};

struct Component
{
    // Stable identifier. Row numbers, list order and even the reference
    // designator can change; this cannot, and every cross reference in the
    // project (test points, findings, notes) uses it.
    QString uid;
    QString reference;             // "U1", "R12" -- empty when unknown
    QString value;                 // "10k", "STM32F103C8T6"
    QString description;
    ComponentCategory category = ComponentCategory::Unknown;
    QString manufacturer;
    QString manufacturerPartNumber;
    QString package;               // "SOIC-8"
    QString footprint;             // as named in the source data
    Point2D position;              // mm, board coordinates
    double rotation = 0.0;         // degrees
    BoardSide side = BoardSide::Top;
    int pinCount = 0;              // 0 = unknown
    QVector<Pad> pads;
    QStringList nets;
    QString datasheetUrl;
    QString datasheetPath;         // local copy, if imported
    QString notes;
    Provenance provenance;

    // Set when the component could not be placed on the board (no geometry in
    // the source data). The UI reports this instead of drawing a guess.
    bool hasGeometry = true;

    QJsonObject toJson() const;
    static Component fromJson(const QJsonObject &json);
};

struct Net
{
    QString name;
    QStringList padIds;            // "<componentUid>:<padId>"
    bool confirmed = false;        // true only for connectivity read from a
                                   // reliable structured source
    Provenance provenance;

    QJsonObject toJson() const;
    static Net fromJson(const QJsonObject &json);
};

struct Track
{
    QVector<Point2D> points;
    double width = 0.0;            // mm
    QString layerName;
    QString netName;

    QJsonObject toJson() const;
    static Track fromJson(const QJsonObject &json);
};

struct Via
{
    Point2D position;
    double diameter = 0.0;
    double drill = 0.0;
    QString netName;

    QJsonObject toJson() const;
    static Via fromJson(const QJsonObject &json);
};

struct Layer
{
    QString name;                  // internal/canonical name
    QString sourceName;            // name used by the imported file
    int index = 0;
    LayerType type = LayerType::Other;
    BoardSide side = BoardSide::Unknown;
    QColor color;
    bool visible = true;

    QJsonObject toJson() const;
    static Layer fromJson(const QJsonObject &json);
};

struct Board
{
    QString uid;
    QString name;
    QString sourcePath;            // file the board was imported from
    QString sourceFormat;          // "kicad-pcb", "gerber", ...
    QStringList warnings;          // limitations of the import
    Rect2D boundingBox;            // mm
    QVector<Layer> layers;
    QVector<Component> components;
    QVector<Net> nets;
    QVector<Track> tracks;
    QVector<Via> vias;
    QVector<QVector<Point2D>> outlines;

    const Component *componentByUid(const QString &uid) const;
    Component *componentByUid(const QString &uid);
    const Component *componentByReference(const QString &reference) const;
    Component *componentByReference(const QString &reference);
    int indexOfComponent(const QString &uid) const;

    QJsonObject toJson() const;
    static Board fromJson(const QJsonObject &json);
};

struct TestPoint
{
    QString uid;
    QString identifier;            // "TP7", "C12.2"
    QString componentUid;          // empty when the test point is a bare pad/net
    QString pinOrPad;
    QString netName;
    Point2D position;              // mm, board coordinates
    bool hasPosition = false;
    QString category;              // "voltage", "resistance", "diode", ...
    double expectedValue = 0.0;
    bool hasExpectedValue = false;
    double tolerance = 0.0;
    bool hasTolerance = false;
    QString unit;
    QString testConditions;        // "board powered, 24 V applied"
    QString instrument;            // "Fluke 87V"
    QString probeNotes;            // where to put the probes
    QString notes;
    Provenance provenance;         // for the expected value

    QJsonObject toJson() const;
    static TestPoint fromJson(const QJsonObject &json);
};

struct Measurement
{
    QString uid;
    QString testPointUid;
    QDateTime timestamp;
    double value = 0.0;
    QString unit;
    double expectedValue = 0.0;
    bool hasExpectedValue = false;
    double tolerance = 0.0;
    bool hasTolerance = false;
    MeasurementStatus status = MeasurementStatus::NotTested;
    QString instrument;
    QString conditions;
    ValueSource source = ValueSource::ManualEntry;
    QString sessionId;             // diagnostic session this belongs to
    QString notes;

    QJsonObject toJson() const;
    static Measurement fromJson(const QJsonObject &json);
};

enum class HypothesisStatus {
    Possible = 0,
    Plausible,
    StronglySupported,
    ConfirmedByMeasurement,
    ConfirmedByRepair,
    RuledOut,
    Unresolved
};

QString toString(HypothesisStatus status);
HypothesisStatus hypothesisStatusFromString(const QString &text);

struct Hypothesis
{
    QString uid;
    QString description;
    QStringList componentUids;
    QStringList supportingEvidence;
    QStringList contradictingEvidence;
    QStringList followUpTests;
    QStringList provenanceIds;
    HypothesisStatus status = HypothesisStatus::Possible;
    QString notes;

    QJsonObject toJson() const;
    static Hypothesis fromJson(const QJsonObject &json);
};

struct DiagnosticSession
{
    QString uid;
    QString title;
    QDateTime started;
    QDateTime updated;
    QStringList symptoms;
    QStringList hypothesisUids;
    bool closed = false;
    QString outcome;

    QJsonObject toJson() const;
    static DiagnosticSession fromJson(const QJsonObject &json);
};

struct ImportedDocument
{
    QString uid;
    QString path;
    QString format;                // "pdf", "image", "kicad-sch", "csv", ...
    QString title;
    int pageCount = 0;
    QString hash;                  // sha256 of the file at import time
    QDateTime imported;
    QStringList warnings;

    QJsonObject toJson() const;
    static ImportedDocument fromJson(const QJsonObject &json);
};

struct ProjectMetadata
{
    QString name;
    QString description;
    QString boardModel;            // what the user calls the device under test
    QString createdBy;
    QDateTime created;
    QDateTime modified;
    QString applicationVersion;    // version that wrote the file
    int schemaVersion = 1;

    QJsonObject toJson() const;
    static ProjectMetadata fromJson(const QJsonObject &json);
};

struct ProjectData
{
    ProjectMetadata metadata;
    QVector<Board> boards;
    QVector<TestPoint> testPoints;
    QVector<Measurement> measurements;
    QVector<Hypothesis> hypotheses;
    QVector<DiagnosticSession> sessions;
    QVector<ImportedDocument> documents;
    QStringList notes;

    QJsonObject toJson() const;
    static ProjectData fromJson(const QJsonObject &json);
};

} // namespace eda
