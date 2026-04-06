#include "GraphEdgeItem.h"
#include "GraphNodeItem.h"
#include "EdgeEditorDialog.h"

#include <QPainter>
#include <QString>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainterPathStroker>
#include <QtMath>

GraphEdgeItem::GraphEdgeItem(
    ArchitectureModel* model,
    const EdgeData& edge,
    GraphNodeItem* src,
    GraphNodeItem* dst,
    QGraphicsItem* parent)
    : QGraphicsObject(parent),
      model_(model),
      edge_(edge),
      src_(src),
      dst_(dst),
      srcPort_(Port::Right),
      dstPort_(Port::Left)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setZValue(0);

    normalPen_ = QPen(Qt::darkGray, 2);
    highlightPen_ = QPen(Qt::blue, 3);
}

QPointF GraphEdgeItem::portScenePosition(GraphNodeItem* node,
                                         Port port) const
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

void GraphEdgeItem::autoSelectPorts(Port& srcPort,
                                    Port& dstPort) const
{
    QRectF srcRect = src_->mapToScene(src_->boundingRect()).boundingRect();
    QRectF dstRect = dst_->mapToScene(dst_->boundingRect()).boundingRect();

    QPointF srcCenter = srcRect.center();
    QPointF dstCenter = dstRect.center();

    double dx = dstCenter.x() - srcCenter.x();
    double dy = dstCenter.y() - srcCenter.y();

    if (std::abs(dx) > std::abs(dy))
    {
        if (dx > 0)
        {
            srcPort = Port::Right;
            dstPort = Port::Left;
        }
        else
        {
            srcPort = Port::Left;
            dstPort = Port::Right;
        }
    }
    else
    {
        if (dy > 0)
        {
            srcPort = Port::Bottom;
            dstPort = Port::Top;
        }
        else
        {
            srcPort = Port::Top;
            dstPort = Port::Bottom;
        }
    }
}

QPainterPath GraphEdgeItem::buildPath() const
{
    QPainterPath path;

    if (!src_ || !dst_)
        return path;

    // --- Scene rectangles ---
    QRectF srcRect = src_->mapToScene(src_->boundingRect()).boundingRect();
    QRectF dstRect = dst_->mapToScene(dst_->boundingRect()).boundingRect();

    QPointF srcCenter = srcRect.center();
    QPointF dstCenter = dstRect.center();

    // --- Compute gaps ---
    double gapRight  = dstRect.left()  - srcRect.right();
    double gapLeft   = srcRect.left()  - dstRect.right();
    double gapBottom = dstRect.top()   - srcRect.bottom();
    double gapTop    = srcRect.top()   - dstRect.bottom();

    // --- Choose anchor points (side midpoints) ---
    QPointF SrcPoint, EndPoint;
    // QPointF p1,p2;
    // Horizontal separation
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
    // Vertical separation
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
    // Overlapping case
    else
    {
        double dx = dstCenter.x() - srcCenter.x();
        double dy = dstCenter.y() - srcCenter.y();

        if (std::abs(dx) > std::abs(dy))
        {
            if (dx > 0)
            {
                SrcPoint = QPointF(srcRect.right(), srcCenter.y());
                EndPoint = QPointF(dstRect.left(),  dstCenter.y());
            }
            else
            {
                SrcPoint = QPointF(srcRect.left(),  srcCenter.y());
                EndPoint = QPointF(dstRect.right(), dstCenter.y());
            }
        }
        else
        {
            if (dy > 0)
            {
                SrcPoint = QPointF(srcCenter.x(), srcRect.bottom());
                EndPoint = QPointF(dstCenter.x(), dstRect.top());
            }
            else
            {
                SrcPoint = QPointF(srcCenter.x(), srcRect.top());
                EndPoint = QPointF(dstCenter.x(), dstRect.bottom());
            }
        }
    }

    // Convert scene → local
    SrcPoint = mapFromScene(SrcPoint);
    EndPoint = mapFromScene(EndPoint);

    // SrcPoint = p1;
    // EndPoint = p2;

    path.moveTo(SrcPoint);

    // --- Midpoints for routing ---
    double midX = (SrcPoint.x() + EndPoint.x()) / 2.0;
    double midY = (SrcPoint.y() + EndPoint.y()) / 2.0;

    bool overlapX = (gapRight <= 0 && gapLeft <= 0);
    bool overlapY = (gapBottom <= 0 && gapTop <= 0);

    // Case 1: Horizontal overlap → vertical main axis
    if (overlapX && !overlapY)
    {
        path.lineTo(SrcPoint.x(), midY);
        path.lineTo(EndPoint.x(), midY);
    }
    // Case 2: Vertical overlap → horizontal main axis
    else if (overlapY && !overlapX)
    {
        path.lineTo(midX, SrcPoint.y());
        path.lineTo(midX, EndPoint.y());
    }
    // Case 3: Overlap both axes
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
    // Case 4: Fully separated
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
    QPainterPath path = buildPath();
    QRectF rect = path.boundingRect();
    rect.adjust(-20, -20, 20, 20);
    return rect;
}

void GraphEdgeItem::paint(QPainter* painter,
                          const QStyleOptionGraphicsItem*,
                          QWidget*)
{
    if (!src_ || !dst_)
        return;

    painter->setRenderHint(QPainter::Antialiasing);

    QPainterPath path = buildPath();

    painter->setPen(hovered_ ? highlightPen_ : normalPen_);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);

    // Arrow
    if (path.elementCount() < 2)
        return;

    QPainterPath::Element e1 = path.elementAt(path.elementCount() - 1);
    QPainterPath::Element e2 = path.elementAt(path.elementCount() - 2);

    QPointF last(e1.x, e1.y);
    QPointF prev(e2.x, e2.y);

    QLineF line(prev, last);
    line.setLength(line.length() - 6);

    QPointF arrowTip = line.p2();
    double angle = std::atan2(-line.dy(), line.dx());
    const double arrowSize = 10.0;

    QPointF arrowP1 = arrowTip - QPointF(
        std::cos(angle + M_PI / 6) * arrowSize,
        -std::sin(angle + M_PI / 6) * arrowSize);

    QPointF arrowP2 = arrowTip - QPointF(
        std::cos(angle - M_PI / 6) * arrowSize,
        -std::sin(angle - M_PI / 6) * arrowSize);

    QPolygonF arrowHead;
    arrowHead << arrowTip << arrowP1 << arrowP2;

    painter->setBrush(hovered_ ? Qt::blue : Qt::darkGray);
    painter->drawPolygon(arrowHead);

    // Add title to the node
    if (hovered_)
    {
        QString title = QString::fromStdString(edge_.edgeType);

        QFont font("Arial", 10);
        painter->setFont(font);

        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(title);

        // --- Midpoint of path ---
        qreal t = 0.5;
        QPointF p1 = path.pointAtPercent(t);
        QPointF p2 = path.pointAtPercent(t + 0.01); // small step ahead

        // --- Angle of edge ---
        double angle = std::atan2(p2.y() - p1.y(),
                                p2.x() - p1.x());

        // Convert to degrees
        double degrees = angle * 180.0 / M_PI;

        // Keep text upright (avoid upside-down text)
        if (degrees > 90 || degrees < -90)
            degrees += 180;

        painter->save();

        // Move to midpoint
        painter->translate(p1);

        // Rotate along edge
        painter->rotate(degrees);

        // Offset text slightly above line
        QPointF textPos(-textRect.width() / 2,
                        -5);

        // Optional: background for readability
        QRect bgRect = textRect.adjusted(-4, -2, 4, 2);
        bgRect.moveCenter(QPoint(0, -5));

        // painter->setBrush(Qt::white);
        // painter->setPen(Qt::NoPen);
        // painter->drawRect(bgRect);

        // Draw text
        painter->setPen(Qt::white);
        painter->drawText(textPos, title);

        painter->restore();
    }

}

QPainterPath GraphEdgeItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(12);
    return stroker.createStroke(buildPath());
}

void GraphEdgeItem::updateEndpoints()
{
    prepareGeometryChange();
    update();
}

void GraphEdgeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = true;
    setZValue(10);
    setCursor(Qt::PointingHandCursor);
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void GraphEdgeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = false;
    setZValue(-1);
    unsetCursor();
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

void GraphEdgeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (!model_)
        return;

    EdgeEditorDialog dlg(model_, edge_.id);
    dlg.exec();

    QGraphicsObject::mouseDoubleClickEvent(event);
}
