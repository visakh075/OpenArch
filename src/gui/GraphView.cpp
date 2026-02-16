#include "GraphView.h"
#include <QWheelEvent>

GraphView::GraphView(QWidget* parent)
    : QGraphicsView(parent)
{
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::TextAntialiasing);

    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorUnderMouse);

    setDragMode(NoDrag);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
}

void GraphView::wheelEvent(QWheelEvent* event)
{
    if (!(event->modifiers() & Qt::ControlModifier)) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    constexpr double zoomFactor = 1.15;

    if (event->angleDelta().y() > 0) {
        if (currentScale_ < maxScale_) {
            scale(zoomFactor, zoomFactor);
            currentScale_ *= zoomFactor;
        }
    } else {
        if (currentScale_ > minScale_) {
            scale(1.0 / zoomFactor, 1.0 / zoomFactor);
            currentScale_ /= zoomFactor;
        }
    }

    event->accept();
}
