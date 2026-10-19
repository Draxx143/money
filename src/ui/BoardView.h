// Central PCB viewer.
//
// The widget owns the board scene, the millimetre <-> pixel transform, zoom and
// pan and the layer visibility. It never loads or parses anything: the board is
// handed in as a const reference to the project model, so the viewer can be
// driven from tests without files, importers or a project document.
#pragma once

#include <QGraphicsView>

#include "../core/ProjectModel.h"

#include <QMap>

class QGraphicsItem;
class QGraphicsSimpleTextItem;
class QLabel;

namespace eda {

class BoardView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit BoardView(QWidget *parent = nullptr);

    const Board *board() const { return m_board; }
    void setBoard(const Board *board);

    // Which side of the board is shown. The bottom view is mirrored exactly the
    // way a technician sees the board when it is turned over; the model data is
    // never modified by it.
    BoardSide side() const { return m_side; }
    void setSide(BoardSide side);

    double zoomFactor() const;
    void setZoomFactor(double factor);

    void fitToBoard();
    void resetView();
    void zoomIn();
    void zoomOut();

    // Which component is drawn at this position, in board millimetres. Empty
    // when there is none. The same test the mouse uses, so a test can check the
    // geometry the user sees.
    QString componentAt(const QPointF &scenePosition) const;

    // Highlights used by the diagnostic workflow.
    QString selectedComponentUid() const { return m_selectedUid; }
    void setSelectedComponent(const QString &uid);
    void setHoveredComponent(const QString &uid);
    void clearDiagnosticHighlights();

signals:
    void componentClicked(const QString &uid);
    void componentHovered(const QString &uid);
    void cursorPositionChanged(const QPointF &positionInMillimetres);
    void zoomChanged(double factor);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // One footprint in the scene: its pads, its body and its reference. Defined
    // in BoardView.cpp because it is an implementation detail of the view.
    class ComponentItem;

    void rebuildScene();
    void applyTransform();
    void applyHighlight();
    // Keeps the reference designators readable while the board is mirrored:
    // the geometry is mirrored, the text is not.
    void applyLabelMirroring();

    const Board *m_board = nullptr;
    QGraphicsSimpleTextItem *m_placeholder = nullptr;
    BoardSide m_side = BoardSide::Top;
    double m_scale = 4.0;   // pixels per millimetre
    QMap<QString, QGraphicsItem *> m_componentItems;
    QString m_selectedUid;
    QString m_hoveredUid;
    bool m_fitPending = false;
};

} // namespace eda
