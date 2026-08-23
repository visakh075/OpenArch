#include "GraphEdgeItem.h"
#include "GraphNodeItem.h"
#include "GraphView.h"
#include "EdgeEditorDialog.h"
#include "GraphThemeManager.h"

#include <QPainter>
#include <QString>
#include <QMenu>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainterPathStroker>
#include <QtMath>

GraphEdgeItem::GraphEdgeItem(
    ArchitectureModel* model,
    const EdgeId id,
    GraphNodeItem* src,
    GraphNodeItem* dst,
    QGraphicsItem* parent)
    : QGraphicsObject(parent),
      model_(model),
      src_(src),
      dst_(dst),
      srcPort_(Port::Right),
      dstPort_(Port::Left)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setZValue(-1);
    e_id = id;

    if (src_)
        src_->addEdge(this);
    if (dst_)
        dst_->addEdge(this);

    normalPen_ = QPen(Qt::darkGray, 2);
    highlightPen_ = QPen(Qt::blue, 3);
    cachedPath_ = buildPath();

    refreshLayout();
}

GraphEdgeItem::~GraphEdgeItem()
{
    if (src_)
        src_->removeEdge(this);
    if (dst_)
        dst_->removeEdge(this);
}

QPointF GraphEdgeItem::portScenePosition(GraphNodeItem* node, Port port) const
{
    QRectF rect = node->mapToScene(node->boundingRect()).boundingRect();
    QPointF c = rect.center();

    switch (port)
    {
        case Port::Top:    return QPointF(c.x(), rect.top());
        case Port::Bottom: return QPointF(c.x(), rect.bottom());
        case Port::Left:   return QPointF(rect.left(), c.y());
        case Port::Right:  return QPointF(rect.right(), c.y());
    }
    return QPointF();
}

void GraphEdgeItem::autoSelectPorts(Port& srcPort, Port& dstPort) const
{
    QRectF srcRect = src_->mapToScene(src_->boundingRect()).boundingRect();
    QRectF dstRect = dst_->mapToScene(dst_->boundingRect()).boundingRect();

    QPointF srcCenter = srcRect.center();
    QPointF dstCenter = dstRect.center();

    double dx = dstCenter.x() - srcCenter.x();
    double dy = dstCenter.y() - srcCenter.y();

    if (std::abs(dx) > std::abs(dy))
    {
        srcPort = (dx > 0) ? Port::Right : Port::Left;
        dstPort = (dx > 0) ? Port::Left  : Port::Right;
    }
    else
    {
        srcPort = (dy > 0) ? Port::Bottom : Port::Top;
        dstPort = (dy > 0) ? Port::Top    : Port::Bottom;
    }
}

QPainterPath GraphEdgeItem::buildPath() const
{
    QPainterPath path;
    if (!src_ || !dst_)
        return path;

    QRectF srcRect = src_->mapToScene(src_->boundingRect()).boundingRect();
    QRectF dstRect = dst_->mapToScene(dst_->boundingRect()).boundingRect();

    QPointF srcCenter = srcRect.center();
    QPointF dstCenter = dstRect.center();

    double gapRight  = dstRect.left()  - srcRect.right();
    double gapLeft   = srcRect.left()  - dstRect.right();
    double gapBottom = dstRect.top()   - srcRect.bottom();
    double gapTop    = srcRect.top()   - dstRect.bottom();

    QPointF SrcPoint, EndPoint;

    if (gapRight > 0)
    {
        SrcPoint = QPointF(srcRect.right(), srcCenter.y());
        EndPoint = QPointF(dstRect.left(),  dstCenter.y());
    }
    else if (gapLeft > 0)
    {
        SrcPoint = QPointF(srcRect.left(),  srcCenter.y());
        EndPoint = QPointF(dstRect.right(), dstCenter.y());
    }
    else if (gapBottom > 0)
    {
        SrcPoint = QPointF(srcCenter.x(), srcRect.bottom());
        EndPoint = QPointF(dstCenter.x(), dstRect.top());
    }
    else if (gapTop > 0)
    {
        SrcPoint = QPointF(srcCenter.x(), srcRect.top());
        EndPoint = QPointF(dstCenter.x(), dstRect.bottom());
    }
    else
    {
        double dx = dstCenter.x() - srcCenter.x();
        double dy = dstCenter.y() - srcCenter.y();

        if (std::abs(dx) > std::abs(dy))
        {
            SrcPoint = (dx > 0) ? QPointF(srcRect.right(), srcCenter.y()) : QPointF(srcRect.left(), srcCenter.y());
            EndPoint = (dx > 0) ? QPointF(dstRect.left(), dstCenter.y())  : QPointF(dstRect.right(), dstCenter.y());
        }
        else
        {
            SrcPoint = (dy > 0) ? QPointF(srcCenter.x(), srcRect.bottom()) : QPointF(srcCenter.x(), srcRect.top());
            EndPoint = (dy > 0) ? QPointF(dstCenter.x(), dstRect.top())    : QPointF(dstCenter.x(), dstRect.bottom());
        }
    }

    SrcPoint = mapFromScene(SrcPoint);
    EndPoint = mapFromScene(EndPoint);

    path.moveTo(SrcPoint);

    double midX = (SrcPoint.x() + EndPoint.x()) / 2.0;
    double midY = (SrcPoint.y() + EndPoint.y()) / 2.0;

    bool overlapX = (gapRight <= 0 && gapLeft <= 0);
    bool overlapY = (gapBottom <= 0 && gapTop <= 0);

    if (overlapX && !overlapY)
    {
        path.lineTo(SrcPoint.x(), midY);
        path.lineTo(EndPoint.x(), midY);
    }
    else if (overlapY && !overlapX)
    {
        path.lineTo(midX, SrcPoint.y());
        path.lineTo(midX, EndPoint.y());
    }
    else if (overlapX && overlapY)
    {
        double dx = std::abs(EndPoint.x() - SrcPoint.x());
        double dy = std::abs(EndPoint.y() - SrcPoint.y());

        if (dx > dy)
        {
            path.lineTo(midX, SrcPoint.y());
            path.lineTo(midX, EndPoint.y());
        }
        else
        {
            path.lineTo(SrcPoint.x(), midY);
            path.lineTo(EndPoint.x(), midY);
        }
    }
    else
    {
        if (gapRight > 0 || gapLeft > 0)
        {
            path.lineTo(midX, SrcPoint.y());
            path.lineTo(midX, EndPoint.y());
        }
        else
        {
            path.lineTo(SrcPoint.x(), midY);
            path.lineTo(EndPoint.x(), midY);
        }
    }

    path.lineTo(EndPoint);
    return path;
}

QRectF GraphEdgeItem::boundingRect() const
{
    QRectF rect = cachedPath_.boundingRect();
    rect.adjust(-20, -20, 20, 20);
    return rect;
}

void GraphEdgeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (!src_ || !dst_)
        return;

    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphEdgeState* State = isSelected() ? &theme.edge.selected : (hovered_ ? &theme.edge.hover : &theme.edge.normal);

    const auto& arrow = State->arrow;
    const auto& label = State->label;

    painter->setRenderHint(QPainter::Antialiasing);

    QPainterPath fullPath = cachedPath_;
    if (fullPath.elementCount() < 2)
        return;

    QPainterPath::Element e1 = fullPath.elementAt(fullPath.elementCount() - 1);
    QPainterPath::Element e2 = fullPath.elementAt(fullPath.elementCount() - 2);

    QPointF last(e1.x, e1.y);
    QPointF prev(e2.x, e2.y);
    QLineF line(prev, last);

    QPointF arrowTip = line.p2();
    double angle = std::atan2(-line.dy(), line.dx());

    double arrowWidth = arrow.width;
    double arrowHeight = arrow.height;

    QPointF arrowP1 = arrowTip - QPointF(
        std::cos(angle) * arrowWidth - std::sin(angle) * arrowHeight / 2,
        -std::sin(angle) * arrowWidth - std::cos(angle) * arrowHeight / 2);

    QPointF arrowP2 = arrowTip - QPointF(
        std::cos(angle) * arrowWidth + std::sin(angle) * arrowHeight / 2,
        -std::sin(angle) * arrowWidth + std::cos(angle) * arrowHeight / 2);

    QPointF arrowBaseCenter = (arrowP1 + arrowP2) / 2.0;

    QPainterPath shortenedPath;
    for (int i = 0; i < fullPath.elementCount(); ++i)
    {
        auto e = fullPath.elementAt(i);
        QPointF p(e.x, e.y);

        if (i == 0)
            shortenedPath.moveTo(p);
        else if (i == fullPath.elementCount() - 1)
            shortenedPath.lineTo(arrowBaseCenter);
        else
            shortenedPath.lineTo(p);
    }

    QPen edgePen(State->lineColor);
    edgePen.setWidth(State->lineWidth);
    edgePen.setJoinStyle(Qt::RoundJoin);
    edgePen.setCapStyle(Qt::RoundCap);

    painter->setPen(edgePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(shortenedPath);

    QPolygonF arrowHead;
    arrowHead << arrowTip << arrowP1 << arrowP2;

    QPen arrowPen(arrow.lineColor);
    arrowPen.setWidth(arrow.lineWidth);
    arrowPen.setJoinStyle(Qt::RoundJoin);

    painter->setPen(arrowPen);
    painter->setBrush(arrow.fillColor);
    painter->drawPolygon(arrowHead);

    QString title = cachedTitle_;
    QFont font;
    font.setPointSize(label.fontSize);
    font.setBold(label.bold);
    painter->setFont(font);

    QRect textRect = cachedTitleRect_;
    qreal t = 0.5;
    QPointF p1 = fullPath.pointAtPercent(t);
    QPointF p2 = fullPath.pointAtPercent(t + 0.01);

    double textAngle = std::atan2(p2.y() - p1.y(), p2.x() - p1.x());
    double degrees = textAngle * 180.0 / M_PI;

    if (degrees > 90 || degrees < -90)
        degrees += 180;

    painter->save();
    painter->translate(p1);
    painter->rotate(degrees);

    QPointF textPos(-textRect.width() / 2, -label.offset);
    QRect bgRect = textRect.adjusted(-label.paddingX, -label.paddingY, label.paddingX, label.paddingY);
    bgRect.moveCenter(QPoint(0, -label.offset));

    QPen bgPen(label.borderColor);
    bgPen.setWidth(label.borderWidth);

    painter->setPen(bgPen);
    painter->setBrush(label.backgroundColor);
    painter->drawRoundedRect(bgRect, label.radius, label.radius);

    painter->setPen(label.textColor);
    painter->drawText(textPos, title);
    painter->restore();
}

void GraphEdgeItem::refreshLayout()
{
    auto edge = model_->getEdgeById(e_id);
    if (!edge)
        return;

    cachedTitle_ = QString::fromStdString(edge->edgeType);
    QFont font;
    font.setPointSize(10);
    QFontMetrics fm(font);
    cachedTitleRect_ = fm.boundingRect(cachedTitle_);

    refreshPath();
}

QPainterPath GraphEdgeItem::shape() const
{
    QPainterPath result;
    QPainterPathStroker stroker;
    stroker.setWidth(12);
    result.addPath(stroker.createStroke(cachedPath_));

    QString title = cachedTitle_;
    if (!title.isEmpty())
    {
        const auto& theme = GraphThemeManager::instance()->theme();
        const GraphEdgeState* State = isSelected() ? &theme.edge.selected : (hovered_ ? &theme.edge.hover : &theme.edge.normal);
        const auto& label = State->label;

        QFont font;
        font.setPointSize(label.fontSize);
        font.setBold(label.bold);

        QRect textRect = cachedTitleRect_;
        QPainterPath path = cachedPath_;
        QPointF p = path.pointAtPercent(0.5);

        QRectF bgRect = textRect.adjusted(-label.paddingX, -label.paddingY, label.paddingX, label.paddingY);
        bgRect.moveCenter(QPointF(p.x(), p.y() - label.offset));

        QPainterPath labelPath;
        labelPath.addRoundedRect(bgRect, label.radius, label.radius);
        result.addPath(labelPath);
    }

    return result;
}

void GraphEdgeItem::updateEndpoints()
{
    refreshPath();
}

void GraphEdgeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = true;
    setZValue(isSelected() ? 20 : 10);
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void GraphEdgeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = false;
    setZValue(isSelected() ? 20 : -1);
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

void GraphEdgeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (!model_)
        return;

    event->accept();
    EdgeEditorDialog dlg(model_, e_id);
    if (dlg.exec() == QDialog::Accepted)
    {
        refreshLayout();
    }
}

void GraphEdgeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    event->ignore();
}

void GraphEdgeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    setSelected(true);

    QMenu menu;
    QAction* editAct = menu.addAction("Edit");
    QAction* delAct = menu.addAction("Delete");

    QAction* selected = menu.exec(event->screenPos());
    if (selected == editAct)
    {
        EdgeEditorDialog dlg(model_, e_id);
        if (dlg.exec() == QDialog::Accepted)
            refreshLayout();
    }
    else if (selected == delAct)
    {
        if (!scene()->views().isEmpty())
        {
            if (auto* view = dynamic_cast<GraphView*>(scene()->views().first()))
                emit view->deleteRequested();
        }
    }
}

void GraphEdgeItem::refreshPath()
{
    prepareGeometryChange();
    cachedPath_ = buildPath();
    cachedBounds_ = cachedPath_.boundingRect();
    cachedBounds_.adjust(-20, -20, 20, 20);
    update();
}