#include "BoardView.h"

#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace eda {

namespace {

// Board coordinates are millimetres with Y pointing down; the scene uses the
// same convention, so the view transform is a plain scale (negative X when the
// back of the board is shown).
constexpr double kMinScale = 0.05;      // pixels per millimetre
constexpr double kMaxScale = 400.0;
constexpr double kDefaultScale = 4.0;
constexpr double kMargin = 5.0;         // scene margin around the board, mm

// Draw order, back to front.
constexpr double kZBoard = 0.0;
constexpr double kZTracks = 1.0;
constexpr double kZVias = 2.0;
constexpr double kZPads = 3.0;
constexpr double kZBody = 4.0;
constexpr double kZOutline = 5.0;
constexpr double kZLabel = 6.0;

const QColor kBackground(QStringLiteral("#0B0F14"));
const QColor kGrid(0x28, 0x35, 0x44);
const QColor kBoardFill(0x12, 0x19, 0x23);
const QColor kOutline(QStringLiteral("#00D1B2"));
const QColor kTrack(0xD6, 0x9E, 0x2E);
const QColor kVia(0xF1, 0xC4, 0x0F);
const QColor kPad(0xC0, 0xC5, 0xC8);
const QColor kTopSide(QStringLiteral("#4DA3FF"));
const QColor kBottomSide(QStringLiteral("#00D1B2"));
const QColor kBody(0x18, 0x22, 0x2E);
const QColor kLabel(QStringLiteral("#94A3B8"));
const QColor kText(QStringLiteral("#F1F5F9"));

QColor sideColor(BoardSide side)
{
    return side == BoardSide::Bottom ? kBottomSide : kTopSide;
}

QPointF toPoint(const Point2D &point)
{
    return QPointF(point.x, point.y);
}

QRectF toRect(const Rect2D &rect)
{
    return QRectF(rect.x, rect.y, rect.width, rect.height);
}

// A pad is drawn in its own shape when the file says which shape it is. A shape
// this version does not keep is drawn as its bounding box, never as something
// invented.
QPainterPath padPath(const Pad &pad)
{
    const double w = std::abs(pad.size.width);
    const double h = std::abs(pad.size.height);
    const QRectF bounds(-w / 2.0, -h / 2.0, w, h);
    QPainterPath path;
    if (w <= 0.0 || h <= 0.0)
        return path;

    switch (pad.shape) {
    case PadShape::Unknown:
    case PadShape::Rectangle:
    case PadShape::Custom:
        path.addRect(bounds);
        break;
    case PadShape::RoundedRectangle:
        path.addRoundedRect(bounds, std::min(w, h) * 0.25, std::min(w, h) * 0.25);
        break;
    case PadShape::Oval:
        path.addRoundedRect(bounds, w * 0.5, h * 0.5);
        break;
    case PadShape::Circle:
        path.addEllipse(bounds);
        break;
    case PadShape::Trapezoid: {
        const double inset = std::min(w, h) * 0.15;
        QPolygonF polygon;
        polygon << QPointF(-w / 2.0, -h / 2.0 + inset) << QPointF(w / 2.0, -h / 2.0)
                << QPointF(w / 2.0 - inset, h / 2.0) << QPointF(-w / 2.0, h / 2.0 - inset);
        path.addPolygon(polygon);
        path.closeSubpath();
        break;
    }
    }
    return path;
}

} // namespace

// One footprint: its pads, its body rectangle and its reference. The group is
// the clickable thing, so a component can be picked wherever it is on the
// board, and highlighting it is a single call.
class BoardView::ComponentItem : public QGraphicsItem
{
public:
    explicit ComponentItem(const Component *component)
        : m_component(component)
    {
        setData(0, component->uid);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
    }

    const Component *component() const { return m_component; }

    QRectF boundingRect() const override { return m_hull; }
    QPainterPath shape() const override
    {
        QPainterPath path;
        path.addRect(m_hull);
        return path;
    }
    void setHull(const QRectF &hull)
    {
        prepareGeometryChange();
        m_hull = hull;
    }
    const QRectF &hull() const { return m_hull; }

    // The children (pads, body, label) draw everything.
    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {}

private:
    const Component *m_component = nullptr;
    QRectF m_hull;
};

BoardView::BoardView(QWidget *parent)
    : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setBackgroundBrush(kBackground);
    setMouseTracking(true);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

    rebuildScene();
}

void BoardView::setBoard(const Board *board)
{
    if (m_board == board)
        return;
    m_board = board;
    m_selectedUid.clear();
    m_hoveredUid.clear();
    rebuildScene();
    fitToBoard();
}

void BoardView::rebuildScene()
{
    QGraphicsScene *boardScene = scene();
    boardScene->clear();
    m_componentItems.clear();
    m_placeholder = nullptr;
    m_hoveredUid.clear();

    if (m_board == nullptr) {
        auto *placeholder = new QGraphicsSimpleTextItem(tr("No board loaded"));
        placeholder->setBrush(kLabel);
        placeholder->setData(0, QStringLiteral("placeholder"));
        boardScene->addItem(placeholder);
        m_placeholder = placeholder;
        boardScene->setSceneRect(QRectF(-100, -60, 200, 120));
        return;
    }

    // --- the board itself -------------------------------------------------
    QRectF boardBox;
    for (const QVector<Point2D> &poly : m_board->outlines) {
        for (const Point2D &point : poly) {
            const QPointF p = toPoint(point);
            boardBox = boardBox.isNull() ? QRectF(p, QSizeF(0, 0)) : boardBox.united(QRectF(p, QSizeF(0, 0)));
        }
    }
    if (boardBox.isNull() && !m_board->boundingBox.isEmpty())
        boardBox = toRect(m_board->boundingBox);
    if (boardBox.isNull()) {
        // No geometry in the file at all: keep every placement visible.
        for (const Component &component : m_board->components) {
            const QPointF p(component.position.x, component.position.y);
            boardBox = boardBox.isNull() ? QRectF(p, QSizeF(0, 0))
                                         : boardBox.united(QRectF(p, QSizeF(0, 0)));
        }
        boardBox.adjust(-kMargin, -kMargin, kMargin, kMargin);
    }
    boardScene->setSceneRect(boardBox.adjusted(-kMargin, -kMargin, kMargin, kMargin));

    // The substrate, so the board reads as a board and not as loose copper.
    if (!m_board->outlines.isEmpty()) {
        QPainterPath substrate;
        for (const QVector<Point2D> &poly : m_board->outlines) {
            if (poly.size() < 2)
                continue;
            QPainterPath loop;
            loop.moveTo(toPoint(poly.first()));
            for (int i = 1; i < poly.size(); ++i)
                loop.lineTo(toPoint(poly.at(i)));
            substrate.addPath(loop);
        }
        substrate.setFillRule(Qt::WindingFill);
        auto *fill = boardScene->addPath(substrate, Qt::NoPen, QBrush(kBoardFill));
        fill->setZValue(kZBoard);
    }

    // --- copper -----------------------------------------------------------
    for (const Track &track : m_board->tracks) {
        if (track.points.size() < 2)
            continue;
        QPainterPath path;
        path.moveTo(toPoint(track.points.first()));
        for (int i = 1; i < track.points.size(); ++i)
            path.lineTo(toPoint(track.points.at(i)));
        const double width = track.width > 0.0 ? track.width : 0.1;
        auto *item = boardScene->addPath(path, QPen(kTrack, width, Qt::SolidLine, Qt::RoundCap,
                                                    Qt::RoundJoin),
                                         Qt::NoBrush);
        item->setZValue(kZTracks);
    }
    for (const Via &via : m_board->vias) {
        const double diameter = via.diameter > 0.0 ? via.diameter : 0.4;
        QPainterPath path;
        path.addEllipse(QPointF(via.position.x, via.position.y), diameter / 2.0, diameter / 2.0);
        if (via.drill > 0.0 && via.drill < diameter)
            path.addEllipse(QPointF(via.position.x, via.position.y), via.drill / 2.0,
                            via.drill / 2.0);
        auto *item = boardScene->addPath(path, QPen(kText, 0.05), QBrush(kVia));
        item->setZValue(kZVias);
    }

    // --- footprints --------------------------------------------------------
    for (const Component &component : m_board->components) {
        auto *group = new ComponentItem(&component);
        boardScene->addItem(group);
        m_componentItems.insert(component.uid, group);

        QRectF hull;
        for (const Pad &pad : component.pads) {
            const QPainterPath path = padPath(pad);
            if (path.isEmpty())
                continue;
            auto *item = new QGraphicsPathItem(path, group);
            item->setPen(QPen(kBackground, 0.02));
            item->setBrush(QBrush(kPad));
            // The path is centred on its own origin, so rotating the item turns
            // the pad about its own middle.
            item->setPos(component.position.x + pad.position.x,
                         component.position.y + pad.position.y);
            item->setRotation(pad.rotation + component.rotation);
            item->setZValue(kZPads);
            hull |= item->sceneBoundingRect();
        }

        if (hull.isEmpty()) {
            // A footprint with no pads (or no geometry in the file) still has to
            // be visible and clickable, so it gets a marker at its placement.
            hull = QRectF(component.position.x - 1.0, component.position.y - 1.0, 2.0, 2.0);
        } else {
            hull.adjust(-0.3, -0.3, 0.3, 0.3);
        }
        group->setHull(hull);

        auto *body = new QGraphicsRectItem(hull, group);
        body->setPen(QPen(sideColor(component.side), 0.15, Qt::SolidLine, Qt::SquareCap,
                          Qt::MiterJoin));
        body->setBrush(QBrush(kBody));
        body->setZValue(kZBody);

        // The reference is real geometry, so it zooms with the board, but it is
        // sized from the footprint so that it stays readable.
        auto *label = new QGraphicsSimpleTextItem(component.reference, group);
        label->setBrush(kLabel);
        const double wanted = std::max(0.9, hull.height() * 0.4);
        const double natural = std::max(label->boundingRect().height(), 0.001);
        label->setScale(wanted / natural);
        label->setPos(hull.center().x() - label->boundingRect().width() * label->scale() / 2.0,
                      hull.center().y() - wanted / 2.0);
        label->setZValue(kZLabel);
    }

    applyLabelMirroring();

    // --- the board outline on top ------------------------------------------
    for (const QVector<Point2D> &poly : m_board->outlines) {
        if (poly.size() < 2)
            continue;
        QPolygonF polygon;
        for (const Point2D &point : poly)
            polygon << toPoint(point);
        auto *item = boardScene->addPolygon(polygon, QPen(kOutline, 0.25), Qt::NoBrush);
        item->setZValue(kZOutline);
    }

    applyHighlight();
}

void BoardView::applyTransform()
{
    const double scaleX = m_side == BoardSide::Bottom ? -m_scale : m_scale;
    setTransform(QTransform(scaleX, 0, 0, m_scale, 0, 0));
    emit zoomChanged(m_scale);
}

double BoardView::zoomFactor() const { return m_scale; }

void BoardView::setZoomFactor(double factor)
{
    const double clamped = std::clamp(factor, kMinScale, kMaxScale);
    if (qFuzzyCompare(clamped, m_scale))
        return;
    m_scale = clamped;
    applyTransform();
}

void BoardView::setSide(BoardSide side)
{
    if (m_side == side)
        return;
    m_side = side;
    applyTransform();
    applyLabelMirroring();
}

void BoardView::fitToBoard()
{
    if (!m_board)
        return;
    const QRectF box = scene()->sceneRect();
    if (box.isEmpty())
        return;
    if (viewport()->width() <= 0 || viewport()->height() <= 0) {
        // The widget has not been laid out yet; fit again on the first resize.
        m_fitPending = true;
        return;
    }
    const double scaleX = viewport()->width() / std::max(1.0, box.width());
    const double scaleY = viewport()->height() / std::max(1.0, box.height());
    m_scale = std::clamp(std::min(scaleX, scaleY) * 0.95, kMinScale, kMaxScale);
    m_fitPending = false;
    applyTransform();
    centerOn(box.center());
}

void BoardView::resetView()
{
    m_scale = kDefaultScale;
    m_fitPending = false;
    applyTransform();
    if (m_board)
        centerOn(scene()->sceneRect().center());
}

void BoardView::zoomIn() { setZoomFactor(m_scale * 1.25); }
void BoardView::zoomOut() { setZoomFactor(m_scale / 1.25); }

void BoardView::setSelectedComponent(const QString &uid)
{
    m_selectedUid = uid;
    applyHighlight();
    if (!uid.isEmpty()) {
        auto it = m_componentItems.constFind(uid);
        if (it != m_componentItems.cend())
            ensureVisible(it.value());
    }
}

void BoardView::setHoveredComponent(const QString &uid)
{
    if (m_hoveredUid == uid)
        return;
    m_hoveredUid = uid;
    applyHighlight();
}

void BoardView::clearDiagnosticHighlights()
{
    // Reserved for the diagnostic overlay; there is nothing to clear yet.
}

void BoardView::applyHighlight()
{
    for (auto it = m_componentItems.cbegin(); it != m_componentItems.cend(); ++it) {
        auto *group = static_cast<ComponentItem *>(it.value());
        const bool selected = it.key() == m_selectedUid;
        const bool hovered = it.key() == m_hoveredUid;
        group->setSelected(selected);

        for (QGraphicsItem *child : group->childItems()) {
            if (auto *body = qgraphicsitem_cast<QGraphicsRectItem *>(child)) {
                const QColor base = sideColor(group->component()->side);
                if (selected) {
                    body->setPen(QPen(kText, 0.35, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
                    body->setBrush(QColor(kText.red(), kText.green(), kText.blue(), 70));
                } else if (hovered) {
                    body->setPen(QPen(kText, 0.2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
                    body->setBrush(QColor(base.red(), base.green(), base.blue(), 90));
                } else {
                    body->setPen(QPen(base, 0.15, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
                    body->setBrush(QBrush(kBody));
                }
            } else if (auto *label = qgraphicsitem_cast<QGraphicsSimpleTextItem *>(child)) {
                label->setBrush(selected || hovered ? kText : kLabel);
            }
        }
    }
}

void BoardView::applyLabelMirroring()
{
    // The whole view is mirrored when the back of the board is shown, and text
    // would come out backwards. Each label is flipped back about its own middle,
    // so it stays over its component and stays readable.
    const QTransform flip = m_side == BoardSide::Bottom
                                ? QTransform(-1.0, 0.0, 0.0, 1.0, 0.0, 0.0)
                                : QTransform();
    for (auto it = m_componentItems.cbegin(); it != m_componentItems.cend(); ++it) {
        for (QGraphicsItem *child : it.value()->childItems()) {
            auto *label = qgraphicsitem_cast<QGraphicsSimpleTextItem *>(child);
            if (label == nullptr)
                continue;
            const double width = label->boundingRect().width() * label->scale();
            QTransform mirrored = flip;
            if (m_side == BoardSide::Bottom)
                mirrored = QTransform(-1.0, 0.0, 0.0, 1.0, width, 0.0);
            label->setTransform(mirrored);
        }
    }
}

QString BoardView::componentAt(const QPointF &scenePosition) const
{
    // Tested front to back, so the footprint that is drawn last wins.
    for (auto it = m_componentItems.cbegin(); it != m_componentItems.cend(); ++it) {
        QGraphicsItem *item = it.value();
        if (item->isVisible() && item->contains(item->mapFromScene(scenePosition)))
            return it.key();
    }
    return QString();
}

void BoardView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const QPointF before = mapToScene(event->position().toPoint());
        const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        setZoomFactor(m_scale * factor);
        const QPointF after = mapToScene(event->position().toPoint());
        const QPointF delta = after - before;
        const double direction = m_side == BoardSide::Bottom ? -1.0 : 1.0;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value()
                                        + int(delta.x() * m_scale * direction));
        verticalScrollBar()->setValue(verticalScrollBar()->value() + int(delta.y() * m_scale));
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void BoardView::mousePressEvent(QMouseEvent *event)
{
    const QPointF scenePosition = mapToScene(event->position().toPoint());
    const QString uid = componentAt(scenePosition);
    if (event->button() == Qt::LeftButton) {
        m_selectedUid = uid;
        applyHighlight();
        emit componentClicked(uid);
    }
    QGraphicsView::mousePressEvent(event);
}

void BoardView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF scenePosition = mapToScene(event->position().toPoint());
    QPointF boardPosition = scenePosition;
    if (m_side == BoardSide::Bottom)
        boardPosition.setX(-boardPosition.x());
    emit cursorPositionChanged(boardPosition);

    // The view only tracks hovering; who is "hovered" for the rest of the
    // application is decided by the window.
    QGraphicsView::mouseMoveEvent(event);
}

void BoardView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, kBackground);

    // A 1 mm grid, thinned to 10 mm when it would get denser than ~8 pixels.
    const double gridStep = m_scale < 8.0 ? 10.0 : 1.0;
    QPen pen(kGrid);
    pen.setCosmetic(true);
    painter->setPen(pen);
    const double left = std::ceil(rect.left() / gridStep) * gridStep;
    const double top = std::ceil(rect.top() / gridStep) * gridStep;
    for (double x = left; x < rect.right(); x += gridStep)
        painter->drawLine(QLineF(x, rect.top(), x, rect.bottom()));
    for (double y = top; y < rect.bottom(); y += gridStep)
        painter->drawLine(QLineF(rect.left(), y, rect.right(), y));
}

void BoardView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (m_fitPending && m_board) {
        m_fitPending = false;
        fitToBoard();
    }
}

} // namespace eda
