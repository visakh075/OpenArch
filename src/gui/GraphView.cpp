#include "GraphView.h"
#include "GraphNodeItem.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QApplication>
#include <QWidget>
#include <QFileDialog>
#include <QSvgGenerator>
#include <QPainter>
#include <QPen>
#include <QGraphicsScene>
#include <cmath>
#include "GraphThemeManager.h"

void GraphView::exportToSvg(ExportMode mode)
{
    QString fileName =
        QFileDialog::getSaveFileName(
            this,
            "Export Diagram",
            "diagram.svg",
            "SVG Files (*.svg)");

    if (fileName.isEmpty())
        return;

    QSvgGenerator generator;

    QRectF sourceRect;

    QSize svgSize;

    const int padding = 30;

    if (mode == ExportMode::CurrentView)
    {
        QRect viewportRect =
            viewport()->rect();

        sourceRect =
            mapToScene(viewportRect)
                .boundingRect();

        //
        // exact viewport size
        //
        svgSize = viewportRect.size();
    }
    else
    {
        sourceRect =
            scene()->itemsBoundingRect()
                .adjusted(
                    -padding,
                    -padding,
                    padding,
                    padding);

        svgSize = QSize(
            static_cast<int>(sourceRect.width()),
            static_cast<int>(sourceRect.height()));
    }

    generator.setFileName(fileName);

    generator.setSize(svgSize);

    generator.setViewBox(
        QRect(
            0,
            0,
            svgSize.width(),
            svgSize.height()));

    generator.setTitle("OpenArch Diagram");

    QPainter painter(&generator);

    painter.setRenderHint(
        QPainter::Antialiasing);

    if (mode == ExportMode::CurrentView)
    {
        QRect viewportRect =
            viewport()->rect();

        svgSize = viewportRect.size();

        generator.setSize(svgSize);

        generator.setViewBox(
            QRect(
                0,
                0,
                svgSize.width(),
                svgSize.height()));

        //
        // render exact viewport pixels
        //
        render(
            &painter,
            QRectF(
                0,
                0,
                svgSize.width(),
                svgSize.height()),
            viewportRect);
    }
    else
    {
        //
        // move scene into local svg coordinates
        //
        painter.translate(
            -sourceRect.topLeft());

        //
        // background
        //
        painter.fillRect(
            sourceRect,
            GraphThemeManager::instance()
                ->theme()
                .view
                .background);

        //
        // render scene only
        //
        scene()->render(
            &painter,
            sourceRect,
            sourceRect);
    }

    painter.end();
}

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

void GraphView::mousePressEvent(QMouseEvent* event)
{
    QPointF scenePos = mapToScene(event->pos());

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
void GraphView::drawBackground(
    QPainter* painter,
    const QRectF& rect)
{
    Q_UNUSED(rect);

    //
    // background
    //

    const auto& theme =
        GraphThemeManager::instance()->theme();

    painter->fillRect(
        sceneRect(),
        theme.view.background);



    //
    // grid pen
    //
    if(theme.view.grid.enabled)
    {
        const int gridSize = 20;

        QPen pen(theme.view.grid.majorColor);
        
        pen.setWidth(1);

        painter->setPen(pen);

        QRect viewportRect = viewport()->rect();

        QPointF topLeft =
            mapToScene(viewportRect.topLeft());

        QPointF bottomRight =
            mapToScene(viewportRect.bottomRight());

        int left =
            static_cast<int>(std::floor(topLeft.x()));

        int right =
            static_cast<int>(std::ceil(bottomRight.x()));

        int top =
            static_cast<int>(std::floor(topLeft.y()));

        int bottom =
            static_cast<int>(std::ceil(bottomRight.y()));

        left -= left % gridSize;
        top -= top % gridSize;

        QVector<QLineF> lines;

        for (int x = left; x < right; x += gridSize)
        {
            lines.append(QLineF(x, top, x, bottom));
        }

        for (int y = top; y < bottom; y += gridSize)
        {
            lines.append(QLineF(left, y, right, y));
        }

        painter->drawLines(lines);

    }
    
}

void GraphView::moveSelectionTo(
    const QPointF& target)
{
    QList<QGraphicsItem*> selected =
        scene()->selectedItems();

    if (selected.isEmpty())
        return;

    QRectF combinedRect;

    bool first = true;

    for (QGraphicsItem* item : selected)
    {
        auto* node =
            dynamic_cast<GraphNodeItem*>(item);

        if (!node)
            continue;

        QRectF r =
            node->sceneBoundingRect();

        if (first)
        {
            combinedRect = r;
            first = false;
        }
        else
        {
            combinedRect =
                combinedRect.united(r);
        }
    }

    QPointF currentCenter =
        combinedRect.center();

    QPointF delta =
        target - currentCenter;

    for (QGraphicsItem* item : selected)
    {
        auto* node =
            dynamic_cast<GraphNodeItem*>(item);

        if (!node)
            continue;

        node->setPos(
            node->pos() + delta);
    }
}