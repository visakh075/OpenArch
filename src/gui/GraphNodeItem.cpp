#include "GraphNodeItem.h"

#include <QPainter>
#include <QCursor>

static constexpr qreal NODE_WIDTH  = 140;
static constexpr qreal NODE_HEIGHT = 60;
static constexpr qreal RADIUS = 8;

GraphNodeItem::GraphNodeItem(ArchitectureModel* model,
                             NodeId nodeId,
                             QGraphicsItem* parent)
    : QGraphicsObject(parent),
      model_(model),
      nodeId_(nodeId)
{
    setAcceptHoverEvents(true);
    setToolTip("Node");
}

QRectF GraphNodeItem::boundingRect() const
{
    return QRectF(0, 0, NODE_WIDTH, NODE_HEIGHT);
}

QString GraphNodeItem::displayText() const
{
    auto nodeOpt = model_->getNodeById(nodeId_);
    if (!nodeOpt)
        return "<deleted>";

    return QString::fromStdString(nodeOpt->name) +
           "\n" +
           QString::fromStdString(nodeOpt->type);
}

void GraphNodeItem::paint(QPainter* painter,
                           const QStyleOptionGraphicsItem*,
                           QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    QPen pen(Qt::black, hovered_ ? 2 : 1);
    painter->setPen(pen);
    painter->setBrush(Qt::white);

    painter->drawRoundedRect(
        boundingRect(),
        RADIUS, RADIUS);

    painter->setPen(Qt::black);
    painter->drawText(
        boundingRect().adjusted(6, 6, -6, -6),
        Qt::AlignCenter,
        displayText());
}

void GraphNodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    hovered_ = true;
    setCursor(QCursor(Qt::PointingHandCursor));
    update();
}

void GraphNodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    hovered_ = false;
    unsetCursor();
    update();
}
