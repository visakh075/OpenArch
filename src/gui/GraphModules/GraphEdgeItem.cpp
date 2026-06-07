#include "GraphEdgeItem.h"
#include "GraphNodeItem.h"
#include "EdgeEditorDialog.h"

#include <QPainter>
#include <QString>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainterPathStroker>
#include <QtMath>
#include "GraphThemeManager.h"
#include <QDebug>

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
    setZValue(-1);
    e_id = id;

    normalPen_ = QPen(Qt::darkGray, 2);
    highlightPen_ = QPen(Qt::blue, 3);
    cachedPath_ = buildPath();
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
    QRectF rect =
        cachedPath_.boundingRect();

    rect.adjust(
        -20,
        -20,
        20,
        20);

    return rect;
}
void GraphEdgeItem::paint(QPainter* painter,
                          const QStyleOptionGraphicsItem*,
                          QWidget*)
{
    if (!src_ || !dst_)
        return;

    const auto& theme =
        GraphThemeManager::instance()->theme();

    /*
     * EDGE STYLE
     */

    const GraphEdgeState* State =
        &theme.edge.normal;

    if (isSelected())
    {
        State = &theme.edge.selected;
    }
    else if (hovered_)
    {
        State = &theme.edge.hover;
    }

    const auto& arrow =
        State->arrow;

    const auto& label =
        State->label;

    painter->setRenderHint(
        QPainter::Antialiasing);

    /*
     * FULL PATH
     */

    QPainterPath fullPath =
        cachedPath_;

    if (fullPath.elementCount() < 2)
        return;

    /*
     * LAST SEGMENT
     */

    QPainterPath::Element e1 =
        fullPath.elementAt(
            fullPath.elementCount() - 1);

    QPainterPath::Element e2 =
        fullPath.elementAt(
            fullPath.elementCount() - 2);

    QPointF last(e1.x, e1.y);
    QPointF prev(e2.x, e2.y);

    /*
     * LINE
     */

    QLineF line(prev, last);

    /*
     * ARROW TIP
     */

    QPointF arrowTip =
        line.p2();

    double angle =
        std::atan2(
            -line.dy(),
            line.dx());

    double arrowWidth =
        arrow.width;

    double arrowHeight =
        arrow.height;

    /*
     * ARROW GEOMETRY
     */

    QPointF arrowP1 =
        arrowTip - QPointF(
            std::cos(angle) * arrowWidth -
            std::sin(angle) * arrowHeight / 2,

            -std::sin(angle) * arrowWidth -
            std::cos(angle) * arrowHeight / 2);

    QPointF arrowP2 =
        arrowTip - QPointF(
            std::cos(angle) * arrowWidth +
            std::sin(angle) * arrowHeight / 2,

            -std::sin(angle) * arrowWidth +
            std::cos(angle) * arrowHeight / 2);

    /*
     * LINE ENDS AT
     * ARROW BASE CENTER
     */

    QPointF arrowBaseCenter =
        (arrowP1 + arrowP2) / 2.0;

    /*
     * SHORTENED PATH
     */

    QPainterPath shortenedPath;

    for (int i = 0;
         i < fullPath.elementCount();
         ++i)
    {
        auto e =
            fullPath.elementAt(i);

        QPointF p(e.x, e.y);

        if (i == 0)
        {
            shortenedPath.moveTo(p);
        }
        else if (i ==
                 fullPath.elementCount() - 1)
        {
            shortenedPath.lineTo(
                arrowBaseCenter);
        }
        else
        {
            shortenedPath.lineTo(p);
        }
    }

    /*
     * EDGE LINE
     */

    QPen edgePen(
        State->lineColor);

    edgePen.setWidth(
        State->lineWidth);

    edgePen.setJoinStyle(
        Qt::RoundJoin);

    edgePen.setCapStyle(
        Qt::RoundCap);

    painter->setPen(edgePen);

    painter->setBrush(
        Qt::NoBrush);

    painter->drawPath(
        shortenedPath);

    /*
     * ARROW
     */

    QPolygonF arrowHead;

    arrowHead << arrowTip
              << arrowP1
              << arrowP2;

    QPen arrowPen(
        arrow.lineColor);

    arrowPen.setWidth(
        arrow.lineWidth);

    arrowPen.setJoinStyle(
        Qt::RoundJoin);

    painter->setPen(
        arrowPen);

    painter->setBrush(
        arrow.fillColor);

    painter->drawPolygon(
        arrowHead);

    /*
     * LABEL
     */

    auto edge_ = model_->getEdgeById(e_id);
    QString title =
        QString::fromStdString(
            edge_->edgeType);

    /*
        * FONT
        */

    QFont font;

    font.setPointSize(
        label.fontSize);

    font.setBold(
        label.bold);

    painter->setFont(font);

    QFontMetrics fm(font);

    QRect textRect =
        fm.boundingRect(title);

    /*
        * MIDPOINT
        */

    qreal t = 0.5;

    QPointF p1 =
        fullPath.pointAtPercent(t);

    QPointF p2 =
        fullPath.pointAtPercent(t + 0.01);

    /*
        * TEXT ANGLE
        */

    double textAngle =
        std::atan2(
            p2.y() - p1.y(),
            p2.x() - p1.x());

    double degrees =
        textAngle * 180.0 / M_PI;

    /*
        * KEEP TEXT UPRIGHT
        */

    if (degrees > 90 ||
        degrees < -90)
    {
        degrees += 180;
    }

    painter->save();

    painter->translate(p1);

    painter->rotate(degrees);

    /*
        * LABEL POSITION
        */

    QPointF textPos(
        -textRect.width() / 2,
        -label.offset);

    /*
        * BACKGROUND RECT
        */

    QRect bgRect =
        textRect.adjusted(
            -label.paddingX,
            -label.paddingY,
            label.paddingX,
            label.paddingY);

    bgRect.moveCenter(
        QPoint(0,
                -label.offset));

    /*
        * LABEL BACKGROUND
        */

    QPen bgPen(
        label.borderColor);

    bgPen.setWidth(
        label.borderWidth);

    painter->setPen(
        bgPen);

    painter->setBrush(
        label.backgroundColor);

    painter->drawRoundedRect(
        bgRect,
        label.radius,
        label.radius);

    /*
        * LABEL TEXT
        */

    painter->setPen(
        label.textColor);

    painter->drawText(
        textPos,
        title);

    painter->restore();

}

QPainterPath GraphEdgeItem::shape() const
{
    QPainterPath result;

    /*
     * EDGE HIT AREA
     */

    QPainterPathStroker stroker;

    stroker.setWidth(12);

    result.addPath(
        stroker.createStroke(
            buildPath()));

    /*
     * LABEL HIT AREA
     */
    
    auto edge_ = model_->getEdgeById(e_id);
    QString title =
        QString::fromStdString(
            edge_->edgeType);

    if (!title.isEmpty())
    {
        const auto& theme =
            GraphThemeManager::instance()
                ->theme();

        const GraphEdgeState* State =
            &theme.edge.normal;

        if (isSelected())
        {
            State =
                &theme.edge.selected;
        }
        else if (hovered_)
        {
            State =
                &theme.edge.hover;
        }

        const auto& label =
            State->label;

        /*
         * FONT
         */

        QFont font;

        font.setPointSize(
            label.fontSize);

        font.setBold(
            label.bold);

        QFontMetrics fm(font);

        QRect textRect =
            fm.boundingRect(title);

        /*
         * PATH MIDPOINT
         */

        QPainterPath path =
            buildPath();

        QPointF p =
            path.pointAtPercent(0.5);

        /*
         * LABEL RECT
         */

        QRectF bgRect =
            textRect.adjusted(
                -label.paddingX,
                -label.paddingY,
                label.paddingX,
                label.paddingY);

        bgRect.moveCenter(
            QPointF(
                p.x(),
                p.y() - label.offset));

        /*
         * ADD LABEL AREA
         */

        QPainterPath labelPath;

        labelPath.addRoundedRect(
            bgRect,
            label.radius,
            label.radius);

        result.addPath(
            labelPath);
    }

    return result;
}

void GraphEdgeItem::updateEndpoints()
{
    refreshPath();
}

void GraphEdgeItem::hoverEnterEvent(
    QGraphicsSceneHoverEvent* event)
{
    hovered_ = true;

    if (isSelected())
    {
        setZValue(20);
    }
    else
    {
        setZValue(10);
    }

    update();

    QGraphicsObject::hoverEnterEvent(event);
}

void GraphEdgeItem::hoverLeaveEvent(
    QGraphicsSceneHoverEvent* event)
{
    hovered_ = false;

    if (isSelected())
    {
        setZValue(20);
    }
    else
    {
        setZValue(-1);
    }

    update();

    QGraphicsObject::hoverLeaveEvent(event);
}

void GraphEdgeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (!model_)
        return;

    EdgeEditorDialog dlg(model_, e_id);
    dlg.exec();

    QGraphicsObject::mouseDoubleClickEvent(event);
}

void GraphEdgeItem::refreshPath()
{
    prepareGeometryChange();

    cachedPath_ =
        buildPath();

    update();
}