#include "GraphView.h"
#include "GraphNodeItem.h"
#include "GraphThemeManager.h"

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
#include <QMenu>
#include <cmath>

GraphView::GraphView(QWidget* parent)
    : QGraphicsView(parent)
{
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::RubberBandDrag);
    setFocusPolicy(Qt::StrongFocus);
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

    case Mode::Edit:
        setDragMode(QGraphicsView::RubberBandDrag);
        setInteractive(true);
        break;

    case Mode::Add:
        setDragMode(QGraphicsView::NoDrag);
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
    // Space held -> Zoom targeted to mouse cursor
    // Normal scroll -> Zoom focused at center of viewport
    if (spacePressed_)
    {
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    }
    else
    {
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    }

    const double factor = 1.15;

    if (event->angleDelta().y() > 0)
        scale(factor, factor);
    else
        scale(1.0 / factor, 1.0 / factor);

    event->accept();
}

void GraphView::mousePressEvent(QMouseEvent* event)
{
    // Pan only if Middle-Click OR Left-Click while holding Space
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && spacePressed_))
    {
        isPanning_ = true;
        lastPanPoint_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
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

        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());

        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void GraphView::mouseReleaseEvent(QMouseEvent* event)
{
    if (isPanning_ && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton))
    {
        isPanning_ = false;
        setCursor(spacePressed_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void GraphView::contextMenuEvent(QContextMenuEvent* event)
{
    if (itemAt(event->pos()))
    {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    if (mode_ != Mode::Edit)
        return;

    // Capture scene position using viewport mapping BEFORE opening the menu
    QPoint viewportPos = viewport()->mapFromGlobal(event->globalPos());
    QPointF scenePos = mapToScene(viewportPos);

    QMenu menu(this);
    QAction* addNodeAct = menu.addAction("Add Node");
    QAction* addLayerAct = menu.addAction("Add Layer");

    QAction* selected = menu.exec(event->globalPos());

    if (selected == addNodeAct)
        emit requestAddNode(scenePos);
    else if (selected == addLayerAct)
        emit requestAddLayer();
}

void GraphView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space)
    {
        spacePressed_ = true;
        setCursor(Qt::OpenHandCursor);
    }

    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        if (mode() == Mode::Edit)
        {
            emit deleteRequested();
            event->accept();
            return;
        }
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

void GraphView::exportToSvg(ExportMode mode)
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export Diagram", "diagram.svg", "SVG Files (*.svg)");
    if (fileName.isEmpty())
        return;

    QSvgGenerator generator;
    QRectF sourceRect;
    QSize svgSize;
    const int padding = 30;

    if (mode == ExportMode::CurrentView)
    {
        QRect viewportRect = viewport()->rect();
        sourceRect = mapToScene(viewportRect).boundingRect();
        svgSize = viewportRect.size();
    }
    else
    {
        sourceRect = scene()->itemsBoundingRect().adjusted(-padding, -padding, padding, padding);
        svgSize = QSize(static_cast<int>(sourceRect.width()), static_cast<int>(sourceRect.height()));
    }

    generator.setFileName(fileName);
    generator.setSize(svgSize);
    generator.setViewBox(QRect(0, 0, svgSize.width(), svgSize.height()));
    generator.setTitle("OpenArch Diagram");

    QPainter painter(&generator);
    painter.setRenderHint(QPainter::Antialiasing);

    if (mode == ExportMode::CurrentView)
    {
        render(&painter, QRectF(0, 0, svgSize.width(), svgSize.height()), viewport()->rect());
    }
    else
    {
        painter.translate(-sourceRect.topLeft());
        painter.fillRect(sourceRect, GraphThemeManager::instance()->theme().view.background);
        scene()->render(&painter, sourceRect, sourceRect);
    }
    painter.end();
}

void GraphView::drawBackground(QPainter* painter, const QRectF& rect)
{
    const auto& theme = GraphThemeManager::instance()->theme();
    painter->fillRect(rect, theme.view.background);

    if (theme.view.grid.enabled)
    {
        const int gridSize = 20;
        QPen pen(theme.view.grid.majorColor);
        pen.setWidth(1);
        painter->setPen(pen);

        QRect viewportRect = viewport()->rect();
        QPointF topLeft = mapToScene(viewportRect.topLeft());
        QPointF bottomRight = mapToScene(viewportRect.bottomRight());

        int left = static_cast<int>(std::floor(topLeft.x()));
        int right = static_cast<int>(std::ceil(bottomRight.x()));
        int top = static_cast<int>(std::floor(topLeft.y()));
        int bottom = static_cast<int>(std::ceil(bottomRight.y()));

        left -= left % gridSize;
        top -= top % gridSize;

        QVector<QLineF> lines;
        for (int x = left; x < right; x += gridSize)
            lines.append(QLineF(x, top, x, bottom));
        for (int y = top; y < bottom; y += gridSize)
            lines.append(QLineF(left, y, right, y));

        painter->drawLines(lines);
    }
}

void GraphView::moveSelectionTo(const QPointF& target)
{
    QList<QGraphicsItem*> selected = scene()->selectedItems();
    if (selected.isEmpty())
        return;

    QRectF combinedRect;
    bool first = true;

    for (QGraphicsItem* item : selected)
    {
        auto* node = dynamic_cast<GraphNodeItem*>(item);
        if (!node)
            continue;

        QRectF r = node->sceneBoundingRect();
        combinedRect = first ? r : combinedRect.united(r);
        first = false;
    }

    QPointF delta = target - combinedRect.center();
    for (QGraphicsItem* item : selected)
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            node->setPos(node->pos() + delta);
    }
}