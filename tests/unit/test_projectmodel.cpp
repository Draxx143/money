// Unit tests for the unified project model: enum round trips and JSON
// serialisation of every record type.
#include <QtTest>

#include "../../src/core/ProjectModel.h"
#include "../TestEnvironment.h"

// QTest prints the values of a failed comparison through QTest::toString(),
// which resolves the name by argument dependent lookup and therefore finds
// eda::toString(...) -- whose QString return type does not fit the char* the
// test library expects. The enums are compared through their string form
// instead, which is what the serialisation uses anyway.
#define COMPARE_ENUM(actual, expected) QCOMPARE(eda::toString(actual), eda::toString(expected))

class TestProjectModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { eda::TestEnvironment::init(QStringLiteral("projectmodel")); }
    void cleanupTestCase() { eda::TestEnvironment::cleanup(); }

    void enumRoundTrips()
    {
        QCOMPARE(eda::toString(eda::BoardSide::Top), QStringLiteral("top"));
        COMPARE_ENUM(eda::boardSideFromString(QStringLiteral("bottom")), eda::BoardSide::Bottom);
        COMPARE_ENUM(eda::boardSideFromString(QStringLiteral("B.Cu")), eda::BoardSide::Bottom);
        COMPARE_ENUM(eda::boardSideFromString(QStringLiteral("nonsense")), eda::BoardSide::Unknown);

        QCOMPARE(eda::toString(eda::ComponentCategory::IntegratedCircuit), QStringLiteral("ic"));
        COMPARE_ENUM(eda::componentCategoryFromString(QStringLiteral("resistor")),
                     eda::ComponentCategory::Resistor);
        COMPARE_ENUM(eda::componentCategoryFromString(QString()), eda::ComponentCategory::Unknown);

        QCOMPARE(eda::toString(eda::LayerType::Copper), QStringLiteral("copper"));
        COMPARE_ENUM(eda::layerTypeFromString(QStringLiteral("silkscreen")), eda::LayerType::Silkscreen);

        QCOMPARE(eda::toString(eda::VerificationStatus::OfficialSourceVerified),
                 QStringLiteral("official-source"));
        COMPARE_ENUM(eda::verificationStatusFromString(QStringLiteral("user-verified")),
                     eda::VerificationStatus::UserVerified);

        QCOMPARE(eda::toString(eda::ValueSource::Measured), QStringLiteral("measured"));
        COMPARE_ENUM(eda::valueSourceFromString(QStringLiteral("ai-suggested")),
                     eda::ValueSource::AiSuggested);

        QCOMPARE(eda::toString(eda::MeasurementStatus::NotTested), QStringLiteral("not-tested"));
        COMPARE_ENUM(eda::measurementStatusFromString(QStringLiteral("pass")), eda::MeasurementStatus::Pass);

        QCOMPARE(eda::toString(eda::HypothesisStatus::ConfirmedByRepair),
                 QStringLiteral("confirmed-repair"));
        COMPARE_ENUM(eda::hypothesisStatusFromString(QStringLiteral("ruled-out")),
                     eda::HypothesisStatus::RuledOut);
    }

    void componentSerialisationKeepsEveryField()
    {
        eda::Component component;
        component.uid = QStringLiteral("c-1");
        component.reference = QStringLiteral("U3");
        component.value = QStringLiteral("STM32F103C8T6");
        component.description = QStringLiteral("MCU");
        component.category = eda::ComponentCategory::IntegratedCircuit;
        component.manufacturer = QStringLiteral("STMicroelectronics");
        component.manufacturerPartNumber = QStringLiteral("STM32F103C8T6");
        component.package = QStringLiteral("LQFP-48");
        component.footprint = QStringLiteral("LQFP-48_7x7mm");
        component.position = { 42.5, -12.25 };
        component.rotation = 90.0;
        component.side = eda::BoardSide::Top;
        component.pinCount = 48;
        component.hasGeometry = true;

        eda::Pad pad;
        pad.id = QStringLiteral("1");
        pad.pin = QStringLiteral("1");
        pad.pinName = QStringLiteral("VBAT");
        pad.position = { -3.5, -3.5 };
        pad.size = { 0.25, 0.6 };
        pad.side = eda::BoardSide::Top;
        pad.netName = QStringLiteral("+3V3");
        component.pads.append(pad);
        component.nets = { QStringLiteral("+3V3"), QStringLiteral("GND") };
        component.datasheetUrl = QStringLiteral("https://example.invalid/stm32.pdf");

        eda::Provenance provenance;
        provenance.title = QStringLiteral("Board BOM");
        provenance.publisher = QStringLiteral("board vendor");
        provenance.localPath = QStringLiteral("/tmp/bom.csv");
        provenance.status = eda::VerificationStatus::ExtractedFromSource;
        provenance.valueSource = eda::ValueSource::ExtractedFromSource;
        component.provenance = provenance;

        const eda::Component restored = eda::Component::fromJson(component.toJson());
        QCOMPARE(restored.uid, component.uid);
        QCOMPARE(restored.reference, component.reference);
        QCOMPARE(restored.value, component.value);
        COMPARE_ENUM(restored.category, component.category);
        QCOMPARE(restored.position.x, component.position.x);
        QCOMPARE(restored.position.y, component.position.y);
        QCOMPARE(restored.rotation, component.rotation);
        COMPARE_ENUM(restored.side, component.side);
        QCOMPARE(restored.pinCount, component.pinCount);
        QCOMPARE(restored.pads.size(), 1);
        QCOMPARE(restored.pads.first().pinName, QStringLiteral("VBAT"));
        QCOMPARE(restored.pads.first().netName, QStringLiteral("+3V3"));
        QCOMPARE(restored.nets, component.nets);
        QCOMPARE(restored.datasheetUrl, component.datasheetUrl);
        COMPARE_ENUM(restored.provenance.status, eda::VerificationStatus::ExtractedFromSource);
        COMPARE_ENUM(restored.provenance.valueSource, eda::ValueSource::ExtractedFromSource);
        QVERIFY(restored.hasGeometry);
    }

    void missingGeometryIsPreservedAsMissing()
    {
        eda::Component component;
        component.uid = QStringLiteral("c-2");
        component.hasGeometry = false;
        const eda::Component restored = eda::Component::fromJson(component.toJson());
        QVERIFY(!restored.hasGeometry);
        QVERIFY(restored.reference.isEmpty());
        QCOMPARE(restored.pinCount, 0);
    }

    void boardAndTestPointSerialisation()
    {
        eda::Board board;
        board.uid = QStringLiteral("b-1");
        board.name = QStringLiteral("main board");
        board.sourceFormat = QStringLiteral("kicad-pcb");
        board.sourcePath = QStringLiteral("/tmp/board.kicad_pcb");
        board.boundingBox = { 0, 0, 80, 55 };
        board.warnings = { QStringLiteral("zones are not imported") };

        eda::Layer layer;
        layer.name = QStringLiteral("F.Cu");
        layer.index = 0;
        layer.type = eda::LayerType::Copper;
        layer.side = eda::BoardSide::Top;
        layer.color = QColor(QStringLiteral("#00D1B2"));
        board.layers.append(layer);

        eda::Component component;
        component.uid = QStringLiteral("c-1");
        component.reference = QStringLiteral("R1");
        board.components.append(component);

        eda::Net net;
        net.name = QStringLiteral("GND");
        net.confirmed = true;
        board.nets.append(net);

        eda::Track track;
        track.points = { { 0, 0 }, { 10, 0 }, { 10, 10 } };
        track.width = 0.25;
        track.layerName = QStringLiteral("F.Cu");
        track.netName = QStringLiteral("GND");
        board.tracks.append(track);

        eda::Via via;
        via.position = { 10, 10 };
        via.drill = 0.3;
        board.vias.append(via);
        board.outlines.append({ { 0, 0 }, { 80, 0 }, { 80, 55 }, { 0, 55 } });

        const eda::Board restored = eda::Board::fromJson(board.toJson());
        QCOMPARE(restored.name, board.name);
        QCOMPARE(restored.sourceFormat, board.sourceFormat);
        QCOMPARE(restored.boundingBox.width, 80.0);
        QCOMPARE(restored.warnings, board.warnings);
        QCOMPARE(restored.layers.size(), 1);
        COMPARE_ENUM(restored.layers.first().type, eda::LayerType::Copper);
        QCOMPARE(restored.components.size(), 1);
        QVERIFY(restored.componentByReference(QStringLiteral("r1")) != nullptr);
        QVERIFY(restored.componentByUid(QStringLiteral("c-1")) != nullptr);
        QVERIFY(restored.componentByUid(QStringLiteral("nope")) == nullptr);
        QCOMPARE(restored.nets.first().confirmed, true);
        QCOMPARE(restored.tracks.first().points.size(), 3);
        QCOMPARE(restored.vias.first().drill, 0.3);
        QCOMPARE(restored.outlines.first().size(), 4);

        eda::TestPoint point;
        point.uid = QStringLiteral("tp-1");
        point.identifier = QStringLiteral("TP1");
        point.componentUid = QStringLiteral("c-1");
        point.pinOrPad = QStringLiteral("2");
        point.netName = QStringLiteral("+5V");
        point.position = { 12.5, 8.0 };
        point.hasPosition = true;
        point.category = QStringLiteral("voltage");
        point.hasExpectedValue = true;
        point.expectedValue = 5.0;
        point.hasTolerance = true;
        point.tolerance = 0.25;
        point.unit = QStringLiteral("V");

        const eda::TestPoint restoredPoint = eda::TestPoint::fromJson(point.toJson());
        QCOMPARE(restoredPoint.identifier, point.identifier);
        QCOMPARE(restoredPoint.expectedValue, 5.0);
        QVERIFY(restoredPoint.hasExpectedValue);
        QVERIFY(restoredPoint.hasPosition);
        QCOMPARE(restoredPoint.position.x, 12.5);

        // A test point without an expected value must stay without one.
        eda::TestPoint bare;
        bare.uid = QStringLiteral("tp-2");
        const eda::TestPoint restoredBare = eda::TestPoint::fromJson(bare.toJson());
        QVERIFY(!restoredBare.hasExpectedValue);
        QCOMPARE(restoredBare.expectedValue, 0.0);
    }

    void measurementAndHypothesisSerialisation()
    {
        eda::Measurement measurement;
        measurement.uid = QStringLiteral("m-1");
        measurement.testPointUid = QStringLiteral("tp-1");
        measurement.timestamp = QDateTime::fromString(QStringLiteral("2026-01-02T03:04:05Z"), Qt::ISODate);
        measurement.value = 4.82;
        measurement.unit = QStringLiteral("V");
        measurement.hasExpectedValue = true;
        measurement.expectedValue = 5.0;
        measurement.hasTolerance = true;
        measurement.tolerance = 0.25;
        measurement.status = eda::MeasurementStatus::Pass;
        measurement.source = eda::ValueSource::Measured;
        measurement.sessionId = QStringLiteral("s-1");

        const eda::Measurement restored = eda::Measurement::fromJson(measurement.toJson());
        QCOMPARE(restored.value, 4.82);
        COMPARE_ENUM(restored.status, eda::MeasurementStatus::Pass);
        COMPARE_ENUM(restored.source, eda::ValueSource::Measured);
        QCOMPARE(restored.timestamp, measurement.timestamp);
        QCOMPARE(restored.sessionId, QStringLiteral("s-1"));

        eda::Hypothesis hypothesis;
        hypothesis.uid = QStringLiteral("h-1");
        hypothesis.description = QStringLiteral("input bulk capacitor dried out");
        hypothesis.componentUids = { QStringLiteral("c-1") };
        hypothesis.supportingEvidence = { QStringLiteral("ripple of 2 V at C1") };
        hypothesis.contradictingEvidence = { QStringLiteral("ESR measures 0.2 ohm") };
        hypothesis.status = eda::HypothesisStatus::Plausible;
        hypothesis.provenanceIds = { QStringLiteral("src-1") };

        const eda::Hypothesis restoredHypothesis = eda::Hypothesis::fromJson(hypothesis.toJson());
        QCOMPARE(restoredHypothesis.description, hypothesis.description);
        COMPARE_ENUM(restoredHypothesis.status, eda::HypothesisStatus::Plausible);
        QCOMPARE(restoredHypothesis.componentUids, hypothesis.componentUids);
        QCOMPARE(restoredHypothesis.contradictingEvidence, hypothesis.contradictingEvidence);
    }

    void wholeProjectRoundTrips()
    {
        eda::ProjectData data;
        data.metadata.name = QStringLiteral("demo");
        data.metadata.boardModel = QStringLiteral("ACME motor controller");
        data.metadata.created = QDateTime::currentDateTimeUtc();
        data.metadata.modified = data.metadata.created;

        eda::Board board;
        board.uid = QStringLiteral("b-1");
        eda::Component component;
        component.uid = QStringLiteral("c-1");
        component.reference = QStringLiteral("Q2");
        board.components.append(component);
        data.boards.append(board);

        eda::Measurement measurement;
        measurement.uid = QStringLiteral("m-1");
        measurement.value = 11.9;
        data.measurements.append(measurement);

        eda::Hypothesis hypothesis;
        hypothesis.uid = QStringLiteral("h-1");
        data.hypotheses.append(hypothesis);

        eda::DiagnosticSession session;
        session.uid = QStringLiteral("s-1");
        session.title = QStringLiteral("no output");
        data.sessions.append(session);

        eda::ImportedDocument document;
        document.uid = QStringLiteral("d-1");
        document.format = QStringLiteral("pdf");
        document.path = QStringLiteral("/tmp/schematic.pdf");
        data.documents.append(document);

        const eda::ProjectData restored = eda::ProjectData::fromJson(data.toJson());
        QCOMPARE(restored.metadata.name, QStringLiteral("demo"));
        QCOMPARE(restored.boards.size(), 1);
        QCOMPARE(restored.boards.first().components.size(), 1);
        QCOMPARE(restored.boards.first().components.first().reference, QStringLiteral("Q2"));
        QCOMPARE(restored.measurements.size(), 1);
        QCOMPARE(restored.hypotheses.size(), 1);
        QCOMPARE(restored.sessions.size(), 1);
        QCOMPARE(restored.documents.size(), 1);
        QCOMPARE(restored.documents.first().format, QStringLiteral("pdf"));
    }
};

QTEST_APPLESS_MAIN(TestProjectModel)
#include "test_projectmodel.moc"
