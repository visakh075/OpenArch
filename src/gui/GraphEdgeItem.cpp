#include "GraphEdgeItem.h"
#include "GraphNodeItem.h"
#include "EdgeEditorDialog.h"

#include <QPainter>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
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
      dst_(dst)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);

    setZValue(0);

    normalPen_ = QPen(Qt::darkGray, 2);
    highlightPen_ = QPen(Qt::blue, 3);
}

QPointF GraphEdgeItem::sourceCenter() const
{
    if (!src_)
        return QPointF();

    return mapFromItem(src_,
                       src_->boundingRect().center());
}

QPointF GraphEdgeItem::destCenter() const
{
    if (!dst_)
        return QPointF();

    return mapFromItem(dst_,
                       dst_->boundingRect().center());
}

QPainterPath GraphEdgeItem::buildPath() const
{
    QPainterPath path;

    if (!src_ || !dst_)
        return path;

    QPointF p1 = sourceCenter();
    QPointF p2 = destCenter();

    QPointF mid;

    // Auto choose routing direction
    if (qAbs(p1.x() - p2.x()) > qAbs(p1.y() - p2.y())) {
        // Horizontal first
        mid = QPointF(p2.x(), p1.y());
    } else {
        // Vertical first
        mid = QPointF(p1.x(), p2.y());
    }

    path.moveTo(p1);
    path.lineTo(mid);
    path.lineTo(p2);

    return path;
}

QRectF GraphEdgeItem::boundingRect() const
{
    if (!src_ || !dst_)
        return QRectF();

    QPointF p1 = sourceCenter();
    QPointF p2 = destCenter();

    QRectF rect(p1, p2);
    rect = rect.normalized();

    const qreal extra = 20;
    rect.adjust(-extra, -extra, extra, extra);

    return rect;
}

void GraphEdgeItem::paint(QPainter* painter,
                          const QStyleOptionGraphicsItem*,
                          QWidget*)
{
    if (!src_ || !dst_)
        return;

    painter->setRenderHint(QPainter::Antialiasing);

    // --- Scene positions ---
    QPointF srcCenter = src_->scenePos();
    QPointF dstCenter = dst_->scenePos();

    QRectF srcRect = src_->mapToScene(src_->boundingRect()).boundingRect();
    QRectF dstRect = dst_->mapToScene(dst_->boundingRect()).boundingRect();

    QPointF p1 = srcCenter;
    QPointF p2 = dstCenter;

    double dx = dstCenter.x() - srcCenter.x();
    double dy = dstCenter.y() - srcCenter.y();

    bool horizontal = std::abs(dx) > std::abs(dy);

    // ---- Anchor source ----
    if (horizontal) {
        if (dx > 0)
            p1 = QPointF(srcRect.right(), srcCenter.y());
        else
            p1 = QPointF(srcRect.left(), srcCenter.y());
    } else {
        if (dy > 0)
            p1 = QPointF(srcCenter.x(), srcRect.bottom());
        else
            p1 = QPointF(srcCenter.x(), srcRect.top());
    }

    // ---- Anchor destination ----
    if (horizontal) {
        if (dx > 0)
            p2 = QPointF(dstRect.left(), dstCenter.y());
        else
            p2 = QPointF(dstRect.right(), dstCenter.y());
    } else {
        if (dy > 0)
            p2 = QPointF(dstCenter.x(), dstRect.top());
        else
            p2 = QPointF(dstCenter.x(), dstRect.bottom());
    }

    // ---- Manhattan path ----
    QPainterPath path;
    path.moveTo(p1);

    QPointF mid;
    if (horizontal)
        mid = QPointF(p2.x(), p1.y());
    else
        mid = QPointF(p1.x(), p2.y());

    path.lineTo(mid);
    path.lineTo(p2);

    painter->setPen(hovered_ ? highlightPen_ : normalPen_);
    painter->drawPath(path);

    // =====================
    //       ARROW
    // =====================

    QLineF arrowLine(mid, p2);
    arrowLine.setLength(arrowLine.length() - 6);

    QPointF arrowTip = arrowLine.p2();
    double angle = std::atan2(-arrowLine.dy(), arrowLine.dx());

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
}

QPainterPath GraphEdgeItem::shape() const
{
    QPainterPath path = buildPath();

    QPainterPathStroker stroker;
    stroker.setWidth(12);  // clickable thickness

    return stroker.createStroke(path);
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
    setCursor(QCursor(Qt::PointingHandCursor));
    update();

    QGraphicsObject::hoverEnterEvent(event);
}

void GraphEdgeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = false;
    setZValue(0);
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
