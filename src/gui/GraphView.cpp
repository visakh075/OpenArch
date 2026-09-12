#include "GraphView.h"
#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"
#include "GraphThemeManager.h"

#include <QMouseEvent>
#include <QTextStream>
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
    setCacheMode(QGraphicsView::CacheNone);
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

        auto* hBar = horizontalScrollBar();
        auto* vBar = verticalScrollBar();

        int oldH = hBar->value();
        int oldV = vBar->value();

        hBar->setValue(oldH - delta.x());
        vBar->setValue(oldV - delta.y());

        if (hBar->value() == oldH || vBar->value() == oldV)
        {
            QPointF sceneDelta = mapToScene(delta) - mapToScene(QPoint(0, 0));
            setSceneRect(sceneRect().translated(-sceneDelta.x(), -sceneDelta.y()));
        }

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

void GraphView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (mode_ == Mode::View)
    {
        event->accept();
        return;
    }

    QGraphicsView::mouseDoubleClickEvent(event);
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

void GraphView::exportToInteractiveHtml()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "Export Interactive HTML", "architecture.html", "HTML Files (*.html)");
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;

    const auto& theme = GraphThemeManager::instance()->theme();
    const int padding = 40;
    QRectF sceneBounds = scene()->itemsBoundingRect().adjusted(-padding, -padding, padding, padding);
    if (sceneBounds.isNull() || sceneBounds.isEmpty())
        sceneBounds = QRectF(0, 0, 800, 600);

    QTextStream out(&file);

    out << "<!DOCTYPE html>\n<html>\n<head>\n";
    out << "<meta charset=\"utf-8\"/>\n";
    out << "<title>OpenArch Diagram</title>\n";
    out << "<style>\n";
    out << "  body { margin: 0; padding: 0; background: " << theme.view.background.name() 
        << "; display: flex; justify-content: center; align-items: center; min-height: 100vh; overflow: auto; font-family: sans-serif; }\n";
    out << "  svg { background: " << theme.view.background.name() 
        << "; box-shadow: 0 4px 20px rgba(0,0,0,0.3); border-radius: 8px; }\n";

    out << "  .graph-node { cursor: pointer; }\n";
    out << "  .graph-node rect { fill: " << theme.node.normal.background.name(QColor::HexRgb) 
        << "; stroke: " << theme.node.normal.border.name() 
        << "; stroke-width: " << theme.node.normal.borderWidth 
        << "; rx: " << theme.node.normal.radius << "px; transition: all 0.15s ease; }\n";
    out << "  .graph-node:hover rect { fill: " << theme.node.hover.background.name() 
        << "; stroke: " << theme.node.hover.border.name() 
        << "; stroke-width: " << theme.node.hover.borderWidth 
        << "; filter: drop-shadow(0 0 6px " << theme.node.hover.border.name() << "); }\n";
    out << "  .graph-node .title { fill: " << theme.node.normal.title.color.name() 
        << "; font-size: " << theme.node.normal.title.size 
        << "px; font-weight: " << (theme.node.normal.title.bold ? "bold" : "normal") 
        << "; pointer-events: none; }\n";
    out << "  .graph-node:hover .title { fill: " << theme.node.hover.title.color.name() << "; }\n";
    out << "  .graph-node .body { fill: " << theme.node.normal.body.color.name() 
        << "; font-size: " << theme.node.normal.body.size << "px; pointer-events: none; }\n";
    out << "  .graph-node:hover .body { fill: " << theme.node.hover.body.color.name() << "; }\n";

    out << "  .graph-edge { cursor: pointer; }\n";
    out << "  .graph-edge path.line { fill: none; stroke: " << theme.edge.normal.lineColor.name() 
        << "; stroke-width: " << theme.edge.normal.lineWidth << "; transition: all 0.15s ease; }\n";
    out << "  .graph-edge polygon.arrow { fill: " << theme.edge.normal.arrow.fillColor.name() 
        << "; stroke: " << theme.edge.normal.arrow.lineColor.name() 
        << "; stroke-width: " << theme.edge.normal.arrow.lineWidth << "; transition: all 0.15s ease; }\n";
    out << "  .graph-edge:hover path.line { stroke: " << theme.edge.hover.lineColor.name() 
        << "; stroke-width: " << theme.edge.hover.lineWidth 
        << "; filter: drop-shadow(0 0 4px " << theme.edge.hover.lineColor.name() << "); }\n";
    out << "  .graph-edge:hover polygon.arrow { fill: " << theme.edge.hover.arrow.fillColor.name() 
        << "; stroke: " << theme.edge.hover.arrow.lineColor.name() << "; }\n";
    out << "  .graph-edge text { fill: " << theme.edge.normal.label.textColor.name() 
        << "; font-size: " << theme.edge.normal.label.fontSize << "px; pointer-events: none; }\n";
    out << "  .graph-edge:hover text { fill: " << theme.edge.hover.label.textColor.name() 
        << "; font-weight: bold; }\n";
    out << "</style>\n</head>\n<body>\n";

    out << "<svg width=\"" << sceneBounds.width() << "\" height=\"" << sceneBounds.height() 
        << "\" viewBox=\"" << sceneBounds.left() << " " << sceneBounds.top() << " " 
        << sceneBounds.width() << " " << sceneBounds.height() << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";

    for (QGraphicsItem* item : scene()->items())
    {
        auto* edge = dynamic_cast<GraphEdgeItem*>(item);
        if (!edge) continue;

        QPainterPath fullPath = edge->edgePath();
        if (fullPath.elementCount() < 2) continue;

        QPainterPath::Element e1 = fullPath.elementAt(fullPath.elementCount() - 1);
        QPainterPath::Element e2 = fullPath.elementAt(fullPath.elementCount() - 2);

        QPointF last = edge->mapToScene(QPointF(e1.x, e1.y));
        QPointF prev = edge->mapToScene(QPointF(e2.x, e2.y));
        QLineF lastSegment(prev, last);

        double angle = std::atan2(-lastSegment.dy(), lastSegment.dx());
        double arrowWidth = theme.edge.normal.arrow.width;
        double arrowHeight = theme.edge.normal.arrow.height;

        QPointF arrowTip = last;
        QPointF arrowP1 = arrowTip - QPointF(
            std::cos(angle) * arrowWidth - std::sin(angle) * arrowHeight / 2.0,
            -std::sin(angle) * arrowWidth - std::cos(angle) * arrowHeight / 2.0);
        QPointF arrowP2 = arrowTip - QPointF(
            std::cos(angle) * arrowWidth + std::sin(angle) * arrowHeight / 2.0,
            -std::sin(angle) * arrowWidth + std::cos(angle) * arrowHeight / 2.0);
        QPointF arrowBase = (arrowP1 + arrowP2) / 2.0;

        QString d;
        for (int i = 0; i < fullPath.elementCount(); ++i)
        {
            auto elem = fullPath.elementAt(i);
            QPointF p = edge->mapToScene(QPointF(elem.x, elem.y));
            if (i == fullPath.elementCount() - 1)
                p = arrowBase;

            if (elem.isMoveTo() || i == 0)
                d += QString("M %1 %2 ").arg(p.x()).arg(p.y());
            else
                d += QString("L %1 %2 ").arg(p.x()).arg(p.y());
        }

        out << "  <g class=\"graph-edge\">\n";
        out << "    <path class=\"line\" d=\"" << d << "\"/>\n";
        out << "    <polygon class=\"arrow\" points=\"" 
            << arrowTip.x() << "," << arrowTip.y() << " " 
            << arrowP1.x() << "," << arrowP1.y() << " " 
            << arrowP2.x() << "," << arrowP2.y() << "\"/>\n";

        QPointF midPoint = edge->mapToScene(fullPath.pointAtPercent(0.5));
        QString edgeTitle = edge->title();
        if (!edgeTitle.isEmpty())
        {
            out << "    <text x=\"" << midPoint.x() << "\" y=\"" << (midPoint.y() - 8) 
                << "\" text-anchor=\"middle\">" 
                << edgeTitle.toHtmlEscaped() << "</text>\n";
        }
    }

    for (QGraphicsItem* item : scene()->items())
    {
        auto* node = dynamic_cast<GraphNodeItem*>(item);
        if (!node) continue;

        QPointF pos = node->pos();
        QRectF localRect = node->boundingRect();
        qreal x = pos.x();
        qreal y = pos.y();
        qreal w = localRect.width();
        qreal h = localRect.height();

        out << "  <g class=\"graph-node\" transform=\"translate(" << x << "," << y << ")\">\n";
        out << "    <rect width=\"" << w << "\" height=\"" << h << "\"/>\n";
        out << "    <text class=\"title\" x=\"" << (w / 2.0) << "\" y=\"" << (h / 2.0 - 4) 
            << "\" text-anchor=\"middle\" dominant-baseline=\"middle\">" 
            << node->displayTitle().toHtmlEscaped() << "</text>\n";
        out << "    <text class=\"body\" x=\"" << (w / 2.0) << "\" y=\"" << (h / 2.0 + 14) 
            << "\" text-anchor=\"middle\" dominant-baseline=\"middle\">" 
            << node->displayType().toHtmlEscaped() << "</text>\n";
        out << "  </g>\n";
    }

    out << "</svg>\n</body>\n</html>\n";
    file.close();
}