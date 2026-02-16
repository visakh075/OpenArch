#include "GraphView.h"
#include "GraphNodeItem.h"

#include <QMouseEvent>
#include <QWheelEvent>

GraphView::GraphView(QWidget* parent)
    : QGraphicsView(parent)
{
    setDragMode(QGraphicsView::ScrollHandDrag);
}

void GraphView::setMode(Mode mode)
{
    mode_ = mode;
    connectStartNode_ = nullptr;

    if (mode_ == Mode::View)
        setDragMode(QGraphicsView::ScrollHandDrag);
    else
        setDragMode(QGraphicsView::NoDrag);
}

void GraphView::wheelEvent(QWheelEvent* event)
{
    const double factor = 1.15;

    if (event->angleDelta().y() > 0)
        scale(factor, factor);
    else
        scale(1.0 / factor, 1.0 / factor);
}

void GraphView::mousePressEvent(QMouseEvent* event)
{
    QPointF scenePos = mapToScene(event->pos());

    if (mode_ == Mode::Add) {
        if (!itemAt(event->pos()))
            emit requestAddNode(scenePos);
        return;
    }

    if (mode_ == Mode::Connect) {
        auto* item = itemAt(event->pos());
        auto* node = dynamic_cast<GraphNodeItem*>(item);
        if (node)
            connectStartNode_ = node;
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void GraphView::mouseReleaseEvent(QMouseEvent* event)
{
    if (mode_ == Mode::Connect && connectStartNode_) {

        auto* item = itemAt(event->pos());
        auto* targetNode = dynamic_cast<GraphNodeItem*>(item);

        if (targetNode && targetNode != connectStartNode_) {
            emit requestConnectNodes(
                connectStartNode_->nodeId(),
                targetNode->nodeId()
            );
        }

        connectStartNode_ = nullptr;
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}
