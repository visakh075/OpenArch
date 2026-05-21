#include "GraphView.h"
#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"

#include <QPainter>
#include <QCursor>


#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>

#include "GraphThemeManager.h"
#include <QDebug>

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

    setZValue(1);
}

QRectF GraphNodeItem::boundingRect() const
{
    return {0, 0, NODE_WIDTH, NODE_HEIGHT};
}

QString GraphNodeItem::displayTitle() const
{
    auto n = model_->getNodeById(nodeId_);
    if (!n)
        return "<deleted>";

    return QString::fromStdString(n->name);
}
QString GraphNodeItem::displayType() const
{
    auto n = model_->getNodeById(nodeId_);
    if (!n)
        return "<deleted>";

    return QString::fromStdString(n->type);
}
void GraphNodeItem::paint(QPainter *p,
                          const QStyleOptionGraphicsItem *,
                          QWidget *)
{
    p->setRenderHint(
        QPainter::Antialiasing);

    const auto& theme =
        // GraphThemeManager::theme;
        GraphThemeManager::instance()->theme();

    /*
     * STYLE
     */

    const GraphNodeState* State =
        &theme.node.normal;

    if (isSelected())
    {
        State = &theme.node.selected;
    }
    else if (hovered_)
    {
        State = &theme.node.hover;
    }

    /*
     * PRIMARY NODE OVERRIDE
     */

    QColor borderColor =
        State->border;

    int borderWidth =
        State->borderWidth;

    if (isPrimary_)
    {
        // borderColor =
        //     QColor("#ff4444");

        borderWidth += 1;
    }

    /*
     * PEN
     */

    QPen pen(borderColor);

    pen.setWidth(borderWidth);

    pen.setJoinStyle(
        Qt::RoundJoin);

    p->setPen(pen);

    /*
     * BACKGROUND
     */

    p->setBrush(
        State->background);

    /*
     * NODE RECT
     */

    QRectF rect =
        boundingRect();

    p->drawRoundedRect(
        rect,
        State->radius,
        State->radius);
/*
 * CONTENT LAYOUT
 */

const int padding =
    State->padding;

QRectF contentRect =
    rect.adjusted(
        padding,
        padding,
        -padding,
        -padding);

    /*
    * TITLE AREA
    */

    QRectF titleRect =
        contentRect;

    titleRect.setHeight(
        28);

    /*
    * BODY AREA
    */

    QRectF bodyRect =
        contentRect;

    bodyRect.setTop(
        titleRect.bottom() + 4);

    /*
    * TITLE FONT
    */

    QFont titleFont;

    titleFont.setPointSize(
        State->title.size);

    titleFont.setBold(
        State->title.bold);

    titleFont.setItalic(
        State->title.italic);

    p->setFont(titleFont);

    p->setPen(
        State->title.color);

    /*
    * DRAW TITLE
    */

    p->drawText(
        titleRect,
        Qt::AlignHCenter |
        Qt::AlignTop |
        Qt::TextWordWrap,
        displayTitle());

    /*
    * BODY FONT
    */

    QFont bodyFont;

    bodyFont.setPointSize(
        State->body.size);

    bodyFont.setBold(
        State->body.bold);

    bodyFont.setItalic(
        State->body.italic);

    p->setFont(bodyFont);

    p->setPen(
        State->body.color);

    /*
    * DRAW BODY
    */

    p->drawText(
        bodyRect,
        Qt::AlignTop |
        Qt::AlignHCenter |
        Qt::TextWordWrap,
        displayType());
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
void GraphNodeItem::setEditable(bool enabled)
{
    setFlag(QGraphicsItem::ItemIsMovable, enabled);
    setFlag(QGraphicsItem::ItemIsSelectable, enabled);
}

void GraphNodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    auto* view = dynamic_cast<GraphView*>(scene()->views().first());
    
    if (view && view->mode() == GraphView::Mode::View)
    {
        event->ignore();  // 🔥 allow pan, block selection
        return;
    }

    QGraphicsItem::mousePressEvent(event);
}

void GraphNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    auto* view = dynamic_cast<GraphView*>(scene()->views().first());

    if (view && view->mode() == GraphView::Mode::View)
    {
        event->ignore();
        return;
    }

    QGraphicsItem::mouseDoubleClickEvent(event);
}