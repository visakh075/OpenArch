#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"

#include <QPainter>
#include <QCursor>

static constexpr qreal NODE_WIDTH  = 140;
static constexpr qreal NODE_HEIGHT = 60;
static constexpr qreal RADIUS      = 8;

GraphNodeItem::GraphNodeItem(ArchitectureModel *m, NodeId id, QGraphicsItem *p)
    : QGraphicsObject(p),
      model_(m),
      nodeId_(id)
{
    setAcceptHoverEvents(true);
    setFlags(ItemIsMovable |
             ItemIsSelectable |
             ItemSendsGeometryChanges);

    setZValue(1); // ensure nodes are above edges
}

QRectF GraphNodeItem::boundingRect() const
{
    return {0, 0, NODE_WIDTH, NODE_HEIGHT};
}

QString GraphNodeItem::displayText() const
{
    auto n = model_->getNodeById(nodeId_);
    if (!n)
        return "<deleted>";

    return QString::fromStdString(n->name) + "\n" +
           QString::fromStdString(n->type);
}

void GraphNodeItem::paint(QPainter *p,
                          const QStyleOptionGraphicsItem *,
                          QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing);

    // --- Border ---
    QPen pen;

    if (isPrimary_)  // 🔴 primary node (anchor)
    {
        pen = QPen(Qt::red, 3);
    }
    else if (isSelected())
    {
        pen = QPen(Qt::blue, 2);
    }
    else if (hovered_)
    {
        pen = QPen(Qt::darkGray, 2);
    }
    else
    {
        pen = QPen(Qt::black, 1);
    }

    p->setPen(pen);

    // --- Background ---
    if (isSelected())
    {
        p->setBrush(QColor(220, 235, 255));
    }
    else
    {
        p->setBrush(Qt::white);
    }

    // --- Draw node ---
    p->drawRoundedRect(boundingRect(), RADIUS, RADIUS);

    // --- Text ---
    p->setPen(Qt::black);
    p->drawText(boundingRect().adjusted(6, 6, -6, -6),
                Qt::AlignCenter,
                displayText());
}

QVariant GraphNodeItem::itemChange(
    QGraphicsItem::GraphicsItemChange change,
    const QVariant& value)
{
    if (change == QGraphicsItem::ItemPositionHasChanged)
    {
        for (auto* e : edges_)
            e->updateEndpoints();
    }

    return QGraphicsObject::itemChange(change, value);
}

void GraphNodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *)
{
    hovered_ = true;
    setCursor(Qt::PointingHandCursor);
    update();
}

void GraphNodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    hovered_ = false;
    unsetCursor();
    update();
}

QPointF GraphNodeItem::currentPosition() const
{
    return pos();
}

QPointF GraphNodeItem::center() const
{
    return mapToScene(boundingRect().center());
}

QRectF GraphNodeItem::rect() const
{
    return boundingRect();
}

void GraphNodeItem::setPrimary(bool p)
{
    if (isPrimary_ == p)
        return;

    isPrimary_ = p;
    update();  // repaint
}

bool GraphNodeItem::isPrimary() const
{
    return isPrimary_;
}