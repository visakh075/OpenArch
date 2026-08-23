#include "GraphView.h"
#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"
#include "NodeEditorDialog.h"

#include <QPainter>
#include <QCursor>


#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <algorithm>
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
    cachedRect_ = calculateNodeRect();
}
GraphNodeItem::~GraphNodeItem()
{
    for (const auto& edge : edges_)
    {
        if (edge && edge->scene())
        {
            edge->updateEndpoints();
        }
    }
}

QRectF GraphNodeItem::calculateNodeRect() const
{
    const auto& theme =
        GraphThemeManager::instance()->theme();

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
     * FONTS
     */

    QFont titleFont;

    titleFont.setPointSize(
        State->title.size);

    titleFont.setBold(
        State->title.bold);

    titleFont.setItalic(
        State->title.italic);

    QFont bodyFont;

    bodyFont.setPointSize(
        State->body.size);

    bodyFont.setBold(
        State->body.bold);

    bodyFont.setItalic(
        State->body.italic);

    /*
     * FONT METRICS
     */

    QFontMetrics titleFm(
        titleFont);

    QFontMetrics bodyFm(
        bodyFont);

    /*
     * TEXT MEASURE WIDTH
     */

    const int maxTextWidth = 320;

    /*
     * TEXT BOUNDS
     */

    QRect titleBounds =
        titleFm.boundingRect(
            QRect(
                0,
                0,
                maxTextWidth,
                2000),
            Qt::TextWordWrap,
            displayTitle());

    QRect bodyBounds =
        bodyFm.boundingRect(
            QRect(
                0,
                0,
                maxTextWidth,
                2000),
            Qt::TextWordWrap,
            displayType());

    /*
     * LAYOUT
     */

    const int padding =
        State->padding;

    const int spacing = 0;

    /*
     * NODE SIZE
     */

    qreal width =
        std::max(
            titleBounds.width(),
            bodyBounds.width()) +
        (padding * 2);

    qreal height =
        titleBounds.height() +
        bodyBounds.height() +
        spacing +
        (padding * 2);

    /*
     * MINIMUM SIZE
     */

    width =
        std::max<qreal>(
            width,
            120);

    height =
        std::max<qreal>(
            height,
            50);

    return QRectF(
        0,
        0,
        width,
        height);
}

QRectF GraphNodeItem::boundingRect() const
{
    return cachedRect_;
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

void GraphNodeItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem*,
    QWidget*)
{
    painter->setRenderHint(
        QPainter::Antialiasing);

    /*
     * THEME STATE
     */

    const auto& theme =
        GraphThemeManager::instance()->theme();

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
     * NODE RECT
     */

    QRectF rect =
        cachedRect_;

    /*
     * BACKGROUND
     */

    QPainterPath path;

    path.addRoundedRect(
        rect,
        State->radius,
        State->radius);

    painter->setPen(
        QPen(
            State->border,
            State->borderWidth));

    painter->setBrush(
        State->background);

    painter->drawPath(path);

    /*
     * CONTENT RECT
     */

    const int padding =
        State->padding;

    const int spacing = 0;

    QRectF contentRect =
        rect.adjusted(
            padding,
            padding,
            -padding,
            -padding);

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

    /*
     * FONT METRICS
     */

    QFontMetrics titleFm(
        titleFont);

    QFontMetrics bodyFm(
        bodyFont);

    /*
     * TEXT BOUNDS
     */

    QRect titleBounds =
        titleFm.boundingRect(
            QRect(
                0,
                0,
                static_cast<int>(
                    contentRect.width()),
                2000),
            Qt::TextWordWrap,
            displayTitle());

    QRect bodyBounds =
        bodyFm.boundingRect(
            QRect(
                0,
                0,
                static_cast<int>(
                    contentRect.width()),
                2000),
            Qt::TextWordWrap,
            displayType());

    /*
     * FULL WIDTH TITLE RECT
     */

    QRectF titleRect(
        contentRect.left(),
        contentRect.top(),
        contentRect.width(),
        titleBounds.height());

    /*
     * FULL WIDTH BODY RECT
     */

    QRectF bodyRect(
        contentRect.left(),
        titleRect.bottom() + spacing,
        contentRect.width(),
        bodyBounds.height());

    /*
     * DRAW TITLE
     */

    painter->setFont(
        titleFont);

    painter->setPen(
        State->title.color);

    painter->drawText(
        titleRect,
        State->title.align |
        Qt::TextWordWrap,
        displayTitle());

    /*
     * DRAW BODY
     */

    painter->setFont(
        bodyFont);

    painter->setPen(
        State->body.color);

    painter->drawText(
        bodyRect,
        State->body.align |
        Qt::TextWordWrap,
        displayType());
}


QVariant GraphNodeItem::itemChange(
    QGraphicsItem::GraphicsItemChange change,
    const QVariant& value)
{
    qDebug() << change;
    // if (change == QGraphicsItem::ItemPositionHasChanged)
    if (change == QGraphicsItem::ItemPositionChange || 
        change == QGraphicsItem::ItemPositionHasChanged ||
        change == QGraphicsItem::ItemScenePositionHasChanged)
    {
        for (const auto& e : edges_)
        {
            if (e && e->scene())
                e->updateEndpoints();
        }
    }
    qDebug() << "change exit\n";

    return QGraphicsObject::itemChange(change, value);
}

void GraphNodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *)
{
    hovered_ = true;
    setCursor(Qt::PointingHandCursor);
    refreshGeometry();
    // update();
}

void GraphNodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    hovered_ = false;
    unsetCursor();
    // update();
    refreshGeometry();
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
    if (!scene() || scene()->views().isEmpty())
    return;

    auto* view = dynamic_cast<GraphView*>(scene()->views().first());
    
    if (view && view->mode() == GraphView::Mode::View)
    {
        event->ignore();
        return;
    }

    QGraphicsItem::mousePressEvent(event);
}

void GraphNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (!scene() || scene()->views().isEmpty())
    return;

    auto* view = dynamic_cast<GraphView*>(scene()->views().first());

    if (view && view->mode() == GraphView::Mode::View)
    {
        event->ignore();
        return;
    }

    // Open node editor dialog
    NodeEditorDialog dlg(model_, nodeId_);
    if (dlg.exec() == QDialog::Accepted)
    {
        refreshGeometry(); // Recalculates bounding rect, updates text, and recalculates edge paths
    }

    QGraphicsItem::mouseDoubleClickEvent(event);
}
void GraphNodeItem::addEdge(GraphEdgeItem* edge)
{
    if (!edge)
        return;

    for (const auto& e : edges_)
    {
        if (e == edge)
            return;
    }
    edges_.push_back(edge);
}
void GraphNodeItem::removeEdge(GraphEdgeItem* edge)
{
    edges_.erase(
        std::remove(edges_.begin(), edges_.end(), edge),
        edges_.end());
}

void GraphNodeItem::refreshGeometry()
{
    prepareGeometryChange();

    cachedRect_ = calculateNodeRect();

    update();

    for (const auto& e : edges_)
    {
        if (e)
            e->refreshPath();
    }
}
