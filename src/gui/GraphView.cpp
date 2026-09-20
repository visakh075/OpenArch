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
#include <QAction>
#include "core/Types.h"
#include <cmath>

static QString toCssRgba(const QColor& c)
{
    return QString("rgba(%1, %2, %3, %4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(c.alphaF(), 0, 'f', 2);
}

static QString toSvgDashArray(Qt::PenStyle style, const QVector<qreal>& pattern)
{
    if (style == Qt::DashLine) return "6 4";
    if (style == Qt::DotLine) return "2 2";
    if (style == Qt::DashDotLine) return "6 3 2 3";
    if (style == Qt::CustomDashLine && !pattern.isEmpty()) {
        QStringList sl;
        for (qreal val : pattern) sl << QString::number(val);
        return sl.join(" ");
    }
    return "none";
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

void GraphView::exportToInteractiveHtml(const QString& filePath)
{
    QString fileName = filePath;
    if (fileName.isEmpty())
    {
        fileName = QFileDialog::getSaveFileName(
            this, "Export Interactive HTML", "architecture.html", "HTML Files (*.html)");
    }
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;

    const auto& theme = GraphThemeManager::instance()->theme();
    const int padding = 80;
    QRectF sceneBounds = scene()->itemsBoundingRect().adjusted(-padding, -padding, padding, padding);
    if (sceneBounds.isNull() || sceneBounds.isEmpty())
        sceneBounds = QRectF(0, 0, 1200, 800);

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

    // Sort containers and leaf nodes by zValue ascending so parent/underlying items render before child/overlying items
    std::sort(containers.begin(), containers.end(), [](GraphNodeItem* a, GraphNodeItem* b) {
        return a->zValue() < b->zValue();
    });
    std::sort(leafNodes.begin(), leafNodes.end(), [](GraphNodeItem* a, GraphNodeItem* b) {
        return a->zValue() < b->zValue();
    });

    QTextStream out(&file);

    out << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    out << "<meta charset=\"utf-8\"/>\n";
    out << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>\n";
    out << "<title>OpenArch Architecture Diagram</title>\n";
    out << "<style>\n";
    out << "  * { box-sizing: border-box; }\n";
    out << "  html, body { margin: 0; padding: 0; width: 100vw; height: 100vh; overflow: hidden; background: "
        << toCssRgba(theme.view.background) << "; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; display: flex; }\n";
    out << "  #canvas-container { flex: 1; height: 100%; position: relative; cursor: grab; overflow: hidden; }\n";
    out << "  #canvas-container.panning { cursor: grabbing; }\n";
    out << "  svg { width: 100%; height: 100%; display: block; user-select: none; }\n";

    // --- Containers ---
    const auto& cNormal = theme.container.normal;
    const auto& cHover = theme.container.hover;
    const auto& cSelected = theme.container.selected;
    QString cNormalDash = toSvgDashArray(cNormal.borderStyle, cNormal.dashPattern);
    QString cHoverDash = toSvgDashArray(cHover.borderStyle, cHover.dashPattern);
    QString cSelectedDash = toSvgDashArray(cSelected.borderStyle, cSelected.dashPattern);

    out << "  .graph-container { cursor: pointer; }\n";
    out << "  .graph-container rect.box { fill: " << toCssRgba(cNormal.background)
        << "; stroke: " << toCssRgba(cNormal.border)
        << "; stroke-width: " << cNormal.borderWidth
        << "px; stroke-dasharray: " << cNormalDash
        << "; rx: " << cNormal.radius
        << "px; pointer-events: all; transition: fill 0.15s ease, stroke 0.15s ease, stroke-width 0.15s ease; }\n";
    out << "  .graph-container:hover rect.box { fill: " << toCssRgba(cHover.background)
        << "; stroke: " << toCssRgba(cHover.border)
        << "; stroke-width: " << cHover.borderWidth
        << "px; stroke-dasharray: " << cHoverDash << "; }\n";
    out << "  .graph-container.selected rect.box { fill: " << toCssRgba(cSelected.background)
        << "; stroke: " << toCssRgba(cSelected.border)
        << "; stroke-width: " << cSelected.borderWidth
        << "px; stroke-dasharray: " << cSelectedDash << "; }\n";

    out << "  .graph-container rect.header { fill: " << toCssRgba(cNormal.headerBackground)
        << "; rx: " << cNormal.radius << "px; pointer-events: all; transition: fill 0.15s ease; }\n";
    out << "  .graph-container:hover rect.header { fill: " << toCssRgba(cHover.headerBackground) << "; }\n";
    out << "  .graph-container.selected rect.header { fill: " << toCssRgba(cSelected.headerBackground) << "; }\n";

    out << "  .graph-container text.header-title { fill: " << toCssRgba(cNormal.title.color)
        << "; font-size: " << cNormal.title.size
        << "px; font-weight: " << (cNormal.title.bold ? "bold" : "normal")
        << "; pointer-events: none; transition: fill 0.15s ease; }\n";
    out << "  .graph-container:hover text.header-title { fill: " << toCssRgba(cHover.title.color) << "; }\n";
    out << "  .graph-container.selected text.header-title { fill: " << toCssRgba(cSelected.title.color) << "; font-weight: bold; }\n";

    // --- Leaf Nodes ---
    const auto& nNormal = theme.node.normal;
    const auto& nHover = theme.node.hover;
    const auto& nSelected = theme.node.selected;
    QString nNormalDash = toSvgDashArray(nNormal.borderStyle, nNormal.dashPattern);
    QString nHoverDash = toSvgDashArray(nHover.borderStyle, nHover.dashPattern);
    QString nSelectedDash = toSvgDashArray(nSelected.borderStyle, nSelected.dashPattern);

    out << "  .graph-node { cursor: pointer; }\n";
    out << "  .graph-node rect.body { fill: " << toCssRgba(nNormal.background)
        << "; stroke: " << toCssRgba(nNormal.border)
        << "; stroke-width: " << nNormal.borderWidth
        << "px; stroke-dasharray: " << nNormalDash
        << "; rx: " << nNormal.radius
        << "px; pointer-events: all; transition: fill 0.15s ease, stroke 0.15s ease, stroke-width 0.15s ease; }\n";
    out << "  .graph-node:hover rect.body { fill: " << toCssRgba(nHover.background)
        << "; stroke: " << toCssRgba(nHover.border)
        << "; stroke-width: " << nHover.borderWidth
        << "px; stroke-dasharray: " << nHoverDash << "; }\n";
    out << "  .graph-node.selected rect.body { fill: " << toCssRgba(nSelected.background)
        << "; stroke: " << toCssRgba(nSelected.border)
        << "; stroke-width: " << nSelected.borderWidth
        << "px; stroke-dasharray: " << nSelectedDash << "; }\n";

    out << "  .graph-node text.title { fill: " << toCssRgba(nNormal.title.color)
        << "; font-size: " << nNormal.title.size
        << "px; font-weight: " << (nNormal.title.bold ? "bold" : "normal")
        << "; pointer-events: none; transition: fill 0.15s ease; }\n";
    out << "  .graph-node:hover text.title { fill: " << toCssRgba(nHover.title.color) << "; }\n";
    out << "  .graph-node.selected text.title { fill: " << toCssRgba(nSelected.title.color) << "; font-weight: bold; }\n";

    out << "  .graph-node text.body { fill: " << toCssRgba(nNormal.body.color)
        << "; font-size: " << nNormal.body.size
        << "px; pointer-events: none; transition: fill 0.15s ease; }\n";
    out << "  .graph-node:hover text.body { fill: " << toCssRgba(nHover.body.color) << "; }\n";
    out << "  .graph-node.selected text.body { fill: " << toCssRgba(nSelected.body.color) << "; }\n";

    // --- Edges (Identical to Qt View rendering) ---
    const auto& eNormal = theme.edge.normal;
    const auto& eHover = theme.edge.hover;
    const auto& eSelected = theme.edge.selected;
    QString edgeNormalDash = (eNormal.lineStyle == Qt::DashLine || eNormal.dashed) ? "6 4"
                           : toSvgDashArray(eNormal.lineStyle, eNormal.dashPattern);
    QString edgeHoverDash = (eHover.lineStyle == Qt::DashLine || eHover.dashed) ? "6 4"
                          : toSvgDashArray(eHover.lineStyle, eHover.dashPattern);
    QString edgeSelectedDash = (eSelected.lineStyle == Qt::DashLine || eSelected.dashed) ? "6 4"
                             : toSvgDashArray(eSelected.lineStyle, eSelected.dashPattern);

    out << "  .graph-edge { cursor: pointer; }\n";
    out << "  .graph-edge .edge-hit { fill: none; stroke: transparent; stroke-width: 20px; stroke-linecap: round; stroke-linejoin: round; pointer-events: stroke; }\n";
    out << "  .graph-edge path.line { fill: none; stroke: " << toCssRgba(eNormal.lineColor)
        << "; stroke-width: " << eNormal.lineWidth
        << "px; stroke-dasharray: " << edgeNormalDash
        << "; stroke-linecap: round; stroke-linejoin: round; pointer-events: stroke; transition: stroke 0.15s ease, stroke-width 0.15s ease; }\n";
    out << "  .graph-edge polygon.arrow { fill: " << toCssRgba(eNormal.arrow.fillColor)
        << "; stroke: " << toCssRgba(eNormal.arrow.lineColor)
        << "; stroke-width: " << eNormal.arrow.lineWidth
        << "px; stroke-linejoin: round; pointer-events: all; transition: fill 0.15s ease, stroke 0.15s ease, stroke-width 0.15s ease; }\n";

    out << "  .graph-edge .edge-label { cursor: pointer; pointer-events: all; }\n";
    out << "  .graph-edge .edge-label rect.label-bg { fill: " << toCssRgba(eNormal.label.backgroundColor)
        << "; stroke: " << toCssRgba(eNormal.label.borderColor)
        << "; stroke-width: " << eNormal.label.borderWidth
        << "px; rx: " << eNormal.label.radius
        << "px; pointer-events: all; transition: fill 0.15s ease, stroke 0.15s ease, stroke-width 0.15s ease; }\n";
    out << "  .graph-edge .edge-label text { fill: " << toCssRgba(eNormal.label.textColor)
        << "; font-size: " << eNormal.label.fontSize
        << "px; font-weight: " << (eNormal.label.bold ? "bold" : "normal")
        << "; pointer-events: none; transition: fill 0.15s ease; }\n";

    // Edge Hover State (Clean, crisp, matching View without blurry halo)
    out << "  .graph-edge:hover path.line { stroke: " << toCssRgba(eHover.lineColor)
        << "; stroke-width: " << eHover.lineWidth
        << "px; stroke-dasharray: " << edgeHoverDash << "; }\n";
    out << "  .graph-edge:hover polygon.arrow { fill: " << toCssRgba(eHover.arrow.fillColor)
        << "; stroke: " << toCssRgba(eHover.arrow.lineColor)
        << "; stroke-width: " << eHover.arrow.lineWidth << "px; }\n";
    out << "  .graph-edge:hover .edge-label rect.label-bg { fill: " << toCssRgba(eHover.label.backgroundColor)
        << "; stroke: " << toCssRgba(eHover.label.borderColor)
        << "; stroke-width: " << eHover.label.borderWidth << "px; }\n";
    out << "  .graph-edge:hover .edge-label text { fill: " << toCssRgba(eHover.label.textColor)
        << "; font-weight: " << (eHover.label.bold ? "bold" : "normal") << "; }\n";

    // Edge Selected State
    out << "  .graph-edge.selected path.line { stroke: " << toCssRgba(eSelected.lineColor)
        << "; stroke-width: " << eSelected.lineWidth
        << "px; stroke-dasharray: " << edgeSelectedDash << "; }\n";
    out << "  .graph-edge.selected polygon.arrow { fill: " << toCssRgba(eSelected.arrow.fillColor)
        << "; stroke: " << toCssRgba(eSelected.arrow.lineColor)
        << "; stroke-width: " << eSelected.arrow.lineWidth << "px; }\n";
    out << "  .graph-edge.selected .edge-label rect.label-bg { fill: " << toCssRgba(eSelected.label.backgroundColor)
        << "; stroke: " << toCssRgba(eSelected.label.borderColor)
        << "; stroke-width: " << eSelected.label.borderWidth << "px; }\n";
    out << "  .graph-edge.selected .edge-label text { fill: " << toCssRgba(eSelected.label.textColor)
        << "; font-weight: bold; }\n";

    // --- Floating Navigation Controls ---
    QColor ctrlBg = theme.node.normal.background;
    QColor ctrlBorder = theme.node.normal.border;
    QColor ctrlText = theme.node.normal.title.color;

    out << "  #controls-bar { position: absolute; bottom: 20px; right: 20px; display: flex; background: "
        << toCssRgba(ctrlBg) << "; border: 1px solid " << toCssRgba(ctrlBorder)
        << "; border-radius: 8px; box-shadow: 0 4px 16px rgba(0,0,0,0.3); z-index: 10; overflow: hidden; }\n";
    out << "  #controls-bar button { background: transparent; border: none; color: " << toCssRgba(ctrlText)
        << "; width: 36px; height: 36px; font-size: 18px; cursor: pointer; display: flex; align-items: center; justify-content: center; transition: background 0.15s ease; }\n";
    out << "  #controls-bar button:hover { background: " << toCssRgba(nHover.background) << "; }\n";
    out << "  #controls-bar button + button { border-left: 1px solid " << toCssRgba(ctrlBorder) << "; }\n";

    // --- Sidebar Drawer ---
    QColor sidebarBg = theme.node.normal.background;
    QColor sidebarBorder = theme.node.normal.border;
    QColor textPrimary = theme.node.normal.title.color;
    QColor textSecondary = theme.node.normal.body.color;
    QColor codeBg = theme.view.background;

    out << "  #sidebar { width: 360px; background: " << toCssRgba(sidebarBg)
        << "; color: " << toCssRgba(textPrimary)
        << "; border-left: 1px solid " << toCssRgba(sidebarBorder)
        << "; display: flex; flex-direction: column; transform: translateX(100%); transition: transform 0.25s cubic-bezier(0.4, 0, 0.2, 1); box-shadow: -4px 0 24px rgba(0,0,0,0.25); z-index: 20; }\n";
    out << "  #sidebar.open { transform: translateX(0); }\n";
    out << "  .sb-header { padding: 16px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid " << toCssRgba(sidebarBorder) << "; }\n";
    out << "  .sb-title { margin: 0; font-size: 16px; font-weight: bold; color: " << toCssRgba(textPrimary) << "; }\n";
    out << "  .sb-close { background: none; border: none; color: " << toCssRgba(textSecondary) << "; font-size: 22px; cursor: pointer; padding: 0 4px; line-height: 1; }\n";
    out << "  .sb-close:hover { color: " << toCssRgba(textPrimary) << "; }\n";
    out << "  .sb-content { padding: 16px; overflow-y: auto; flex: 1; font-size: 13px; }\n";
    out << "  .sb-section { margin-bottom: 20px; }\n";
    out << "  .sb-section-title { font-size: 11px; text-transform: uppercase; color: " << toCssRgba(textSecondary) << "; font-weight: bold; margin-bottom: 8px; letter-spacing: 0.5px; }\n";
    out << "  pre.code-block { background: " << toCssRgba(codeBg) << "; border: 1px solid " << toCssRgba(sidebarBorder)
        << "; padding: 10px; border-radius: 6px; overflow-x: auto; color: " << toCssRgba(textPrimary) << "; font-family: 'Consolas', 'Courier New', monospace; font-size: 12px; margin: 0; }\n";
    out << "  .field-row { display: flex; margin-bottom: 8px; justify-content: space-between; align-items: center; }\n";
    out << "  .field-name { color: " << toCssRgba(textSecondary) << "; }\n";
    out << "  .field-val { color: " << toCssRgba(textPrimary) << "; font-weight: 500; text-align: right; word-break: break-word; }\n";

    // Status badges
    out << "  .status-badge { display: inline-block; padding: 2px 8px; border-radius: 12px; font-size: 10px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; }\n";
    out << "  .status-new { background: rgba(136, 192, 208, 0.2); color: #88c0d0; border: 1px solid #88c0d0; }\n";
    out << "  .status-changed { background: rgba(235, 203, 139, 0.2); color: #ebcb8b; border: 1px solid #ebcb8b; }\n";
    out << "  .status-approved { background: rgba(163, 190, 140, 0.2); color: #a3be8c; border: 1px solid #a3be8c; }\n";
    out << "  .status-reviewed { background: rgba(180, 142, 173, 0.2); color: #b48ead; border: 1px solid #b48ead; }\n";
    out << "  .status-invalid { background: rgba(191, 97, 106, 0.2); color: #bf616a; border: 1px solid #bf616a; }\n";
    out << "  .status-deleted { background: rgba(128, 128, 128, 0.2); color: #888888; border: 1px solid #888888; }\n";

    out << "</style>\n</head>\n<body>\n";

    out << "<div id=\"canvas-container\">\n";
    out << "<svg id=\"main-svg\" viewBox=\"" << sceneBounds.left() << " " << sceneBounds.top() << " "
        << sceneBounds.width() << " " << sceneBounds.height() << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";

    // Grid pattern definition
    if (theme.view.grid.enabled)
    {
        int gridSpacing = theme.view.grid.spacing > 0 ? theme.view.grid.spacing : 20;
        out << "  <defs>\n";
        out << "    <pattern id=\"canvas-grid\" width=\"" << gridSpacing << "\" height=\"" << gridSpacing << "\" patternUnits=\"userSpaceOnUse\">\n";
        out << "      <path d=\"M " << gridSpacing << " 0 L 0 0 0 " << gridSpacing << "\" fill=\"none\" stroke=\""
            << toCssRgba(theme.view.grid.majorColor) << "\" stroke-width=\"" << theme.view.grid.lineWidth << "\"/>\n";
        out << "    </pattern>\n";
        out << "  </defs>\n";
    }

    out << "  <g id=\"viewport-group\">\n";

    // 1. Grid Background
    if (theme.view.grid.enabled)
    {
        out << "    <rect id=\"grid-bg\" x=\"" << (sceneBounds.left() - 50000) << "\" y=\"" << (sceneBounds.top() - 50000)
            << "\" width=\"100000\" height=\"100000\" fill=\"url(#canvas-grid)\" pointer-events=\"none\"/>\n";
    }

    // 2. Containers Layer
    out << "    <g id=\"containers-layer\">\n";
    for (auto* node : containers)
    {
        QPointF scenePos = node->mapToScene(QPointF(0, 0));
        QRectF bounds = node->rect();
        qreal headerH = node->headerRect().height() > 0 ? node->headerRect().height() : 32.0;
        qreal headerTitleY = headerH / 2.0;
        auto* m = node->model();
        auto opt = m ? m->getNodeById(node->nodeId()) : std::nullopt;

        QString title = node->displayTitle().toHtmlEscaped();
        QString type = node->displayType().toHtmlEscaped();
        QString meta = opt ? QString::fromStdString(opt->metadata).toHtmlEscaped() : "{}";
        QString attr = opt ? QString::fromStdString(opt->attributes).toHtmlEscaped() : "{}";
        QString status = opt ? QString::fromStdString(to_string(opt->status)) : "new";
        QString reviewer = opt ? QString::fromStdString(opt->reviewer).toHtmlEscaped() : "";
        QString checksum = opt ? QString::number(opt->checksum) : "0";

        QString parentStr = "None (Root)";
        if (opt && opt->parentId.has_value() && m) {
            auto parentOpt = m->getNodeById(*opt->parentId);
            if (parentOpt)
                parentStr = QString("%1 (#%2)").arg(QString::fromStdString(parentOpt->name).toHtmlEscaped()).arg(*opt->parentId);
            else
                parentStr = QString("#%1").arg(*opt->parentId);
        }

        out << "      <g class=\"graph-container\" transform=\"translate(" << scenePos.x() << "," << scenePos.y() << ")\""
            << " data-id=\"" << node->nodeId() << "\" data-title=\"" << title << "\" data-type=\"" << type
            << "\" data-kind=\"container\""
            << " data-parent=\"" << parentStr
            << "\" data-status=\"" << status
            << "\" data-reviewer=\"" << reviewer
            << "\" data-checksum=\"" << checksum
            << "\" data-meta=\"" << meta << "\" data-attr=\"" << attr << "\">\n";
        out << "        <rect class=\"box\" width=\"" << bounds.width() << "\" height=\"" << bounds.height() << "\"/>\n";
        out << "        <rect class=\"header\" width=\"" << bounds.width() << "\" height=\"" << headerH << "\"/>\n";
        out << "        <text class=\"header-title\" x=\"14\" y=\"" << headerTitleY << "\" dominant-baseline=\"middle\">" << title << "</text>\n";
        out << "      </g>\n";
    }
    out << "    </g>\n";

    // 3. Edges Layer
    out << "    <g id=\"edges-layer\">\n";
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

        auto* m = edge->model();
        auto opt = m ? m->getEdgeById(edge->edgeId()) : std::nullopt;

        QString edgeTitle = edge->title().toHtmlEscaped();
        QString srcTitle = edge->srcNode() ? edge->srcNode()->displayTitle().toHtmlEscaped() : "Unknown";
        QString dstTitle = edge->dstNode() ? edge->dstNode()->displayTitle().toHtmlEscaped() : "Unknown";
        NodeId srcId = opt ? opt->srcNode : (edge->srcNode() ? edge->srcNode()->nodeId() : 0);
        NodeId dstId = opt ? opt->dstNode : (edge->dstNode() ? edge->dstNode()->nodeId() : 0);
        QString status = opt ? QString::fromStdString(to_string(opt->status)) : "new";
        QString reviewer = opt ? QString::fromStdString(opt->reviewer).toHtmlEscaped() : "";
        QString checksum = opt ? QString::number(opt->checksum) : "0";
        QString meta = opt ? QString::fromStdString(opt->metadata).toHtmlEscaped() : "{}";
        QString attr = opt ? QString::fromStdString(opt->attributes).toHtmlEscaped() : "{}";

        out << "      <g class=\"graph-edge\" data-id=\"" << edge->edgeId()
            << "\" data-type=\"" << edgeTitle
            << "\" data-kind=\"edge\""
            << " data-src-name=\"" << srcTitle << "\" data-src-id=\"" << srcId
            << "\" data-dst-name=\"" << dstTitle << "\" data-dst-id=\"" << dstId
            << "\" data-status=\"" << status
            << "\" data-reviewer=\"" << reviewer
            << "\" data-checksum=\"" << checksum
            << "\" data-meta=\"" << meta << "\" data-attr=\"" << attr << "\">\n";

        // Transparent hit area for easy hover and selection
        out << "        <path class=\"edge-hit\" d=\"" << d << "\" fill=\"none\" stroke=\"transparent\" stroke-width=\"20\" pointer-events=\"stroke\"/>\n";
        // Visible stroke path with rounded joins & caps
        out << "        <path class=\"line\" d=\"" << d << "\" pointer-events=\"stroke\"/>\n";
        // Arrow head
        out << "        <polygon class=\"arrow\" points=\""
            << arrowTip.x() << "," << arrowTip.y() << " "
            << arrowP1.x() << "," << arrowP1.y() << " "
            << arrowP2.x() << "," << arrowP2.y() << "\" pointer-events=\"all\"/>\n";

        // Label Badge rotated along segment angle (matching Qt View)
        if (!edgeTitle.isEmpty())
        {
            QPointF p1 = edge->mapToScene(fullPath.pointAtPercent(0.5));
            QPointF p2 = edge->mapToScene(fullPath.pointAtPercent(0.51));

            double textAngle = std::atan2(p2.y() - p1.y(), p2.x() - p1.x());
            double degrees = textAngle * 180.0 / M_PI;
            if (degrees > 90 || degrees < -90)
                degrees += 180.0;

            QRect textRect = edge->titleRect();
            if (textRect.isNull() || textRect.isEmpty()) {
                QFont font;
                font.setPointSize(theme.edge.normal.label.fontSize);
                font.setBold(theme.edge.normal.label.bold);
                QFontMetrics fm(font);
                textRect = fm.boundingRect(edgeTitle);
            }

            qreal padX = theme.edge.normal.label.paddingX;
            qreal padY = theme.edge.normal.label.paddingY;
            qreal bgWidth = textRect.width() + (padX * 2);
            qreal bgHeight = textRect.height() + (padY * 2);

            qreal badgeHalfHeight = (textRect.height() / 2.0) + padY;
            qreal verticalDistance = (theme.edge.normal.lineWidth / 2.0) + theme.edge.normal.label.offset + badgeHalfHeight;

            out << "        <g class=\"edge-label\" transform=\"translate(" << p1.x() << "," << p1.y() << ") rotate(" << degrees << ")\">\n";
            out << "          <rect class=\"label-bg\" x=\"" << (-bgWidth / 2.0) << "\" y=\"" << (-verticalDistance - bgHeight / 2.0)
                << "\" width=\"" << bgWidth << "\" height=\"" << bgHeight << "\"/>\n";
            out << "          <text x=\"0\" y=\"" << (-verticalDistance) << "\" text-anchor=\"middle\" dominant-baseline=\"central\">"
                << edgeTitle << "</text>\n";
            out << "        </g>\n";
        }
        out << "      </g>\n";
    }
    out << "    </g>\n";

    // 4. Leaf Nodes Layer
    out << "    <g id=\"nodes-layer\">\n";
    for (auto* node : leafNodes)
    {
        QPointF scenePos = node->mapToScene(QPointF(0, 0));
        QRectF bounds = node->rect();
        qreal w = bounds.width();
        qreal h = bounds.height();
        QRectF tRect = node->titleRect();
        QRectF bRect = node->bodyRect();
        qreal titleY = (!tRect.isNull() && !tRect.isEmpty()) ? tRect.center().y() : (h / 2.0 - 5.0);
        qreal bodyY  = (!bRect.isNull() && !bRect.isEmpty())  ? bRect.center().y()  : (h / 2.0 + 13.0);

        auto* m = node->model();
        auto opt = m ? m->getNodeById(node->nodeId()) : std::nullopt;

        QString title = node->displayTitle().toHtmlEscaped();
        QString type  = node->displayType().toHtmlEscaped();
        QString meta  = opt ? QString::fromStdString(opt->metadata).toHtmlEscaped() : "{}";
        QString attr  = opt ? QString::fromStdString(opt->attributes).toHtmlEscaped() : "{}";
        QString status = opt ? QString::fromStdString(to_string(opt->status)) : "new";
        QString reviewer = opt ? QString::fromStdString(opt->reviewer).toHtmlEscaped() : "";
        QString checksum = opt ? QString::number(opt->checksum) : "0";

        QString parentStr = "None (Root)";
        if (opt && opt->parentId.has_value() && m) {
            auto parentOpt = m->getNodeById(*opt->parentId);
            if (parentOpt)
                parentStr = QString("%1 (#%2)").arg(QString::fromStdString(parentOpt->name).toHtmlEscaped()).arg(*opt->parentId);
            else
                parentStr = QString("#%1").arg(*opt->parentId);
        }

        out << "      <g class=\"graph-node\" transform=\"translate(" << scenePos.x() << "," << scenePos.y() << ")\""
            << " data-id=\"" << node->nodeId() << "\" data-title=\"" << title << "\" data-type=\"" << type
            << "\" data-kind=\"node\""
            << " data-parent=\"" << parentStr
            << "\" data-status=\"" << status
            << "\" data-reviewer=\"" << reviewer
            << "\" data-checksum=\"" << checksum
            << "\" data-meta=\"" << meta << "\" data-attr=\"" << attr << "\">\n";
        out << "        <rect class=\"body\" width=\"" << w << "\" height=\"" << h << "\" pointer-events=\"all\"/>\n";
        out << "        <text class=\"title\" x=\"" << (w / 2.0) << "\" y=\"" << titleY
            << "\" text-anchor=\"middle\" dominant-baseline=\"middle\">" << title << "</text>\n";
        out << "        <text class=\"body\" x=\"" << (w / 2.0) << "\" y=\"" << bodyY
            << "\" text-anchor=\"middle\" dominant-baseline=\"middle\">" << type << "</text>\n";
        out << "      </g>\n";
    }
    out << "    </g>\n";

    out << "  </g>\n"; // End #viewport-group
    out << "</svg>\n";

    // Floating Controls
    out << "<div id=\"controls-bar\">\n";
    out << "  <button id=\"btn-zoom-in\" title=\"Zoom In\">+</button>\n";
    out << "  <button id=\"btn-zoom-reset\" title=\"Reset / Fit View\">&#x2299;</button>\n";
    out << "  <button id=\"btn-zoom-out\" title=\"Zoom Out\">&minus;</button>\n";
    out << "</div>\n";

    out << "</div>\n"; // End #canvas-container

    // Sidebar & Interactivity JS
    out << "<div id=\"sidebar\">\n";
    out << "  <div class=\"sb-header\">\n";
    out << "    <h3 class=\"sb-title\" id=\"sb-title\">Inspector</h3>\n";
    out << "    <button class=\"sb-close\" id=\"sb-close-btn\">&times;</button>\n";
    out << "  </div>\n";
    out << "  <div class=\"sb-content\" id=\"sb-body\"></div>\n";
    out << "</div>\n";

    out << "<script>\n";
    out << "  const svg = document.getElementById('main-svg');\n";
    out << "  const g = document.getElementById('viewport-group');\n";
    out << "  const container = document.getElementById('canvas-container');\n";
    out << "  const sidebar = document.getElementById('sidebar');\n";
    out << "  const closeBtn = document.getElementById('sb-close-btn');\n";
    out << "  const edgesLayer = document.getElementById('edges-layer');\n";
    out << "  const nodesLayer = document.getElementById('nodes-layer');\n";
    out << "\n";
    out << "  let isPanning = false, startX = 0, startY = 0;\n";
    out << "  let currentScale = 1.0, currentTx = 0, currentTy = 0;\n";
    out << "  let selectedElement = null;\n";
    out << "\n";
    out << "  function updateTransform() {\n";
    out << "    g.setAttribute('transform', `translate(${currentTx}, ${currentTy}) scale(${currentScale})`);\n";
    out << "  }\n";
    out << "\n";
    out << "  // Zoom centered on cursor position\n";
    out << "  container.addEventListener('wheel', (e) => {\n";
    out << "    e.preventDefault();\n";
    out << "    const zoomFactor = e.deltaY < 0 ? 1.12 : 0.89;\n";
    out << "    const rect = svg.getBoundingClientRect();\n";
    out << "    const mouseX = e.clientX - rect.left;\n";
    out << "    const mouseY = e.clientY - rect.top;\n";
    out << "    const svgX = (mouseX - currentTx) / currentScale;\n";
    out << "    const svgY = (mouseY - currentTy) / currentScale;\n";
    out << "    currentScale *= zoomFactor;\n";
    out << "    currentTx = mouseX - svgX * currentScale;\n";
    out << "    currentTy = mouseY - svgY * currentScale;\n";
    out << "    updateTransform();\n";
    out << "  }, { passive: false });\n";
    out << "\n";
    out << "  // Pan handling\n";
    out << "  container.addEventListener('mousedown', (e) => {\n";
    out << "    if (e.target.closest('#sidebar') || e.target.closest('#controls-bar')) return;\n";
    out << "    if (e.target.closest('.graph-node') || e.target.closest('.graph-container') || e.target.closest('.graph-edge')) return;\n";
    out << "    isPanning = true;\n";
    out << "    container.classList.add('panning');\n";
    out << "    startX = e.clientX - currentTx;\n";
    out << "    startY = e.clientY - currentTy;\n";
    out << "  });\n";
    out << "\n";
    out << "  window.addEventListener('mousemove', (e) => {\n";
    out << "    if (!isPanning) return;\n";
    out << "    currentTx = e.clientX - startX;\n";
    out << "    currentTy = e.clientY - startY;\n";
    out << "    updateTransform();\n";
    out << "  });\n";
    out << "\n";
    out << "  window.addEventListener('mouseup', () => {\n";
    out << "    isPanning = false;\n";
    out << "    container.classList.remove('panning');\n";
    out << "  });\n";
    out << "\n";
    out << "  // Zoom buttons\n";
    out << "  function zoomBy(factor) {\n";
    out << "    const rect = svg.getBoundingClientRect();\n";
    out << "    const cx = rect.width / 2;\n";
    out << "    const cy = rect.height / 2;\n";
    out << "    const svgX = (cx - currentTx) / currentScale;\n";
    out << "    const svgY = (cy - currentTy) / currentScale;\n";
    out << "    currentScale *= factor;\n";
    out << "    currentTx = cx - svgX * currentScale;\n";
    out << "    currentTy = cy - svgY * currentScale;\n";
    out << "    updateTransform();\n";
    out << "  }\n";
    out << "  document.getElementById('btn-zoom-in').addEventListener('click', () => zoomBy(1.2));\n";
    out << "  document.getElementById('btn-zoom-out').addEventListener('click', () => zoomBy(0.8));\n";
    out << "  document.getElementById('btn-zoom-reset').addEventListener('click', () => {\n";
    out << "    currentScale = 1.0; currentTx = 0; currentTy = 0;\n";
    out << "    updateTransform();\n";
    out << "  });\n";
    out << "\n";
    out << "  function formatJson(jsonStr) {\n";
    out << "    try { return JSON.stringify(JSON.parse(jsonStr), null, 2); }\n";
    out << "    catch (e) { return jsonStr || '{}'; }\n";
    out << "  }\n";
    out << "\n";
    out << "  function clearSelection() {\n";
    out << "    if (selectedElement) {\n";
    out << "      selectedElement.classList.remove('selected');\n";
    out << "      selectedElement = null;\n";
    out << "    }\n";
    out << "    sidebar.classList.remove('open');\n";
    out << "  }\n";
    out << "\n";
    out << "  closeBtn.addEventListener('click', clearSelection);\n";
    out << "  window.addEventListener('keydown', (e) => { if (e.key === 'Escape') clearSelection(); });\n";
    out << "  container.addEventListener('click', (e) => {\n";
    out << "    if (!e.target.closest('.graph-node') && !e.target.closest('.graph-container') && !e.target.closest('.graph-edge')) {\n";
    out << "      clearSelection();\n";
    out << "    }\n";
    out << "  });\n";
    out << "\n";
    out << "  // Node & Container Selection\n";
    out << "  document.querySelectorAll('.graph-node, .graph-container').forEach(el => {\n";
    out << "    el.addEventListener('click', (e) => {\n";
    out << "      e.stopPropagation();\n";
    out << "      if (selectedElement) selectedElement.classList.remove('selected');\n";
    out << "      selectedElement = el;\n";
    out << "      const isCont = el.classList.contains('graph-container');\n";
    out << "      if (!isCont && el.parentElement) el.parentElement.appendChild(el);\n";
    out << "      el.classList.add('selected');\n";
    out << "\n";
    out << "      document.getElementById('sb-title').innerText = (isCont ? 'Container: ' : 'Node: ') + el.getAttribute('data-title');\n";
    out << "      const status = el.getAttribute('data-status') || 'new';\n";
    out << "      const reviewer = el.getAttribute('data-reviewer') || 'None';\n";
    out << "      const checksum = el.getAttribute('data-checksum') || '0';\n";
    out << "      const parent = el.getAttribute('data-parent') || 'None (Root)';\n";
    out << "\n";
    out << "      document.getElementById('sb-body').innerHTML = `\n";
    out << "        <div class=\"sb-section\">\n";
    out << "          <div class=\"sb-section-title\">Identity & Status</div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">ID</span><span class=\"field-val\">#${el.getAttribute('data-id')}</span></div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">Name</span><span class=\"field-val\">${el.getAttribute('data-title')}</span></div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">Type</span><span class=\"field-val\">${el.getAttribute('data-type')}</span></div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">Status</span><span class=\"field-val\"><span class=\"status-badge status-${status.toLowerCase()}\">${status}</span></span></div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">Parent</span><span class=\"field-val\">${parent}</span></div>\n";
    out << "        </div>\n";
    out << "        <div class=\"sb-section\">\n";
    out << "          <div class=\"sb-section-title\">Governance</div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">Reviewer</span><span class=\"field-val\">${reviewer}</span></div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">Checksum</span><span class=\"field-val\">${checksum}</span></div>\n";
    out << "        </div>\n";
    out << "        <div class=\"sb-section\">\n";
    out << "          <div class=\"sb-section-title\">Attributes</div>\n";
    out << "          <pre class=\"code-block\">${formatJson(el.getAttribute('data-attr'))}</pre>\n";
    out << "        </div>\n";
    out << "        <div class=\"sb-section\">\n";
    out << "          <div class=\"sb-section-title\">Metadata / Layout</div>\n";
    out << "          <pre class=\"code-block\">${formatJson(el.getAttribute('data-meta'))}</pre>\n";
    out << "        </div>\n";
    out << "      `;\n";
    out << "      sidebar.classList.add('open');\n";
    out << "    });\n";
    out << "  });\n";
    out << "\n";
    out << "  // Edge Selection\n";
    out << "  document.querySelectorAll('.graph-edge').forEach(el => {\n";
    out << "    el.addEventListener('click', (e) => {\n";
    out << "      e.stopPropagation();\n";
    out << "      if (selectedElement) selectedElement.classList.remove('selected');\n";
    out << "      selectedElement = el;\n";
    out << "      el.classList.add('selected');\n";
    out << "      edgesLayer.appendChild(el);\n";
    out << "\n";
    out << "      document.getElementById('sb-title').innerText = 'Edge #' + el.getAttribute('data-id');\n";
    out << "      const status = el.getAttribute('data-status') || 'new';\n";
    out << "      const reviewer = el.getAttribute('data-reviewer') || 'None';\n";
    out << "      const checksum = el.getAttribute('data-checksum') || '0';\n";
    out << "      const srcName = el.getAttribute('data-src-name');\n";
    out << "      const srcId = el.getAttribute('data-src-id');\n";
    out << "      const dstName = el.getAttribute('data-dst-name');\n";
    out << "      const dstId = el.getAttribute('data-dst-id');\n";
    out << "      const edgeType = el.getAttribute('data-type') || 'Connection';\n";
    out << "\n";
    out << "      document.getElementById('sb-body').innerHTML = `\n";
    out << "        <div class=\"sb-section\">\n";
    out << "          <div class=\"sb-section-title\">Connection</div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">ID</span><span class=\"field-val\">#${el.getAttribute('data-id')}</span></div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">Type</span><span class=\"field-val\">${edgeType}</span></div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">From</span><span class=\"field-val\">${srcName} (#${srcId})</span></div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">To</span><span class=\"field-val\">${dstName} (#${dstId})</span></div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">Status</span><span class=\"field-val\"><span class=\"status-badge status-${status.toLowerCase()}\">${status}</span></span></div>\n";
    out << "        </div>\n";
    out << "        <div class=\"sb-section\">\n";
    out << "          <div class=\"sb-section-title\">Governance</div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">Reviewer</span><span class=\"field-val\">${reviewer}</span></div>\n";
    out << "          <div class=\"field-row\"><span class=\"field-name\">Checksum</span><span class=\"field-val\">${checksum}</span></div>\n";
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