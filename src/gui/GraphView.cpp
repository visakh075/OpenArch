#include "GraphView.h"
#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"
#include "GraphThemeManager.h"
#include "MainWindow.h"

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

static QString toCssRgba(const QColor& c)
{
    return QString("rgba(%1, %2, %3, %4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(c.alphaF(), 0, 'f', 2);
}

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

    auto* mainWin = dynamic_cast<MainWindow*>(window());
    QPointF sceneClickPos = mapToScene(event->pos());

    QMenu menu(this);
    QAction* pasteAct = menu.addAction(QIcon(":/icons/paste.svg"), "Paste Here");
    pasteAct->setEnabled(mainWin && mainWin->hasClipboard());

    QAction* addNodeAct = menu.addAction("Add Node Here");

    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == pasteAct && mainWin)
    {
        mainWin->pasteNodesAt(sceneClickPos);
    }
    else if (chosen == addNodeAct)
    {
        emit requestAddNode(sceneClickPos);
    }
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
        const int gridSize = theme.view.grid.spacing > 0 ? theme.view.grid.spacing : 20;
        QPen pen(theme.view.grid.majorColor);
        pen.setWidth(theme.view.grid.lineWidth);
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
    const int padding = 60;
    QRectF sceneBounds = scene()->itemsBoundingRect().adjusted(-padding, -padding, padding, padding);
    if (sceneBounds.isNull() || sceneBounds.isEmpty())
        sceneBounds = QRectF(0, 0, 1000, 700);

    std::vector<GraphNodeItem*> containers;
    std::vector<GraphNodeItem*> leafNodes;
    std::vector<GraphEdgeItem*> edges;

    for (QGraphicsItem* item : scene()->items())
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
        {
            if (node->isContainer())
                containers.push_back(node);
            else
                leafNodes.push_back(node);
        }
        else if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
        {
            edges.push_back(edge);
        }
    }

    QTextStream out(&file);

    out << "<!DOCTYPE html>\n<html>\n<head>\n";
    out << "<meta charset=\"utf-8\"/>\n";
    out << "<title>OpenArch Diagram</title>\n";
    out << "<style>\n";
    out << "  * { box-sizing: border-box; }\n";
    out << "  body { margin: 0; padding: 0; background: " << toCssRgba(theme.view.background) 
        << "; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; overflow: hidden; width: 100vw; height: 100vh; display: flex; }\n";
    out << "  #canvas-container { flex: 1; height: 100%; position: relative; cursor: grab; overflow: hidden; }\n";
    out << "  #canvas-container:active { cursor: grabbing; }\n";
    out << "  svg { width: 100%; height: 100%; display: block; }\n";

    // --- Leaf Nodes ---
    out << "  .graph-node { cursor: pointer; }\n";
    out << "  .graph-node rect.body { fill: " << toCssRgba(theme.node.normal.background) 
        << "; stroke: " << toCssRgba(theme.node.normal.border) 
        << "; stroke-width: " << theme.node.normal.borderWidth 
        << "px; rx: " << theme.node.normal.radius << "px; transition: all 0.15s ease; }\n";
    out << "  .graph-node:hover rect.body { fill: " << toCssRgba(theme.node.hover.background)
        << "; stroke: " << toCssRgba(theme.node.hover.border) 
        << "; stroke-width: " << theme.node.hover.borderWidth 
        << "px; filter: drop-shadow(0 0 6px " << toCssRgba(theme.node.hover.border) << "); }\n";
    out << "  .graph-node text.title { fill: " << toCssRgba(theme.node.normal.title.color) 
        << "; font-size: " << theme.node.normal.title.size 
        << "px; font-weight: " << (theme.node.normal.title.bold ? "bold" : "normal") 
        << "; pointer-events: none; }\n";
    out << "  .graph-node:hover text.title { fill: " << toCssRgba(theme.node.hover.title.color) << "; }\n";
    out << "  .graph-node text.body { fill: " << toCssRgba(theme.node.normal.body.color) 
        << "; font-size: " << theme.node.normal.body.size << "px; pointer-events: none; }\n";
    out << "  .graph-node:hover text.body { fill: " << toCssRgba(theme.node.hover.body.color) << "; }\n";

    // --- Containers (Fully Theme-Driven) ---
    const auto& cNormal = theme.container.normal;
    const auto& cHover = theme.container.hover;

    QString dashArray = "none";
    if (cNormal.borderStyle == Qt::DashLine || !cNormal.dashPattern.isEmpty())
        dashArray = "6 4";

    out << "  .graph-container rect.box { fill: " << toCssRgba(cNormal.background) 
        << "; stroke: " << toCssRgba(cNormal.border) 
        << "; stroke-width: " << cNormal.borderWidth 
        << "px; stroke-dasharray: " << dashArray 
        << "; rx: " << cNormal.radius << "px; transition: all 0.15s ease; }\n";
    out << "  .graph-container:hover rect.box { fill: " << toCssRgba(cHover.background)
        << "; stroke: " << toCssRgba(cHover.border)
        << "; stroke-width: " << cHover.borderWidth << "px; }\n";
    out << "  .graph-container rect.header { fill: " << toCssRgba(cNormal.headerBackground) 
        << "; rx: " << cNormal.radius << "px; }\n";
    out << "  .graph-container text.header-title { fill: " << toCssRgba(cNormal.title.color) 
        << "; font-size: " << cNormal.title.size << "px; font-weight: " 
        << (cNormal.title.bold ? "bold" : "normal") << "; pointer-events: none; }\n";

    // --- Edges ---
    QString edgeDash = (theme.edge.normal.lineStyle == Qt::DashLine || theme.edge.normal.dashed) ? "6 4" : "none";

    out << "  .graph-edge { cursor: pointer; }\n";
    out << "  .graph-edge path.line { fill: none; stroke: " << toCssRgba(theme.edge.normal.lineColor) 
        << "; stroke-width: " << theme.edge.normal.lineWidth 
        << "px; stroke-dasharray: " << edgeDash 
        << "; transition: stroke 0.15s ease, stroke-width 0.15s ease; }\n";
    out << "  .graph-edge polygon.arrow { fill: " << toCssRgba(theme.edge.normal.arrow.fillColor) 
        << "; stroke: " << toCssRgba(theme.edge.normal.arrow.lineColor) 
        << "; stroke-width: " << theme.edge.normal.arrow.lineWidth << "px; transition: fill 0.15s ease; }\n";
    out << "  .graph-edge:hover path.line { stroke: " << toCssRgba(theme.edge.hover.lineColor) 
        << "; stroke-width: " << theme.edge.hover.lineWidth 
        << "px; filter: drop-shadow(0 0 4px " << toCssRgba(theme.edge.hover.lineColor) << "); }\n";
    out << "  .graph-edge:hover polygon.arrow { fill: " << toCssRgba(theme.edge.hover.arrow.fillColor) 
        << "; stroke: " << toCssRgba(theme.edge.hover.arrow.lineColor) << "; }\n";
    out << "  .graph-edge text { fill: " << toCssRgba(theme.edge.normal.label.textColor) 
        << "; font-size: " << theme.edge.normal.label.fontSize << "px; pointer-events: none; }\n";
    out << "  .graph-edge rect.label-bg { fill: " << toCssRgba(theme.edge.normal.label.backgroundColor)
        << "; stroke: " << toCssRgba(theme.edge.normal.label.borderColor)
        << "; stroke-width: " << theme.edge.normal.label.borderWidth << "px; rx: " << theme.edge.normal.label.radius << "px; }\n";

    // --- Sidebar Drawer ---
    QColor sidebarBg = theme.node.normal.background;
    QColor sidebarBorder = theme.node.normal.border;
    QColor textPrimary = theme.node.normal.title.color;
    QColor textSecondary = theme.node.normal.body.color;
    QColor codeBg = theme.view.background;

    out << "  #sidebar { width: 340px; background: " << toCssRgba(sidebarBg) 
        << "; color: " << toCssRgba(textPrimary) 
        << "; border-left: 1px solid " << toCssRgba(sidebarBorder) 
        << "; display: flex; flex-direction: column; transform: translateX(100%); transition: transform 0.25s cubic-bezier(0.4, 0, 0.2, 1); box-shadow: -4px 0 24px rgba(0,0,0,0.2); z-index: 10; }\n";
    out << "  #sidebar.open { transform: translateX(0); }\n";
    out << "  .sb-header { padding: 16px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid " << toCssRgba(sidebarBorder) << "; }\n";
    out << "  .sb-title { margin: 0; font-size: 16px; font-weight: bold; color: " << toCssRgba(textPrimary) << "; }\n";
    out << "  .sb-close { background: none; border: none; color: " << toCssRgba(textSecondary) << "; font-size: 20px; cursor: pointer; padding: 0 4px; }\n";
    out << "  .sb-content { padding: 16px; overflow-y: auto; flex: 1; font-size: 13px; }\n";
    out << "  .sb-section { margin-bottom: 20px; }\n";
    out << "  .sb-section-title { font-size: 11px; text-transform: uppercase; color: " << toCssRgba(textSecondary) << "; font-weight: bold; margin-bottom: 8px; letter-spacing: 0.5px; }\n";
    out << "  pre.code-block { background: " << toCssRgba(codeBg) << "; border: 1px solid " << toCssRgba(sidebarBorder) 
        << "; padding: 10px; border-radius: 6px; overflow-x: auto; color: " << toCssRgba(textPrimary) << "; font-family: monospace; font-size: 12px; margin: 0; }\n";
    out << "  .field-row { display: flex; margin-bottom: 6px; justify-content: space-between; }\n";
    out << "  .field-name { color: " << toCssRgba(textSecondary) << "; }\n";
    out << "  .field-val { color: " << toCssRgba(textPrimary) << "; font-weight: 500; }\n";
    out << "</style>\n</head>\n<body>\n";

    out << "<div id=\"canvas-container\">\n";
    out << "<svg id=\"main-svg\" viewBox=\"" << sceneBounds.left() << " " << sceneBounds.top() << " " 
        << sceneBounds.width() << " " << sceneBounds.height() << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    out << "  <g id=\"viewport-group\">\n";

    // Containers
    for (auto* node : containers)
    {
        QPointF scenePos = node->mapToScene(QPointF(0, 0));
        QRectF bounds = node->boundingRect();
        auto opt = node->model()->getNodeById(node->nodeId());

        QString title = node->displayTitle().toHtmlEscaped();
        QString type = node->displayType().toHtmlEscaped();
        QString meta = opt ? QString::fromStdString(opt->metadata).toHtmlEscaped() : "{}";
        QString attr = opt ? QString::fromStdString(opt->attributes).toHtmlEscaped() : "{}";

        out << "    <g class=\"graph-container\" transform=\"translate(" << scenePos.x() << "," << scenePos.y() << ")\""
            << " data-id=\"" << node->nodeId() << "\" data-title=\"" << title << "\" data-type=\"" << type 
            << "\" data-meta=\"" << meta << "\" data-attr=\"" << attr << "\">\n";
        out << "      <rect class=\"box\" width=\"" << bounds.width() << "\" height=\"" << bounds.height() << "\"/>\n";
        out << "      <rect class=\"header\" width=\"" << bounds.width() << "\" height=\"32\"/>\n";
        out << "      <text class=\"header-title\" x=\"14\" y=\"20\">" << title << "</text>\n";
        out << "    </g>\n";
    }

    // Edges
    for (auto* edge : edges)
    {
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

        QString edgeTitle = edge->title().toHtmlEscaped();

        out << "    <g class=\"graph-edge\" data-id=\"" << edge->edgeId() << "\" data-type=\"" << edgeTitle << "\">\n";
        out << "      <path class=\"line\" d=\"" << d << "\"/>\n";
        out << "      <polygon class=\"arrow\" points=\"" 
            << arrowTip.x() << "," << arrowTip.y() << " " 
            << arrowP1.x() << "," << arrowP1.y() << " " 
            << arrowP2.x() << "," << arrowP2.y() << "\"/>\n";

        if (!edgeTitle.isEmpty())
        {
            QPointF midPoint = edge->mapToScene(fullPath.pointAtPercent(0.5));
            qreal labelW = edgeTitle.length() * 7.5 + 12;
            out << "      <rect class=\"label-bg\" x=\"" << (midPoint.x() - labelW / 2.0) << "\" y=\"" << (midPoint.y() - 19) 
                << "\" width=\"" << labelW << "\" height=\"16\"/>\n";
            out << "      <text x=\"" << midPoint.x() << "\" y=\"" << (midPoint.y() - 7) 
                << "\" text-anchor=\"middle\">" << edgeTitle << "</text>\n";
        }
        out << "    </g>\n";
    }

    // Leaf Nodes
    for (auto* node : leafNodes)
    {
        QPointF scenePos = node->mapToScene(QPointF(0, 0));
        QRectF bounds = node->boundingRect();
        qreal w = bounds.width();
        qreal h = bounds.height();

        auto opt = node->model()->getNodeById(node->nodeId());
        QString title = node->displayTitle().toHtmlEscaped();
        QString type  = node->displayType().toHtmlEscaped();
        QString meta  = opt ? QString::fromStdString(opt->metadata).toHtmlEscaped() : "{}";
        QString attr  = opt ? QString::fromStdString(opt->attributes).toHtmlEscaped() : "{}";

        out << "    <g class=\"graph-node\" transform=\"translate(" << scenePos.x() << "," << scenePos.y() << ")\""
            << " data-id=\"" << node->nodeId() << "\" data-title=\"" << title << "\" data-type=\"" << type 
            << "\" data-meta=\"" << meta << "\" data-attr=\"" << attr << "\">\n";
        out << "      <rect class=\"body\" width=\"" << w << "\" height=\"" << h << "\"/>\n";
        out << "      <text class=\"title\" x=\"" << (w / 2.0) << "\" y=\"" << (h / 2.0 - 4) 
            << "\" text-anchor=\"middle\" dominant-baseline=\"middle\">" << title << "</text>\n";
        out << "      <text class=\"body\" x=\"" << (w / 2.0) << "\" y=\"" << (h / 2.0 + 14) 
            << "\" text-anchor=\"middle\" dominant-baseline=\"middle\">" << type << "</text>\n";
        out << "    </g>\n";
    }

    out << "  </g>\n";
    out << "</svg>\n";
    out << "</div>\n";

    // Sidebar & Interactivity JS
    out << "<div id=\"sidebar\">\n";
    out << "  <div class=\"sb-header\">\n";
    out << "    <h3 class=\"sb-title\" id=\"sb-title\">Inspector</h3>\n";
    out << "    <button class=\"sb-close\" onclick=\"closeSidebar()\">&times;</button>\n";
    out << "  </div>\n";
    out << "  <div class=\"sb-content\" id=\"sb-body\"></div>\n";
    out << "</div>\n";

    out << "<script>\n";
    out << "  const svg = document.getElementById('main-svg');\n";
    out << "  const g = document.getElementById('viewport-group');\n";
    out << "  const sidebar = document.getElementById('sidebar');\n";
    out << "  let isPanning = false, startX = 0, startY = 0;\n";
    out << "  let currentScale = 1.0, currentTx = 0, currentTy = 0;\n";

    out << "  function updateTransform() {\n";
    out << "    g.setAttribute('transform', `translate(${currentTx}, ${currentTy}) scale(${currentScale})`);\n";
    out << "  }\n";

    out << "  window.addEventListener('wheel', (e) => {\n";
    out << "    e.preventDefault();\n";
    out << "    const zoomFactor = e.deltaY < 0 ? 1.1 : 0.9;\n";
    out << "    currentScale *= zoomFactor;\n";
    out << "    updateTransform();\n";
    out << "  }, { passive: false });\n";

    out << "  window.addEventListener('mousedown', (e) => {\n";
    out << "    if (e.target.closest('#sidebar') || e.target.closest('.graph-node') || e.target.closest('.graph-container')) return;\n";
    out << "    isPanning = true;\n";
    out << "    startX = e.clientX - currentTx;\n";
    out << "    startY = e.clientY - currentTy;\n";
    out << "  });\n";

    out << "  window.addEventListener('mousemove', (e) => {\n";
    out << "    if (!isPanning) return;\n";
    out << "    currentTx = e.clientX - startX;\n";
    out << "    currentTy = e.clientY - startY;\n";
    out << "    updateTransform();\n";
    out << "  });\n";

    out << "  window.addEventListener('mouseup', () => { isPanning = false; });\n";

    out << "  function formatJson(jsonStr) {\n";
    out << "    try { return JSON.stringify(JSON.parse(jsonStr), null, 2); }\n";
    out << "    catch (e) { return jsonStr || '{}'; }\n";
    out << "  }\n";

    out << "  function closeSidebar() { sidebar.classList.remove('open'); }\n";

    out << "  document.querySelectorAll('.graph-node, .graph-container').forEach(el => {\n";
    out << "    el.addEventListener('click', (e) => {\n";
    out << "      e.stopPropagation();\n";
    out << "      document.getElementById('sb-title').innerText = el.getAttribute('data-title');\n";
    out << "      document.getElementById('sb-body').innerHTML = `\n";
    out << "        <div class=\"sb-section\">\n";
    out << "          <div class=\"sb-section-title\">Details</div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">ID</span><span class=\"field-val\">#${el.getAttribute('data-id')}</span></div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">Type</span><span class=\"field-val\">${el.getAttribute('data-type')}</span></div>\n";
    out << "        </div>\n";
    out << "        <div class=\"sb-section\">\n";
    out << "          <div class=\"sb-section-title\">Attributes</div>\n";
    out << "          <pre class=\"code-block\">${formatJson(el.getAttribute('data-attr'))}</pre>\n";
    out << "        </div>\n";
    out << "        <div class=\"sb-section\">\n";
    out << "          <div class=\"sb-section-title\">Metadata</div>\n";
    out << "          <pre class=\"code-block\">${formatJson(el.getAttribute('data-meta'))}</pre>\n";
    out << "        </div>\n";
    out << "      `;\n";
    out << "      sidebar.classList.add('open');\n";
    out << "    });\n";
    out << "  });\n";
    out << "</script>\n</body>\n</html>\n";

    file.close();
}