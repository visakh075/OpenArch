#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"
#include "GraphView.h"
#include "NodeEditorDialog.h"
#include "GraphThemeManager.h"

#include <QPainter>
#include <QCursor>
#include <QMenu>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <algorithm>

GraphNodeItem::GraphNodeItem(ArchitectureModel *m, NodeId id, QGraphicsItem *p)
    : QGraphicsObject(p),
      model_(m),
      nodeId_(id)
{
    setAcceptHoverEvents(true);
    setFlags(ItemIsMovable |
             ItemIsSelectable |
             ItemSendsGeometryChanges |
             ItemSendsScenePositionChanges);

    setZValue(1);
    cachedRect_ = calculateNodeRect();
}

GraphNodeItem::~GraphNodeItem()
{
    if (tempLine_)
    {
        if (scene())
            scene()->removeItem(tempLine_);
        delete tempLine_;
        tempLine_ = nullptr;
    }

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
    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphNodeState* State = isSelected() ? &theme.node.selected : (hovered_ ? &theme.node.hover : &theme.node.normal);

    QFont titleFont;
    titleFont.setPointSize(State->title.size);
    titleFont.setBold(State->title.bold);
    titleFont.setItalic(State->title.italic);

    QFont bodyFont;
    bodyFont.setPointSize(State->body.size);
    bodyFont.setBold(State->body.bold);
    bodyFont.setItalic(State->body.italic);

    QFontMetrics titleFm(titleFont);
    QFontMetrics bodyFm(bodyFont);

    const int maxTextWidth = 320;
    QRect titleBounds = titleFm.boundingRect(QRect(0, 0, maxTextWidth, 2000), Qt::TextWordWrap, displayTitle());
    QRect bodyBounds = bodyFm.boundingRect(QRect(0, 0, maxTextWidth, 2000), Qt::TextWordWrap, displayType());

    const int padding = State->padding;
    const int spacing = 0;

    qreal width = std::max(titleBounds.width(), bodyBounds.width()) + (padding * 2);
    qreal height = titleBounds.height() + bodyBounds.height() + spacing + (padding * 2);

    width = std::max<qreal>(width, 120);
    height = std::max<qreal>(height, 50);

    return QRectF(0, 0, width, height);
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

void GraphNodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphNodeState* State = isSelected() ? &theme.node.selected : (hovered_ ? &theme.node.hover : &theme.node.normal);

    QRectF rect = cachedRect_;
    QPainterPath path;
    path.addRoundedRect(rect, State->radius, State->radius);

    painter->setPen(QPen(State->border, State->borderWidth));
    painter->setBrush(State->background);
    painter->drawPath(path);

    const int padding = State->padding;
    const int spacing = 0;

    QRectF contentRect = rect.adjusted(padding, padding, -padding, -padding);

    QFont titleFont;
    titleFont.setPointSize(State->title.size);
    titleFont.setBold(State->title.bold);
    titleFont.setItalic(State->title.italic);

    QFont bodyFont;
    bodyFont.setPointSize(State->body.size);
    bodyFont.setBold(State->body.bold);
    bodyFont.setItalic(State->body.italic);

    QFontMetrics titleFm(titleFont);
    QFontMetrics bodyFm(bodyFont);

    QRect titleBounds = titleFm.boundingRect(QRect(0, 0, static_cast<int>(contentRect.width()), 2000), Qt::TextWordWrap, displayTitle());
    QRect bodyBounds = bodyFm.boundingRect(QRect(0, 0, static_cast<int>(contentRect.width()), 2000), Qt::TextWordWrap, displayType());

    QRectF titleRect(contentRect.left(), contentRect.top(), contentRect.width(), titleBounds.height());
    QRectF bodyRect(contentRect.left(), titleRect.bottom() + spacing, contentRect.width(), bodyBounds.height());

    painter->setFont(titleFont);
    painter->setPen(State->title.color);
    painter->drawText(titleRect, State->title.align | Qt::TextWordWrap, displayTitle());

    painter->setFont(bodyFont);
    painter->setPen(State->body.color);
    painter->drawText(bodyRect, State->body.align | Qt::TextWordWrap, displayType());
}

QVariant GraphNodeItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
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

    return QGraphicsObject::itemChange(change, value);
}

void GraphNodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = true;
    setCursor(Qt::PointingHandCursor);
    refreshGeometry();
    QGraphicsObject::hoverEnterEvent(event);
}

void GraphNodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = false;
    unsetCursor();
    refreshGeometry();
    QGraphicsObject::hoverLeaveEvent(event);
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
    update();
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

    // Shift + Left-Click: Start Connection
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier))
    {
        isConnecting_ = true;

        tempPathItem_ = new QGraphicsPathItem();
        QPen p(QColor(0, 180, 216), 2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        tempPathItem_->setPen(p);
        tempPathItem_->setZValue(100);
        tempPathItem_->setPath(buildPreviewPath(event->scenePos()));
        scene()->addItem(tempPathItem_);

        event->accept();
        return; // Suppress base QGraphicsObject::mousePressEvent so the node is NOT dragged
    }

    QGraphicsObject::mousePressEvent(event);
}

void GraphNodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (isConnecting_ && tempPathItem_)
    {
        // Detect hovering over a potential target node for snapping
        GraphNodeItem* targetNode = nullptr;
        for (QGraphicsItem* item : scene()->items(event->scenePos()))
        {
            if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            {
                if (node != this)
                {
                    targetNode = node;
                    break;
                }
            }
        }

        tempPathItem_->setPath(buildPreviewPath(event->scenePos(), targetNode));
        event->accept();
        return; // Suppress base QGraphicsObject::mouseMoveEvent
    }

    QGraphicsObject::mouseMoveEvent(event);
}

void GraphNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (isConnecting_)
    {
        isConnecting_ = false;

        if (tempPathItem_)
        {
            scene()->removeItem(tempPathItem_);
            delete tempPathItem_;
            tempPathItem_ = nullptr;
        }

        // Find drop target node
        for (QGraphicsItem* item : scene()->items(event->scenePos()))
        {
            if (auto* target = dynamic_cast<GraphNodeItem*>(item))
            {
                if (target != this)
                {
                    if (!scene()->views().isEmpty())
                    {
                        if (auto* view = dynamic_cast<GraphView*>(scene()->views().first()))
                        {
                            emit view->requestConnectNodes(nodeId_, target->nodeId());
                        }
                    }
                    break;
                }
            }
        }

        event->accept();
        return; // Suppress base QGraphicsObject::mouseReleaseEvent
    }

    QGraphicsObject::mouseReleaseEvent(event);
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

    event->accept();
    NodeEditorDialog dlg(model_, nodeId_);
    if (dlg.exec() == QDialog::Accepted)
    {
        refreshGeometry();
    }
}

void GraphNodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    setSelected(true);

    QMenu menu;
    QAction* editAct = menu.addAction("Edit");
    QAction* delAct = menu.addAction("Delete");

    QAction* selected = menu.exec(event->screenPos());
    if (selected == editAct)
    {
        NodeEditorDialog dlg(model_, nodeId_);
        if (dlg.exec() == QDialog::Accepted)
            refreshGeometry();
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

QPainterPath GraphNodeItem::buildPreviewPath(const QPointF& targetScenePos, GraphNodeItem* targetNode) const
{
    QPainterPath path;

    QRectF srcRect = mapToScene(boundingRect()).boundingRect();
    QPointF srcCenter = srcRect.center();

    QRectF dstRect;
    QPointF dstCenter;

    if (targetNode && targetNode != this)
    {
        dstRect = targetNode->mapToScene(targetNode->boundingRect()).boundingRect();
        dstCenter = dstRect.center();
    }
    else
    {
        // Treat mouse cursor as a tiny 1x1 rect
        dstRect = QRectF(targetScenePos.x() - 1, targetScenePos.y() - 1, 2, 2);
        dstCenter = targetScenePos;
    }

    double gapRight  = dstRect.left()  - srcRect.right();
    double gapLeft   = srcRect.left()  - dstRect.right();
    double gapBottom = dstRect.top()   - srcRect.bottom();
    double gapTop    = srcRect.top()   - dstRect.bottom();

    QPointF p1, p2;

    if (gapRight > 0)
    {
        p1 = QPointF(srcRect.right(), srcCenter.y());
        p2 = QPointF(dstRect.left(), dstCenter.y());
    }
    else if (gapLeft > 0)
    {
        p1 = QPointF(srcRect.left(), srcCenter.y());
        p2 = QPointF(dstRect.right(), dstCenter.y());
    }
    else if (gapBottom > 0)
    {
        p1 = QPointF(srcCenter.x(), srcRect.bottom());
        p2 = QPointF(dstCenter.x(), dstRect.top());
    }
    else if (gapTop > 0)
    {
        p1 = QPointF(srcCenter.x(), srcRect.top());
        p2 = QPointF(dstCenter.x(), dstRect.bottom());
    }
    else
    {
        double dx = dstCenter.x() - srcCenter.x();
        double dy = dstCenter.y() - srcCenter.y();

        if (std::abs(dx) > std::abs(dy))
        {
            p1 = (dx > 0) ? QPointF(srcRect.right(), srcCenter.y()) : QPointF(srcRect.left(), srcCenter.y());
            p2 = (dx > 0) ? QPointF(dstRect.left(), dstCenter.y())  : QPointF(dstRect.right(), dstCenter.y());
        }
        else
        {
            p1 = (dy > 0) ? QPointF(srcCenter.x(), srcRect.bottom()) : QPointF(srcCenter.x(), srcRect.top());
            p2 = (dy > 0) ? QPointF(dstCenter.x(), dstRect.top())    : QPointF(dstCenter.x(), dstRect.bottom());
        }
    }

    path.moveTo(p1);

    double midX = (p1.x() + p2.x()) / 2.0;
    double midY = (p1.y() + p2.y()) / 2.0;

    bool overlapX = (gapRight <= 0 && gapLeft <= 0);
    bool overlapY = (gapBottom <= 0 && gapTop <= 0);

    if (overlapX && !overlapY)
    {
        path.lineTo(p1.x(), midY);
        path.lineTo(p2.x(), midY);
    }
    else if (overlapY && !overlapX)
    {
        path.lineTo(midX, p1.y());
        path.lineTo(midX, p2.y());
    }
    else
    {
        if (gapRight > 0 || gapLeft > 0)
        {
            path.lineTo(midX, p1.y());
            path.lineTo(midX, p2.y());
        }
        else
        {
            path.lineTo(p1.x(), midY);
            path.lineTo(p2.x(), midY);
        }
    }

    path.lineTo(p2);
    return path;
}