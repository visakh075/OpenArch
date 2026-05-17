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

QString GraphNodeItem::displayText() const
{
    auto n = model_->getNodeById(nodeId_);
    if (!n)
        return "<deleted>";

    return QString::fromStdString(n->name) + "\n" +
           QString::fromStdString(n->type);
}

// void GraphNodeItem::paint(QPainter *p,
//                           const QStyleOptionGraphicsItem *,
//                           QWidget *)
// {
//     p->setRenderHint(QPainter::Antialiasing);

//     // --- Border ---
//     QPen pen;

//     if (isPrimary_)  // 🔴 primary node (anchor)
//     {
//         pen = QPen(Qt::red, 3);
//     }
//     else if (isSelected())
//     {
//         pen = QPen(Qt::blue, 2);
//     }
//     else if (hovered_)
//     {
//         pen = QPen(Qt::darkGray, 2);
//     }
//     else
//     {
//         pen = QPen(Qt::black, 1);
//     }

//     p->setPen(pen);

//     // --- Background ---
//     if (isSelected())
//     {
//         p->setBrush(QColor(220, 235, 255));
//     }
//     else
//     {
//         p->setBrush(Qt::white);
//     }

//     // --- Draw node ---
//     p->drawRoundedRect(boundingRect(), RADIUS, RADIUS);

//     // --- Text ---
//     p->setPen(Qt::black);
//     p->drawText(boundingRect().adjusted(6, 6, -6, -6),
//                 Qt::AlignCenter,
//                 displayText());
// }

void GraphNodeItem::paint(QPainter *p,
                          const QStyleOptionGraphicsItem *,
                          QWidget *)
{
    p->setRenderHint(
        QPainter::Antialiasing);

    const auto& theme =
        GraphThemeManager::theme();

    /*
     * STYLE
     */

    const GraphNodeStyle* style =
        &theme.node.normal;

    if (isSelected())
    {
        style = &theme.node.selected;
    }
    else if (hovered_)
    {
        style = &theme.node.hover;
    }

    /*
     * PRIMARY NODE OVERRIDE
     */

    QColor borderColor =
        style->border;

    int borderWidth =
        style->borderWidth;

    if (isPrimary_)
    {
        borderColor =
            QColor("#ff4444");

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
        style->background);

    /*
     * NODE RECT
     */

    QRectF rect =
        boundingRect();

    p->drawRoundedRect(
        rect,
        style->radius,
        style->radius);

    /*
     * TITLE FONT
     */

    QFont titleFont;

    titleFont.setPointSize(
        style->title.size);

    titleFont.setBold(
        style->title.bold);

    p->setFont(titleFont);

    /*
     * TEXT COLOR
     */

    p->setPen(
        style->title.color);

    /*
     * TEXT RECT
     */

    QRectF textRect =
        rect.adjusted(
            8,
            8,
            -8,
            -8);

    /*
     * DRAW TEXT
     */

    p->drawText(
        textRect,
        Qt::AlignCenter |
        Qt::TextWordWrap,
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