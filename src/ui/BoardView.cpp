#include "BoardView.h"

#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

namespace eda {

BoardView::BoardView(QWidget *parent) : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setBackgroundBrush(QColor(QStringLiteral("#0B0F14")));
    setMouseTracking(true);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

    auto *placeholder = new QGraphicsSimpleTextItem(tr("No board loaded"));
    placeholder->setBrush(QColor(QStringLiteral("#94A3B8")));
    placeholder->setData(0, QStringLiteral("placeholder"));
    scene()->addItem(placeholder);
    m_placeholder = placeholder;
}

void BoardView::setBoard(const Board *board)
{
    m_board = board;
    m_selectedUid.clear();
    m_hoveredUid.clear();
    rebuildScene();
    if (board)
        fitToBoard();
}

void BoardView::rebuildScene()
{
    scene()->clear();
    m_componentItems.clear();
    m_placeholder = nullptr;

    if (!m_board) {
        auto *placeholder = new QGraphicsSimpleTextItem(tr("No board loaded"));
        placeholder->setBrush(QColor(QStringLiteral("#94A3B8")));
        scene()->addItem(placeholder);
        m_placeholder = placeholder;
        scene()->setSceneRect(QRectF(-100, -60, 200, 120));
        return;
    }

    // Milestone 2 replaces this stub with real geometry (outline, pads, tracks,
    // zones and silkscreen). Until then the viewer draws the board outline and
    // the component outlines that the importer produced, so that a board with
    // geometry is visible immediately.
    const double margin = 5.0;
    QRectF box(m_board->boundingBox.x, m_board->boundingBox.y, m_board->boundingBox.width,
               m_board->boundingBox.height);
    if (box.isEmpty()) {
        for (const Component &component : m_board->components) {
            const QPointF point(component.position.x, component.position.y);
            box = box.isEmpty() ? QRectF(point, QSizeF(1, 1)) : box.united(QRectF(point, QSizeF(1, 1)));
        }
        box.adjust(-margin, -margin, margin, margin);
    }
    scene()->setSceneRect(box.adjusted(-margin, -margin, margin, margin));

    QPen outlinePen(QColor(QStringLiteral("#00D1B2")));
    outlinePen.setCosmetic(true);
    outlinePen.setWidthF(1.2);
    scene()->addRect(box, outlinePen, Qt::NoBrush);

    for (const Component &component : m_board->components) {
        QRectF rect(component.position.x - 1.0, component.position.y - 1.0, 2.0, 2.0);
        auto *item = scene()->addRect(rect, QPen(QColor(QStringLiteral("#4DA3FF"))),
                                      QBrush(QColor(0x18, 0x22, 0x2E)));
        item->setData(0, component.uid);
        item->setFlag(QGraphicsItem::ItemIsSelectable, true);
        m_componentItems.insert(component.uid, item);
    }
}

void BoardView::applyTransform()
{
    const double scaleX = m_side == BoardSide::Bottom ? -m_scale : m_scale;
    QTransform transform(scaleX, 0, 0, m_scale, 0, 0);
    setTransform(transform);
    emit zoomChanged(m_scale);
}

double BoardView::zoomFactor() const { return m_scale; }

void BoardView::setZoomFactor(double factor)
{
    const double clamped = qBound(0.05, factor, 400.0);
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
}

void BoardView::fitToBoard()
{
    if (!m_board)
        return;
    const QRectF box = scene()->sceneRect();
    if (box.isEmpty())
        return;
    const double scaleX = viewport()->width() / qMax(1.0, box.width());
    const double scaleY = viewport()->height() / qMax(1.0, box.height());
    m_scale = qBound(0.05, qMin(scaleX, scaleY) * 0.95, 400.0);
    applyTransform();
    centerOn(box.center());
}

void BoardView::resetView()
{
    m_scale = 4.0;
    applyTransform();
    if (m_board)
        centerOn(scene()->sceneRect().center());
}

void BoardView::zoomIn() { setZoomFactor(m_scale * 1.25); }
void BoardView::zoomOut() { setZoomFactor(m_scale / 1.25); }

void BoardView::setSelectedComponent(const QString &uid)
{
    m_selectedUid = uid;
    for (auto it = m_componentItems.cbegin(); it != m_componentItems.cend(); ++it)
        it.value()->setSelected(it.key() == uid);
    if (!uid.isEmpty()) {
        auto it = m_componentItems.constFind(uid);
        if (it != m_componentItems.cend())
            ensureVisible(it.value());
    }
}

void BoardView::setHoveredComponent(const QString &uid) { m_hoveredUid = uid; }

void BoardView::clearDiagnosticHighlights() {}

QString BoardView::componentAt(const QPointF &scenePosition) const
{
    for (auto it = m_componentItems.cbegin(); it != m_componentItems.cend(); ++it) {
        if (it.value()->boundingRect().contains(scenePosition))
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
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + int(delta.x() * m_scale * direction));
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
        emit componentClicked(uid);
    }
    QGraphicsView::mousePressEvent(event);
}

void BoardView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF scenePosition = mapToScene(event->position().toPoint());
    emit cursorPositionChanged(scenePosition);
    const QString uid = componentAt(scenePosition);
    if (uid != m_hoveredUid) {
        m_hoveredUid = uid;
        emit componentHovered(uid);
    }
    QGraphicsView::mouseMoveEvent(event);
}

void BoardView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, QColor(QStringLiteral("#0B0F14")));

    // 1 mm grid, thinned out when it would become denser than 8 pixels.
    const double gridStep = m_scale < 8.0 ? 10.0 : 1.0;
    const QRectF sceneBox = scene()->sceneRect();
    QPen pen(QColor(0x28, 0x35, 0x44));
    pen.setCosmetic(true);
    painter->setPen(pen);
    const double left = qMax(rect.left(), sceneBox.left());
    const double right = qMin(rect.right(), sceneBox.right());
    const double top = qMax(rect.top(), sceneBox.top());
    const double bottom = qMin(rect.bottom(), sceneBox.bottom());
    for (double x = qCeil(left / gridStep) * gridStep; x < right; x += gridStep)
        painter->drawLine(QLineF(x, top, x, bottom));
    for (double y = qCeil(top / gridStep) * gridStep; y < bottom; y += gridStep)
        painter->drawLine(QLineF(left, y, right, y));
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
