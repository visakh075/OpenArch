#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"
#include "gui/GraphView.h"
#include "gui/EditorDialogs/NodeEditorDialog.h"
#include "gui/theme/GraphThemeManager.h"
#include "gui/MainWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QPainter>
#include <QCursor>
#include <QMenu>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <algorithm>
#include <cmath>

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

    setZValue(p != nullptr ? (p->zValue() + 1) : 1);
    refreshGeometry();
}

GraphNodeItem::~GraphNodeItem()
{
    if (tempPathItem_)
    {
        if (scene())
            scene()->removeItem(tempPathItem_);
        delete tempPathItem_;
        tempPathItem_ = nullptr;
    }

    edges_.clear();
}

bool GraphNodeItem::isContainer() const
{
    for (auto* item : childItems())
    {
        if (dynamic_cast<GraphNodeItem*>(item))
            return true;
    }

    if (!model_) return false;
    auto n = model_->getNodeById(nodeId_);
    if (!n) return false;

    QString typeStr = QString::fromStdString(n->type).trimmed().toLower();
    return (typeStr == "container" || typeStr == "group" || 
            typeStr == "box" || typeStr == "package" || typeStr == "cluster");
}

void GraphNodeItem::setContainerSizing(ContainerSizing mode)
{
    if (sizingMode_ != mode)
    {
        sizingMode_ = mode;
        refreshGeometry();
    }
}

void GraphNodeItem::setManualContainerSize(qreal w, qreal h)
{
    manualWidth_ = w;
    manualHeight_ = h;
    if (sizingMode_ == ContainerSizing::Manual)
    {
        refreshGeometry();
    }
}

void GraphNodeItem::adoptChild(GraphNodeItem* child)
{
    if (!child || child == this || child->parentItem() == this)
        return;

    // Prevent circular parenting
    QGraphicsItem* p = this->parentItem();
    while (p)
    {
        if (p == child) return;
        p = p->parentItem();
    }

    QPointF originalScenePos = child->scenePos();
    QRectF childBounds = child->boundingRect();

    QPointF localPos = mapFromScene(originalScenePos);
    qreal minY = cachedHeaderRect_.height() + 10.0;
    qreal minX = 10.0;

    localPos.setX(std::max(minX, localPos.x()));
    localPos.setY(std::max(minY, localPos.y()));

    const qreal margin = 20.0;
    qreal requiredWidth = localPos.x() + childBounds.width() + margin;
    qreal requiredHeight = localPos.y() + childBounds.height() + margin;

    manualWidth_ = std::max(manualWidth_, requiredWidth);
    manualHeight_ = std::max(manualHeight_, requiredHeight);

    child->setParentItem(this);
    child->setPos(localPos);
    child->setZValue(this->zValue() + 1);

    if (model_)
    {
        auto n = model_->getNodeById(child->nodeId());
        if (n)
        {
            n->parentId = nodeId_;
            model_->updateNode(*n);
        }
    }

    refreshGeometry();

    if (scene() && !scene()->views().isEmpty())
    {
        if (auto* mainWin = dynamic_cast<MainWindow*>(scene()->views().first()->window()))
        {
            mainWin->populateNavigator();
        }
    }
}

void GraphNodeItem::releaseChild(GraphNodeItem* child)
{
    if (!child || child->parentItem() != this)
        return;

    QPointF scenePos = child->scenePos();
    child->setParentItem(nullptr);
    child->setPos(scenePos);
    child->setZValue(1);

    if (model_)
    {
        auto n = model_->getNodeById(child->nodeId());
        if (n)
        {
            n->parentId = std::nullopt;
            model_->updateNode(*n);
        }
    }

    refreshGeometry();

    if (scene() && !scene()->views().isEmpty())
    {
        if (auto* mainWin = dynamic_cast<MainWindow*>(scene()->views().first()->window()))
        {
            mainWin->populateNavigator();
        }
    }
}

QRectF GraphNodeItem::calculateNodeRect()
{
    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphNodeState* state = isSelected() ? &theme.node.selected : (hovered_ ? &theme.node.hover : &theme.node.normal);

    cachedTitleFont_ = QFont();
    cachedTitleFont_.setPointSize(state->title.size);
    cachedTitleFont_.setBold(state->title.bold);
    cachedTitleFont_.setItalic(state->title.italic);

    cachedBodyFont_ = QFont();
    cachedBodyFont_.setPointSize(state->body.size);
    cachedBodyFont_.setBold(state->body.bold);
    cachedBodyFont_.setItalic(state->body.italic);

    QFontMetrics titleFm(cachedTitleFont_);
    QFontMetrics bodyFm(cachedBodyFont_);

    const int maxTextWidth = 320;
    QRect titleBounds = titleFm.boundingRect(QRect(0, 0, maxTextWidth, 2000), Qt::TextWordWrap, displayTitle());
    QRect bodyBounds = bodyFm.boundingRect(QRect(0, 0, maxTextWidth, 2000), Qt::TextWordWrap, displayType());

    const int padding = state->padding;
    const int spacing = 0;

    qreal width = std::max(titleBounds.width(), bodyBounds.width()) + (padding * 2);
    qreal height = titleBounds.height() + bodyBounds.height() + spacing + (padding * 2);

    width = std::max<qreal>(width, 120);
    height = std::max<qreal>(height, 50);

    cachedRect_ = QRectF(0, 0, width, height);
    QRectF contentRect = cachedRect_.adjusted(padding, padding, -padding, -padding);

    cachedTitleRect_ = QRectF(contentRect.left(), contentRect.top(), contentRect.width(), titleBounds.height());
    cachedBodyRect_  = QRectF(contentRect.left(), cachedTitleRect_.bottom() + spacing, contentRect.width(), bodyBounds.height());

    return cachedRect_;
}

QRectF GraphNodeItem::calculateContainerRect()
{
    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphNodeState* state = isSelected() ? &theme.node.selected : (hovered_ ? &theme.node.hover : &theme.node.normal);

    cachedTitleFont_ = QFont();
    cachedTitleFont_.setPointSize(state->title.size);
    cachedTitleFont_.setBold(true);

    QFontMetrics fm(cachedTitleFont_);
    qreal headerHeight = fm.height() + (state->padding * 2);

    QRectF childrenUnion;
    bool hasChildren = false;

    for (QGraphicsItem* item : childItems())
    {
        if (!item->isVisible())
            continue;

        childrenUnion = childrenUnion.united(item->mapRectToParent(item->boundingRect()));
        hasChildren = true;
    }

    const qreal padding = 20.0;
    const qreal minW = 220.0;
    const qreal minH = 140.0;

    qreal computedWidth = 0.0;
    qreal computedHeight = 0.0;

    if (sizingMode_ == ContainerSizing::AutoFit)
    {
        if (hasChildren)
        {
            computedWidth  = std::max(minW, childrenUnion.right() + padding);
            computedHeight = std::max(minH, childrenUnion.bottom() + padding);
        }
        else
        {
            computedWidth  = minW;
            computedHeight = minH;
        }
    }
    else // ContainerSizing::Manual
    {
        computedWidth  = manualWidth_;
        computedHeight = manualHeight_;

        if (hasChildren)
        {
            computedWidth  = std::max(computedWidth, childrenUnion.right() + padding);
            computedHeight = std::max(computedHeight, childrenUnion.bottom() + padding);
            manualWidth_ = computedWidth;
            manualHeight_ = computedHeight;
        }
    }

    computedHeight = std::max(computedHeight, headerHeight + minH);

    cachedHeaderRect_ = QRectF(0, 0, computedWidth, headerHeight);
    cachedRect_       = QRectF(0, 0, computedWidth, computedHeight);

    return cachedRect_;
}

QRectF GraphNodeItem::boundingRect() const
{
    const auto& theme = GraphThemeManager::instance()->theme();
    qreal penWidth = theme.node.normal.borderWidth;
    qreal pad = (penWidth / 2.0) + 1.0;
    return cachedRect_.adjusted(-pad, -pad, pad, pad);
}

QPainterPath GraphNodeItem::shape() const
{
    QPainterPath path;
    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphNodeState* state = isSelected() ? &theme.node.selected 
                                : (hovered_ ? &theme.node.hover : &theme.node.normal);

    path.addRoundedRect(cachedRect_, state->radius, state->radius);
    return path;
}

QString GraphNodeItem::displayTitle() const
{
    if (!model_) return "<deleted>";
    auto n = model_->getNodeById(nodeId_);
    if (!n) return "<deleted>";
    return QString::fromStdString(n->name);
}

QString GraphNodeItem::displayType() const
{
    if (!model_) return "<deleted>";
    auto n = model_->getNodeById(nodeId_);
    if (!n) return "<deleted>";
    return QString::fromStdString(n->type);
}

void GraphNodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphNodeState* state = isSelected() ? &theme.node.selected : (hovered_ ? &theme.node.hover : &theme.node.normal);

    if (isContainer())
    {
        QPainterPath bodyPath;
        bodyPath.addRoundedRect(cachedRect_, state->radius, state->radius);

        QColor bodyBg = state->background;
        bodyBg.setAlpha(45);
        painter->setPen(QPen(state->border, state->borderWidth, Qt::DashLine));
        painter->setBrush(bodyBg);
        painter->drawPath(bodyPath);

        QPainterPath headerPath;
        headerPath.addRoundedRect(cachedHeaderRect_, state->radius, state->radius);
        QColor headerBg = state->border;
        headerBg.setAlpha(35);
        painter->setPen(Qt::NoPen);
        painter->setBrush(headerBg);
        painter->drawPath(headerPath);

        painter->setFont(cachedTitleFont_);
        painter->setPen(state->title.color);
        painter->drawText(cachedHeaderRect_.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, displayTitle());
    }
    else
    {
        QPainterPath path;
        path.addRoundedRect(cachedRect_, state->radius, state->radius);

        painter->setPen(QPen(state->border, state->borderWidth));
        painter->setBrush(state->background);
        painter->drawPath(path);

        painter->setFont(cachedTitleFont_);
        painter->setPen(state->title.color);
        painter->drawText(cachedTitleRect_, state->title.align | Qt::TextWordWrap, displayTitle());

        painter->setFont(cachedBodyFont_);
        painter->setPen(state->body.color);
        painter->drawText(cachedBodyRect_, state->body.align | Qt::TextWordWrap, displayType());
    }
}

QVariant GraphNodeItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
    // Clamping inside parent container
    if (change == QGraphicsItem::ItemPositionChange && scene())
    {
        if (auto* parentNode = dynamic_cast<GraphNodeItem*>(parentItem()))
        {
            // Allow dragging outside if Alt key is pressed
            bool altPressed = (QApplication::keyboardModifiers() & Qt::AltModifier);
            if (!altPressed)
            {
                QPointF newPos = value.toPointF();
                qreal minX = 10.0;
                qreal minY = parentNode->cachedHeaderRect_.height() + 10.0;

                newPos.setX(std::max(minX, newPos.x()));
                newPos.setY(std::max(minY, newPos.y()));

                // Clamp to bottom-right in manual mode
                if (parentNode->sizingMode_ == ContainerSizing::Manual)
                {
                    qreal maxX = std::max(minX, parentNode->cachedRect_.width() - boundingRect().width() - 10.0);
                    qreal maxY = std::max(minY, parentNode->cachedRect_.height() - boundingRect().height() - 10.0);
                    newPos.setX(std::min(newPos.x(), maxX));
                    newPos.setY(std::min(newPos.y(), maxY));
                }

                return newPos;
            }
        }
    }

    if (change == QGraphicsItem::ItemPositionHasChanged ||
        change == QGraphicsItem::ItemScenePositionHasChanged)
    {
        for (const auto& e : edges_)
        {
            if (e && e->scene())
                e->updateEndpoints();
        }

        if (auto* parentContainer = dynamic_cast<GraphNodeItem*>(parentItem()))
        {
            parentContainer->refreshGeometry();
        }

        for (auto* child : childItems())
        {
            if (auto* nodeChild = dynamic_cast<GraphNodeItem*>(child))
            {
                for (const auto& ce : nodeChild->edges_)
                {
                    if (ce && ce->scene())
                        ce->updateEndpoints();
                }
            }
        }
    }
    else if (change == QGraphicsItem::ItemSelectedHasChanged)
    {
        refreshGeometry();
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

    if (event->button() == Qt::LeftButton)
    {
        pressScenePos_ = event->scenePos();
        dragStartScenePos_ = event->scenePos();
        isDraggingNode_ = false;
    }

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
        return;
    }

    if (isContainer() && event->button() == Qt::LeftButton)
    {
        if (!cachedHeaderRect_.contains(event->pos()))
        {
            QList<QGraphicsItem*> itemsAtPoint = scene()->items(event->scenePos());
            for (QGraphicsItem* item : itemsAtPoint)
            {
                if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
                {
                    scene()->clearSelection();
                    edge->setSelected(true);
                    event->accept();
                    return;
                }
            }
        }
    }

    QGraphicsObject::mousePressEvent(event);
}

void GraphNodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (isConnecting_ && tempPathItem_)
    {
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
        return;
    }

    if (!isConnecting_ && (event->scenePos() - dragStartScenePos_).manhattanLength() > 4.0)
    {
        isDraggingNode_ = true;
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
        return;
    }

    QGraphicsObject::mouseReleaseEvent(event);

    qreal moveDist = (event->scenePos() - pressScenePos_).manhattanLength();

    if ((isDraggingNode_ || moveDist > 4.0) && scene())
    {
        isDraggingNode_ = false;

        QPointF dropPoint = event->scenePos();
        QRectF mySceneRect = mapRectToScene(boundingRect());

        GraphNodeItem* targetParent = nullptr;
        qreal bestOverlapArea = 0.0;

        for (QGraphicsItem* item : scene()->items(dropPoint))
        {
            auto* candidate = dynamic_cast<GraphNodeItem*>(item);
            if (!candidate || candidate == this)
                continue;

            bool isDescendant = false;
            QGraphicsItem* p = candidate->parentItem();
            while (p)
            {
                if (p == this)
                {
                    isDescendant = true;
                    break;
                }
                p = p->parentItem();
            }

            if (!isDescendant)
            {
                targetParent = candidate;
                break;
            }
        }

        if (!targetParent)
        {
            for (QGraphicsItem* item : scene()->items())
            {
                auto* candidate = dynamic_cast<GraphNodeItem*>(item);
                if (!candidate || candidate == this)
                    continue;

                bool isDescendant = false;
                QGraphicsItem* p = candidate->parentItem();
                while (p)
                {
                    if (p == this)
                    {
                        isDescendant = true;
                        break;
                    }
                    p = p->parentItem();
                }
                if (isDescendant)
                    continue;

                QRectF candRect = candidate->mapRectToScene(candidate->boundingRect());
                QRectF overlap = mySceneRect.intersected(candRect);

                if (overlap.isValid() && !overlap.isEmpty())
                {
                    qreal area = overlap.width() * overlap.height();
                    if (area > bestOverlapArea)
                    {
                        bestOverlapArea = area;
                        targetParent = candidate;
                    }
                }
            }
        }

        // Scenario 1: Dropped over another container
        if (targetParent && parentItem() != targetParent)
        {
            QString nodeName = displayTitle();
            QString parentName = targetParent->displayTitle();

            QWidget* parentWidget = (!scene()->views().isEmpty()) ? scene()->views().first() : nullptr;

            auto reply = QMessageBox::question(
                parentWidget,
                "Add to Container",
                QString("Add '%1' to '%2'?").arg(nodeName, parentName),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);

            if (reply == QMessageBox::Yes)
            {
                targetParent->adoptChild(this);
            }
            else
            {
                // Snap back inside legitimate parent bounds if rejected
                if (auto* curParent = dynamic_cast<GraphNodeItem*>(parentItem()))
                {
                    QPointF lp = pos();
                    qreal minX = 10.0;
                    qreal minY = curParent->cachedHeaderRect_.height() + 10.0;
                    setPos(std::max(minX, lp.x()), std::max(minY, lp.y()));
                }
                refreshGeometry();
            }
        }
        // Scenario 2: Dragged away from current parent container
        else if (!targetParent && parentItem() != nullptr)
        {
            auto* curParent = dynamic_cast<GraphNodeItem*>(parentItem());
            if (curParent)
            {
                QWidget* parentWidget = (!scene()->views().isEmpty()) ? scene()->views().first() : nullptr;

                auto reply = QMessageBox::question(
                    parentWidget,
                    "Remove from Container",
                    QString("Remove '%1' from '%2'?").arg(displayTitle(), curParent->displayTitle()),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes);

                if (reply == QMessageBox::Yes)
                {
                    curParent->releaseChild(this);
                }
                else
                {
                    // Snap back inside parent boundary
                    QPointF lp = pos();
                    qreal minX = 10.0;
                    qreal minY = curParent->cachedHeaderRect_.height() + 10.0;
                    setPos(std::max(minX, lp.x()), std::max(minY, lp.y()));
                    refreshGeometry();
                }
            }
        }
    }

    if (model_)
    {
        auto n = model_->getNodeById(nodeId_);
        if (n)
        {
            model_->updateNode(*n);
        }
    }
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

        if (!scene()->views().isEmpty())
        {
            if (auto* mainWin = dynamic_cast<MainWindow*>(scene()->views().first()->window()))
            {
                mainWin->populateNavigator();
            }
        }
    }
}

void GraphNodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    setSelected(true);

    MainWindow* mainWin = nullptr;
    if (!scene()->views().isEmpty())
    {
        mainWin = dynamic_cast<MainWindow*>(scene()->views().first()->window());
    }

    QMenu menu;
    QAction* cutAct   = menu.addAction(QIcon(":/icons/cut.svg"), "Cut");
    QAction* copyAct  = menu.addAction(QIcon(":/icons/copy.svg"), "Copy");
    QAction* dupAct   = menu.addAction("Duplicate");
    
    QAction* pasteAct = menu.addAction(QIcon(":/icons/paste.svg"), "Paste");
    pasteAct->setEnabled(mainWin && mainWin->hasClipboard());

    menu.addSeparator();
    QAction* editAct  = menu.addAction("Edit Details...");
    QAction* delAct   = menu.addAction("Delete");

    QAction* selected = menu.exec(event->screenPos());
    if (selected == cutAct)
    {
        if (mainWin) mainWin->cutSelectedNodes();
    }
    else if (selected == copyAct)
    {
        if (mainWin) mainWin->copySelectedNodes();
    }
    else if (selected == pasteAct)
    {
        if (mainWin) mainWin->pasteNodesAt(event->scenePos());
    }
    else if (selected == dupAct)
    {
        if (mainWin) mainWin->copySelectedNode();
    }
    else if (selected == editAct)
    {
        NodeEditorDialog dlg(model_, nodeId_);
        if (dlg.exec() == QDialog::Accepted)
        {
            refreshGeometry();

            if (mainWin)
            {
                mainWin->populateNavigator();
            }
        }
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

    if (isContainer())
        calculateContainerRect();
    else
        calculateNodeRect();

    update();

    for (const auto& e : edges_)
    {
        if (e)
            e->refreshPath();
    }

    if (auto* parentContainer = dynamic_cast<GraphNodeItem*>(parentItem()))
    {
        parentContainer->refreshGeometry();
    }
}

QPainterPath GraphNodeItem::buildPreviewPath(const QPointF& targetScenePos, GraphNodeItem* targetNode) const
{
    QPainterPath path;

    QRectF srcRect = mapRectToScene(boundingRect());
    QPointF srcCenter = srcRect.center();

    QRectF dstRect;
    QPointF dstCenter;

    if (targetNode && targetNode != this)
    {
        dstRect = targetNode->mapRectToScene(targetNode->boundingRect());
        dstCenter = dstRect.center();
    }
    else
    {
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

void GraphNodeItem::onThemeChanged()
{
    refreshGeometry();
}