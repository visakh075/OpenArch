#include "GraphView.h"
#include "GraphNodeItem.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QApplication>
#include <QWidget>

GraphView::GraphView(QWidget* parent)
    : QGraphicsView(parent)
{
    setDragMode(QGraphicsView::NoDrag); // IMPORTANT: we control panning manually
    setFocusPolicy(Qt::StrongFocus);   // ensure key events work
}

void GraphView::setMode(Mode m)
{
    mode_ = m;

    switch (mode_)
    {
    case Mode::View:
        setDragMode(QGraphicsView::ScrollHandDrag);
        setInteractive(true);
        break;

    case Mode::Layout:
        setDragMode(QGraphicsView::RubberBandDrag);
        setInteractive(true);
        break;

    case Mode::Add:
        setDragMode(QGraphicsView::ScrollHandDrag);
        setInteractive(false);
        break;

    case Mode::Connect:
        setDragMode(QGraphicsView::NoDrag);
        setInteractive(false);
        break;

    case Mode::Arch:
        setDragMode(QGraphicsView::RubberBandDrag);
        setInteractive(true);
        break;
    }
}

void GraphView::wheelEvent(QWheelEvent* event)
{
    const double factor = 1.15;

    if (event->angleDelta().y() > 0)
        scale(factor, factor);
    else
        scale(1.0 / factor, 1.0 / factor);
}

// =========================
// 🖱️ MOUSE PRESS
// =========================
void GraphView::mousePressEvent(QMouseEvent* event)
{
    QPointF scenePos = mapToScene(event->pos());

    // ✅ PAN (Space or Middle Mouse)
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && spacePressed_))
    {
        isPanning_ = true;
        lastPanPoint_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }


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

// =========================
// 🖱️ MOUSE MOVE
// =========================
void GraphView::mouseMoveEvent(QMouseEvent* event)
{
    if (isPanning_)
    {
        QPoint delta = event->pos() - lastPanPoint_;
        lastPanPoint_ = event->pos();

        horizontalScrollBar()->setValue(
            horizontalScrollBar()->value() - delta.x());

        verticalScrollBar()->setValue(
            verticalScrollBar()->value() - delta.y());

        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

// =========================
// 🖱️ MOUSE RELEASE
// =========================
void GraphView::mouseReleaseEvent(QMouseEvent* event)
{
    // Stop panning
    if (isPanning_ &&
        (event->button() == Qt::LeftButton ||
         event->button() == Qt::MiddleButton))
    {
        isPanning_ = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

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

// =========================
// ⌨️ KEY EVENTS
// =========================
void GraphView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space)
    {
        spacePressed_ = true;
        setCursor(Qt::OpenHandCursor);
    }

    QGraphicsView::keyPressEvent(event);
}

void GraphView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space)
    {
        spacePressed_ = false;
        setCursor(Qt::ArrowCursor);
    }

    QGraphicsView::keyReleaseEvent(event);
}