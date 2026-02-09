#include "GraphEdgeItem.h"

#include <QPainter>
#include <QtMath>
#include <QCursor>

GraphEdgeItem::GraphEdgeItem(ArchitectureModel* model,
                             const EdgeData& edge,
                             QGraphicsItem* parent)
    : QGraphicsObject(parent),
      model_(model),
      edgeId_(edge.id),
      srcId_(edge.srcNode),
      dstId_(edge.dstNode),
      label_(QString::fromStdString(edge.edgeType))
{
    setAcceptHoverEvents(true);
    setZValue(-1);
}

void GraphEdgeItem::setEndpoints(const QPointF& src,
                                 const QPointF& dst)
{
    prepareGeometryChange();
    src_ = src;
    dst_ = dst;
}

QRectF GraphEdgeItem::boundingRect() const
{
    return QRectF(src_, dst_)
        .normalized()
        .adjusted(-20, -20, 20, 20);
}

void GraphEdgeItem::paint(QPainter* painter,
                           const QStyleOptionGraphicsItem*,
                           QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    QPen pen(hovered_ ? Qt::darkBlue : Qt::black, 1.5);
    painter->setPen(pen);

    painter->drawLine(src_, dst_);

    // Arrow
    QLineF line(src_, dst_);
    double angle = std::atan2(-line.dy(), line.dx());
    constexpr qreal arrowSize = 10;

    QPointF p1 = dst_ - QPointF(
        std::cos(angle + M_PI / 6) * arrowSize,
        -std::sin(angle + M_PI / 6) * arrowSize);

    QPointF p2 = dst_ - QPointF(
        std::cos(angle - M_PI / 6) * arrowSize,
        -std::sin(angle - M_PI / 6) * arrowSize);

    painter->setBrush(pen.color());
    painter->drawPolygon(QPolygonF() << dst_ << p1 << p2);

    painter->drawText((src_ + dst_) / 2 + QPointF(4, -4),
                      label_);
}

void GraphEdgeItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    hovered_ = true;
    setCursor(QCursor(Qt::PointingHandCursor));
    update();
}

void GraphEdgeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    hovered_ = false;
    unsetCursor();
    update();
}
