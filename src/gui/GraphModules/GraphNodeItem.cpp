#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"
#include "gui/GraphView.h"
#include "gui/EditorDialogs/NodeEditorDialog.h"
#include "gui/theme/GraphThemeManager.h"
#include "gui/MainWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QInputDialog>
#include <QPainter>
#include <QCursor>
#include <QMenu>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <algorithm>
#include <cmath>

static bool s_showGovernanceBadges = true;

bool GraphNodeItem::showGovernanceBadges()
{
    return s_showGovernanceBadges;
}

void GraphNodeItem::setShowGovernanceBadges(bool show)
{
    s_showGovernanceBadges = show;
}

QColor GraphNodeItem::governanceStatusColor(Status s)
{
    switch (s)
    {
    case Status::New:      return QColor("#88c0d0");
    case Status::Changed:  return QColor("#ebcb8b");
    case Status::Approved: return QColor("#a3be8c");
    case Status::Reviewed: return QColor("#b48ead");
    case Status::Invalid:  return QColor("#bf616a");
    case Status::Deleted:  return QColor("#888888");
    }
    return QColor("#888888");
}

QColor GraphNodeItem::governanceStatusBgColor(Status s)
{
    QColor c = governanceStatusColor(s);
    c.setAlpha(45);
    return c;
}

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

    connect(GraphThemeManager::instance(), &GraphThemeManager::themeChanged,
            this, &GraphNodeItem::onThemeChanged);

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

QRectF GraphNodeItem::foldToggleRect() const
{
    if (!isContainer())
        return QRectF();
    qreal h = cachedHeaderRect_.height();
    qreal iconSize = 14.0;
    return QRectF(cachedHeaderRect_.left() + 8.0,
                  cachedHeaderRect_.top() + (h - iconSize) / 2.0,
                  iconSize, iconSize);
}

GraphNodeItem* GraphNodeItem::effectiveVisibleNode() const
{
    const GraphNodeItem* current = this;
    const GraphNodeItem* highestFolded = nullptr;

    const QGraphicsItem* p = current->parentItem();
    while (p)
    {
        if (auto* parentNode = dynamic_cast<const GraphNodeItem*>(p))
        {
            if (parentNode->isFolded())
            {
                highestFolded = parentNode;
            }
            p = parentNode->parentItem();
        }
        else
        {
            break;
        }
    }

    if (highestFolded)
        return const_cast<GraphNodeItem*>(highestFolded);
    return const_cast<GraphNodeItem*>(this);
}

void GraphNodeItem::updateChildrenVisibility()
{
    for (QGraphicsItem* item : childItems())
    {
        if (auto* childNode = dynamic_cast<GraphNodeItem*>(item))
        {
            if (isFolded_)
            {
                childNode->setVisible(false);
            }
            else
            {
                childNode->setVisible(true);
                if (childNode->isContainer())
                {
                    childNode->updateChildrenVisibility();
                }
            }
        }
    }
}

void GraphNodeItem::updateAllConnectedEdges()
{
    if (scene())
    {
        GraphEdgeItem::updateSceneEdges(scene());
        return;
    }

    for (const auto& e : edges_)
    {
        if (e && e->scene())
            e->updateEndpoints();
    }
    for (auto* child : childItems())
    {
        if (auto* nodeChild = dynamic_cast<GraphNodeItem*>(child))
        {
            nodeChild->updateAllConnectedEdges();
        }
    }
}

void GraphNodeItem::setFolded(bool folded)
{
    if (!isContainer() || isFolded_ == folded)
        return;

    isFolded_ = folded;

    prepareGeometryChange();

    updateChildrenVisibility();
    calculateContainerRect();
    update();

    if (auto* parentContainer = dynamic_cast<GraphNodeItem*>(parentItem()))
    {
        parentContainer->refreshGeometry();
    }

    if (scene())
    {
        GraphEdgeItem::updateSceneEdges(scene());
    }

    if (scene() && !scene()->views().isEmpty())
    {
        if (auto* mainWin = dynamic_cast<MainWindow*>(scene()->views().first()->window()))
        {
            mainWin->populateNavigator();
        }
    }
}

void GraphNodeItem::toggleFold()
{
    setFolded(!isFolded_);
}

void GraphNodeItem::adoptChild(GraphNodeItem* child)
{
    if (!child || child == this || child->parentItem() == this)
        return;

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

    if (isFolded_)
    {
        child->setVisible(false);
    }

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

    if (scene())
    {
        GraphEdgeItem::updateSceneEdges(scene());
    }

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
    child->setVisible(true);

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

    if (scene())
    {
        GraphEdgeItem::updateSceneEdges(scene());
    }

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
    const GraphComponentState* state = isSelected() ? &theme.node.selected : (hovered_ ? &theme.node.hover : &theme.node.normal);

    cachedTitleFont_ = QFont();
    cachedTitleFont_.setPointSize(state->title.size);
    cachedTitleFont_.setBold(state->title.bold);
    cachedTitleFont_.setItalic(state->title.italic);

    cachedBodyFont_ = QFont();
    cachedBodyFont_.setPointSize(state->body.size);
    cachedBodyFont_.setBold(state->body.bold);
    cachedBodyFont_.setItalic(state->body.italic);

    cachedTertiaryFont_ = QFont();
    int tertNodeSize = state->tertiary.size > 0 ? state->tertiary.size : (state->body.size > 2 ? state->body.size - 2 : 9);
    cachedTertiaryFont_.setPointSize(tertNodeSize);
    cachedTertiaryFont_.setBold(state->tertiary.bold);
    cachedTertiaryFont_.setItalic(state->tertiary.italic);

    QFontMetrics titleFm(cachedTitleFont_);
    QFontMetrics bodyFm(cachedBodyFont_);

    const int maxTextWidth = 320;
    QRect titleBounds = titleFm.boundingRect(QRect(0, 0, maxTextWidth, 2000), Qt::TextWordWrap, displayTitle());
    QRect bodyBounds = bodyFm.boundingRect(QRect(0, 0, maxTextWidth, 2000), Qt::TextWordWrap, displayType());

    const int padding = state->padding;
    const int spacing = 0;

    qreal width = std::max(titleBounds.width(), bodyBounds.width()) + (padding * 2);
    qreal height = titleBounds.height() + bodyBounds.height() + spacing + (padding * 2);

    width = std::max<qreal>(width, theme.node.minWidth);
    height = std::max<qreal>(height, theme.node.minHeight);

    cachedRect_ = QRectF(0, 0, width, height);
    QRectF contentRect = cachedRect_.adjusted(padding, padding, -padding, -padding);

    cachedTitleRect_ = QRectF(contentRect.left(), contentRect.top(), contentRect.width(), titleBounds.height());
    cachedBodyRect_  = QRectF(contentRect.left(), cachedTitleRect_.bottom() + spacing, contentRect.width(), bodyBounds.height());
    cachedTertiaryRect_ = QRectF();

    if (s_showGovernanceBadges && model_)
    {
        auto nodeOpt = model_->getNodeById(nodeId_);
        Status status = nodeOpt ? nodeOpt->status : Status::New;
        QString statusText = QString::fromStdString(to_string(status)).toUpper();

        QFont badgeFont;
        badgeFont.setPointSize(7);
        badgeFont.setBold(true);
        QFontMetrics bFm(badgeFont);

        qreal badgeW = std::max(42.0, (qreal)bFm.horizontalAdvance(statusText) + 10.0);
        qreal badgeH = 14.0;

        // Position OUTSIDE the node: floating above the top-right corner
        cachedBadgeRect_ = QRectF(std::max(0.0, cachedRect_.width() - badgeW - 2.0), -badgeH - 3.0, badgeW, badgeH);
    }
    else
    {
        cachedBadgeRect_ = QRectF();
    }

    return cachedRect_;
}

QRectF GraphNodeItem::calculateContainerRect()
{
    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphComponentState* state = isSelected() ? &theme.container.selected : (hovered_ ? &theme.container.hover : &theme.container.normal);
    const GraphComponentState* nodeState = isSelected() ? &theme.node.selected : (hovered_ ? &theme.node.hover : &theme.node.normal);

    cachedTitleFont_ = QFont();
    cachedTitleFont_.setPointSize(state->title.size);
    cachedTitleFont_.setBold(state->title.bold);
    cachedTitleFont_.setItalic(state->title.italic);

    cachedBodyFont_ = QFont();
    int bodySize = state->body.size > 0 ? state->body.size : nodeState->body.size;
    cachedBodyFont_.setPointSize(bodySize > 0 ? bodySize : 11);
    cachedBodyFont_.setBold(state->body.bold);
    cachedBodyFont_.setItalic(state->body.italic);

    cachedTertiaryFont_ = QFont();
    int tertSize = state->tertiary.size > 0 ? state->tertiary.size : (bodySize > 2 ? bodySize - 2 : 9);
    cachedTertiaryFont_.setPointSize(tertSize);
    cachedTertiaryFont_.setBold(state->tertiary.bold);
    cachedTertiaryFont_.setItalic(state->tertiary.italic);

    QFontMetrics fm(cachedTitleFont_);
    qreal headerHeight = fm.height() + state->headerHeightPadding;
    headerHeight = std::max(headerHeight, 28.0);

    QRectF childrenUnion;
    bool hasChildren = false;

    for (QGraphicsItem* item : childItems())
    {
        if (dynamic_cast<GraphNodeItem*>(item))
        {
            childrenUnion = childrenUnion.united(item->mapRectToParent(item->boundingRect()));
            hasChildren = true;
        }
    }

    const qreal padding = 20.0;
    const qreal minW = theme.container.minWidth;
    const qreal minH = theme.container.minHeight;

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
    else
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

    QRectF bodyArea(0, headerHeight, computedWidth, computedHeight - headerHeight);
    if (isFolded_)
    {
        QRectF paddedBody = bodyArea.adjusted(12, 10, -12, -10);
        int childCount = 0;
        for (auto* item : childItems())
        {
            if (dynamic_cast<GraphNodeItem*>(item))
                childCount++;
        }

        QFontMetrics bFm(cachedBodyFont_);
        qreal typeH = bFm.height();

        if (childCount > 0)
        {
            QFontMetrics tFm(cachedTertiaryFont_);
            qreal tertH = tFm.height();
            qreal spacing = 4.0;
            qreal totalH = typeH + spacing + tertH;
            qreal startY = paddedBody.center().y() - (totalH / 2.0);

            cachedBodyRect_ = QRectF(paddedBody.left(), startY, paddedBody.width(), typeH);
            cachedTertiaryRect_ = QRectF(paddedBody.left(), cachedBodyRect_.bottom() + spacing, paddedBody.width(), tertH);
        }
        else
        {
            cachedBodyRect_ = paddedBody;
            cachedTertiaryRect_ = QRectF();
        }
    }
    else
    {
        cachedBodyRect_ = bodyArea;
        cachedTertiaryRect_ = QRectF();
    }

    if (s_showGovernanceBadges && model_)
    {
        auto nodeOpt = model_->getNodeById(nodeId_);
        Status status = nodeOpt ? nodeOpt->status : Status::New;
        QString statusText = QString::fromStdString(to_string(status)).toUpper();

        auto summary = model_->getContainerGovernanceSummary(nodeId_);
        if (summary.totalChildren > 0 && summary.hasIssues)
        {
            statusText += " ⚠";
        }

        QFont badgeFont;
        badgeFont.setPointSize(7);
        badgeFont.setBold(true);
        QFontMetrics bFm(badgeFont);

        qreal badgeW = std::max(42.0, (qreal)bFm.horizontalAdvance(statusText) + 12.0);
        qreal badgeH = 14.0;

        // Position OUTSIDE the container: floating above the top-right corner
        cachedBadgeRect_ = QRectF(std::max(0.0, computedWidth - badgeW - 2.0), -badgeH - 3.0, badgeW, badgeH);
    }
    else
    {
        cachedBadgeRect_ = QRectF();
    }

    return cachedRect_;
}

QRectF GraphNodeItem::boundingRect() const
{
    const auto& theme = GraphThemeManager::instance()->theme();
    const auto* state = isContainer() ? &theme.container.normal : &theme.node.normal;
    qreal penWidth = state->borderWidth;
    qreal pad = (penWidth / 2.0) + 1.0;
    QRectF r = cachedRect_.adjusted(-pad, -pad, pad, pad);
    if (s_showGovernanceBadges && !cachedBadgeRect_.isEmpty())
    {
        r = r.united(cachedBadgeRect_.adjusted(-pad, -pad, pad, pad));
    }
    return r;
}

QPainterPath GraphNodeItem::shape() const
{
    QPainterPath path;
    const auto& theme = GraphThemeManager::instance()->theme();
    const auto* state = isContainer() 
        ? (isSelected() ? &theme.container.selected : (hovered_ ? &theme.container.hover : &theme.container.normal))
        : (isSelected() ? &theme.node.selected : (hovered_ ? &theme.node.hover : &theme.node.normal));

    path.addRoundedRect(cachedRect_, state->radius, state->radius);
    if (s_showGovernanceBadges && !cachedBadgeRect_.isEmpty())
    {
        path.addRoundedRect(cachedBadgeRect_, 3.0, 3.0);
    }
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

QString GraphNodeItem::tertiaryText() const
{
    if (isContainer() && isFolded_)
    {
        int childCount = 0;
        for (auto* item : childItems())
        {
            if (dynamic_cast<GraphNodeItem*>(item))
                childCount++;
        }
        if (childCount > 0)
            return QString("(%1 %2 hidden)").arg(childCount).arg(childCount == 1 ? "component" : "components");
    }
    return QString();
}

void GraphNodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    const auto& theme = GraphThemeManager::instance()->theme();

    if (isContainer())
    {
        const GraphComponentState* state = isSelected() ? &theme.container.selected 
                                        : (hovered_ ? &theme.container.hover : &theme.container.normal);

        QPainterPath bodyPath;
        bodyPath.addRoundedRect(cachedRect_, state->radius, state->radius);

        QPen containerPen(state->border, state->borderWidth, state->borderStyle);
        if (state->borderStyle == Qt::CustomDashLine && !state->dashPattern.isEmpty())
        {
            containerPen.setDashPattern(state->dashPattern);
        }

        painter->setPen(containerPen);
        painter->setBrush(state->background);
        painter->drawPath(bodyPath);

        QPainterPath headerPath;
        headerPath.addRoundedRect(cachedHeaderRect_, state->radius, state->radius);

        painter->setPen(Qt::NoPen);
        painter->setBrush(state->headerBackground);
        painter->drawPath(headerPath);

        painter->setFont(cachedTitleFont_);
        painter->setPen(state->title.color);

        // Fold toggle icon
        QRectF toggleRect = foldToggleRect();
        QPainterPath triPath;
        if (isFolded_)
        {
            // Right-pointing triangle (Folded / Collapsed): ▶
            qreal left = toggleRect.left() + 4.0;
            qreal right = toggleRect.right() - 3.0;
            qreal top = toggleRect.top() + 3.0;
            qreal bottom = toggleRect.bottom() - 3.0;
            qreal midY = toggleRect.center().y();
            triPath.moveTo(left, top);
            triPath.lineTo(right, midY);
            triPath.lineTo(left, bottom);
            triPath.closeSubpath();
        }
        else
        {
            // Down-pointing triangle (Expanded / Unfolded): ▼
            qreal left = toggleRect.left() + 3.0;
            qreal right = toggleRect.right() - 3.0;
            qreal top = toggleRect.top() + 4.0;
            qreal bottom = toggleRect.bottom() - 3.0;
            qreal midX = toggleRect.center().x();
            triPath.moveTo(left, top);
            triPath.lineTo(right, top);
            triPath.lineTo(midX, bottom);
            triPath.closeSubpath();
        }

        painter->setPen(Qt::NoPen);
        painter->setBrush(state->title.color);
        painter->drawPath(triPath);

        painter->setPen(state->title.color);
        QRectF titleTextRect = cachedHeaderRect_.adjusted(26, 0, -10, 0);
        painter->drawText(titleTextRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, displayTitle());

        if (isFolded_)
        {
            const auto* nodeState = isSelected() ? &theme.node.selected
                                                 : (hovered_ ? &theme.node.hover : &theme.node.normal);
            QColor bodyColor = (state->body.color.isValid() && state->body.color.alpha() > 0)
                             ? state->body.color
                             : nodeState->body.color;

            QColor tertColor = (state->tertiary.color.isValid() && state->tertiary.color.alpha() > 0)
                             ? state->tertiary.color
                             : (nodeState->tertiary.color.isValid() && nodeState->tertiary.color.alpha() > 0
                                 ? nodeState->tertiary.color
                                 : QColor(bodyColor.red(), bodyColor.green(), bodyColor.blue(), std::min(160, bodyColor.alpha())));

            QString typeText = displayType();
            int childCount = 0;
            for (auto* item : childItems())
            {
                if (dynamic_cast<GraphNodeItem*>(item))
                    childCount++;
            }

            Qt::Alignment bodyAlign = state->body.align;
            if (!(bodyAlign & (Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter)))
            {
                bodyAlign |= Qt::AlignVCenter;
            }

            Qt::Alignment tertAlign = state->tertiary.align;
            if (!(tertAlign & (Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter)))
            {
                tertAlign |= Qt::AlignVCenter;
            }

            painter->setFont(cachedBodyFont_);
            painter->setPen(bodyColor);
            painter->drawText(cachedBodyRect_, bodyAlign | Qt::TextWordWrap, typeText);

            if (childCount > 0 && !cachedTertiaryRect_.isEmpty())
            {
                painter->setFont(cachedTertiaryFont_);
                painter->setPen(tertColor);
                QString noteText = QString("(%1 %2 hidden)").arg(childCount).arg(childCount == 1 ? "component" : "components");
                painter->drawText(cachedTertiaryRect_, tertAlign | Qt::TextWordWrap, noteText);
            }
        }
    }
    else
    {
        const GraphComponentState* state = isSelected() ? &theme.node.selected 
                                        : (hovered_ ? &theme.node.hover : &theme.node.normal);

        QPainterPath path;
        path.addRoundedRect(cachedRect_, state->radius, state->radius);

        QPen nodePen(state->border, state->borderWidth, state->borderStyle);
        if (state->borderStyle == Qt::CustomDashLine && !state->dashPattern.isEmpty())
        {
            nodePen.setDashPattern(state->dashPattern);
        }

        painter->setPen(nodePen);
        painter->setBrush(state->background);
        painter->drawPath(path);

        painter->setFont(cachedTitleFont_);
        painter->setPen(state->title.color);
        painter->drawText(cachedTitleRect_, state->title.align | Qt::TextWordWrap, displayTitle());

        painter->setFont(cachedBodyFont_);
        painter->setPen(state->body.color);
        painter->drawText(cachedBodyRect_, state->body.align | Qt::TextWordWrap, displayType());
    }

    // Governance Status Badge (outside the node)
    if (s_showGovernanceBadges && !cachedBadgeRect_.isEmpty() && model_)
    {
        auto nodeOpt = model_->getNodeById(nodeId_);
        if (nodeOpt)
        {
            Status status = nodeOpt->status;
            bool isContainerWithChildren = false;
            bool hasChildIssues = false;
            ArchitectureModel::ContainerGovernanceSummary govSummary;

            if (isContainer())
            {
                govSummary = model_->getContainerGovernanceSummary(nodeId_);
                if (govSummary.totalChildren > 0)
                {
                    isContainerWithChildren = true;
                    hasChildIssues = govSummary.hasIssues;
                }
            }

            QColor badgeColor = governanceStatusColor(status);
            QColor badgeBg    = governanceStatusBgColor(status);

            if (isContainerWithChildren && hasChildIssues)
            {
                badgeColor = governanceStatusColor(govSummary.rollupStatus);
                badgeBg    = governanceStatusBgColor(govSummary.rollupStatus);
            }

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);

            QPainterPath badgePath;
            badgePath.addRoundedRect(cachedBadgeRect_, 3.0, 3.0);

            // Dark base background for crisp contrast against any canvas background
            painter->fillPath(badgePath, QColor(36, 40, 52, 230));
            painter->fillPath(badgePath, badgeBg);

            QPen badgePen(badgeColor, 1.0);
            painter->setPen(badgePen);
            painter->drawPath(badgePath);

            QFont badgeFont;
            badgeFont.setPointSize(7);
            badgeFont.setBold(true);
            painter->setFont(badgeFont);
            painter->setPen(badgeColor);

            QString statusText = QString::fromStdString(to_string(status)).toUpper();
            if (isContainerWithChildren && hasChildIssues)
            {
                statusText += " ⚠";
            }
            painter->drawText(cachedBadgeRect_, Qt::AlignCenter, statusText);
            painter->restore();
        }
    }
}

QVariant GraphNodeItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
    if (change == QGraphicsItem::ItemPositionChange && scene())
    {
        if (auto* parentNode = dynamic_cast<GraphNodeItem*>(parentItem()))
        {
            bool altPressed = (QApplication::keyboardModifiers() & Qt::AltModifier);
            if (!altPressed)
            {
                QPointF newPos = value.toPointF();
                qreal minX = 10.0;
                qreal badgeAllowance = s_showGovernanceBadges ? 24.0 : 10.0;
                qreal minY = parentNode->cachedHeaderRect_.height() + badgeAllowance;

                newPos.setX(std::max(minX, newPos.x()));
                newPos.setY(std::max(minY, newPos.y()));

                if (parentNode->sizingMode_ == ContainerSizing::Manual)
                {
                    qreal maxX = std::max(minX, parentNode->cachedRect_.width() - nodeRect().width() - 10.0);
                    qreal maxY = std::max(minY, parentNode->cachedRect_.height() - nodeRect().height() - 10.0);
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
        if (scene())
        {
            GraphEdgeItem::updateSceneEdges(scene());
        }
        else
        {
            updateAllConnectedEdges();
        }

        if (auto* parentContainer = dynamic_cast<GraphNodeItem*>(parentItem()))
        {
            parentContainer->refreshGeometry();
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

        const auto& previewTheme = GraphThemeManager::instance()->theme().edge.preview;

        tempPathItem_ = new QGraphicsPathItem();
        QPen p(previewTheme.color, previewTheme.width, previewTheme.style, Qt::RoundCap, Qt::RoundJoin);
        tempPathItem_->setPen(p);
        tempPathItem_->setZValue(100);
        tempPathItem_->setPath(buildPreviewPath(event->scenePos()));
        scene()->addItem(tempPathItem_);

        event->accept();
        return;
    }

    if (isContainer() && event->button() == Qt::LeftButton)
    {
        if (foldToggleRect().adjusted(-4, -4, 4, 4).contains(event->pos()))
        {
            toggleFold();
            event->accept();
            return;
        }

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

            if (!isDescendant && candidate->isContainer())
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
                if (!candidate || candidate == this || !candidate->isContainer())
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

    if (isContainer())
    {
        if (isFolded_ || cachedHeaderRect_.contains(event->pos()))
        {
            toggleFold();
            event->accept();
            return;
        }
    }

    event->accept();
    NodeEditorDialog dlg(model_, nodeId_);
    if (dlg.exec() == QDialog::Accepted)
    {
        if (scene())
        {
            for (auto* it : scene()->items())
            {
                if (auto* ni = dynamic_cast<GraphNodeItem*>(it))
                {
                    ni->refreshGeometry();
                    ni->update();
                }
                else if (auto* ei = dynamic_cast<GraphEdgeItem*>(it))
                {
                    ei->refreshLayout();
                    ei->update();
                }
            }
        }

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
    QAction* foldAct  = nullptr;
    if (isContainer())
    {
        foldAct = menu.addAction(isFolded_ ? "Expand Container" : "Fold Container");
        menu.addSeparator();
    }

    QAction* cutAct   = menu.addAction(QIcon(":/icons/cut.svg"), "Cut");
    QAction* copyAct  = menu.addAction(QIcon(":/icons/copy.svg"), "Copy");
    QAction* dupAct   = menu.addAction("Duplicate");
    
    QAction* pasteAct = menu.addAction(QIcon(":/icons/paste.svg"), "Paste");
    pasteAct->setEnabled(mainWin && mainWin->hasClipboard());

    menu.addSeparator();
    auto* govMenu = menu.addMenu("Governance");
    QAction* reviewAct  = govMenu->addAction("Mark as Reviewed...");
    QAction* approveAct = govMenu->addAction("Mark as Approved...");
    QAction* changeAct  = govMenu->addAction("Mark as Changed");
    QAction* invalidAct = govMenu->addAction("Mark as Invalid");

    QAction* cascadeApproveAct = nullptr;
    QAction* cascadeReviewAct  = nullptr;
    if (model_)
    {
        auto s = model_->getContainerGovernanceSummary(nodeId_);
        if (s.totalChildren > 0)
        {
            govMenu->addSeparator();
            cascadeApproveAct = govMenu->addAction(QString("Cascade Approve (%1 Children, %2 Edges)...").arg(s.totalChildren).arg(s.totalInternalEdges));
            cascadeReviewAct  = govMenu->addAction(QString("Cascade Review (%1 Children, %2 Edges)...").arg(s.totalChildren).arg(s.totalInternalEdges));
        }
    }

    govMenu->addSeparator();
    QAction* verifyAct  = govMenu->addAction("Verify Checksum Integrity");

    menu.addSeparator();
    QAction* editAct  = menu.addAction("Edit Details...");
    QAction* delAct   = menu.addAction("Delete");

    QAction* selected = menu.exec(event->screenPos());
    if (selected == foldAct)
    {
        toggleFold();
    }
    else if (selected == cutAct)
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
    else if (selected == reviewAct || selected == approveAct)
    {
        bool isApprove = (selected == approveAct);
        QString defUser = QString::fromLocal8Bit(qgetenv("USER"));
        if (defUser.isEmpty()) defUser = QString::fromLocal8Bit(qgetenv("USERNAME"));
        if (defUser.isEmpty()) defUser = "architect";

        bool ok = false;
        QWidget* parentWidget = (!scene()->views().isEmpty()) ? scene()->views().first() : nullptr;
        QString reviewer = QInputDialog::getText(
            parentWidget,
            isApprove ? "Approve Node(s)" : "Review Node(s)",
            "Enter Reviewer ID / Name:",
            QLineEdit::Normal,
            defUser,
            &ok);

        if (ok && !reviewer.trimmed().isEmpty())
        {
            Status newStatus = isApprove ? Status::Approved : Status::Reviewed;
            std::string revStr = reviewer.trimmed().toStdString();

            for (auto* item : scene()->selectedItems())
            {
                if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
                {
                    bool didCascade = false;
                    auto s = model_->getContainerGovernanceSummary(nodeItem->nodeId());
                    if (s.totalChildren > 0)
                    {
                        auto nOpt = model_->getNodeById(nodeItem->nodeId());
                        QString name = nOpt ? QString::fromStdString(nOpt->name) : QString("Node #%1").arg(nodeItem->nodeId());
                        auto reply = QMessageBox::question(
                            parentWidget,
                            "Cascade to Child Components?",
                            QString("Container '%1' has %2 child node(s) and %3 internal edge(s).\n\n"
                                    "Do you want to cascade this %4 to all child components?")
                                .arg(name)
                                .arg(s.totalChildren)
                                .arg(s.totalInternalEdges)
                                .arg(isApprove ? "approval" : "review"),
                            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                            QMessageBox::Yes);

                        if (reply == QMessageBox::Cancel)
                            continue;

                        if (reply == QMessageBox::Yes)
                        {
                            model_->cascadeNodeGovernance(nodeItem->nodeId(), newStatus, revStr);
                            didCascade = true;
                        }
                    }

                    if (!didCascade)
                    {
                        model_->setNodeGovernance(nodeItem->nodeId(), newStatus, revStr);
                    }
                }
            }

            for (auto* item : scene()->items())
            {
                if (auto* ni = dynamic_cast<GraphNodeItem*>(item))
                {
                    ni->refreshGeometry();
                    ni->update();
                }
                else if (auto* ei = dynamic_cast<GraphEdgeItem*>(item))
                {
                    ei->refreshLayout();
                    ei->update();
                }
            }

            if (mainWin)
                mainWin->populateNavigator();
        }
    }
    else if (selected == cascadeApproveAct || selected == cascadeReviewAct)
    {
        bool isApprove = (selected == cascadeApproveAct);
        QString defUser = QString::fromLocal8Bit(qgetenv("USER"));
        if (defUser.isEmpty()) defUser = QString::fromLocal8Bit(qgetenv("USERNAME"));
        if (defUser.isEmpty()) defUser = "architect";

        bool ok = false;
        QWidget* parentWidget = (!scene()->views().isEmpty()) ? scene()->views().first() : nullptr;
        QString reviewer = QInputDialog::getText(
            parentWidget,
            isApprove ? "Cascade Approve Container" : "Cascade Review Container",
            "Enter Reviewer ID / Name:",
            QLineEdit::Normal,
            defUser,
            &ok);

        if (ok && !reviewer.trimmed().isEmpty())
        {
            Status newStatus = isApprove ? Status::Approved : Status::Reviewed;
            model_->cascadeNodeGovernance(nodeId_, newStatus, reviewer.trimmed().toStdString());

            for (auto* item : scene()->items())
            {
                if (auto* ni = dynamic_cast<GraphNodeItem*>(item))
                {
                    ni->refreshGeometry();
                    ni->update();
                }
                else if (auto* ei = dynamic_cast<GraphEdgeItem*>(item))
                {
                    ei->refreshLayout();
                    ei->update();
                }
            }

            if (mainWin)
                mainWin->populateNavigator();
        }
    }
    else if (selected == changeAct)
    {
        for (auto* item : scene()->selectedItems())
        {
            if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
            {
                model_->setNodeGovernance(nodeItem->nodeId(), Status::Changed, "");
            }
        }
        for (auto* it : scene()->items())
        {
            if (auto* ni = dynamic_cast<GraphNodeItem*>(it))
            {
                ni->refreshGeometry();
                ni->update();
            }
        }
        if (mainWin)
            mainWin->populateNavigator();
    }
    else if (selected == invalidAct)
    {
        for (auto* item : scene()->selectedItems())
        {
            if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
            {
                model_->setNodeGovernance(nodeItem->nodeId(), Status::Invalid, "");
            }
        }
        for (auto* it : scene()->items())
        {
            if (auto* ni = dynamic_cast<GraphNodeItem*>(it))
            {
                ni->refreshGeometry();
                ni->update();
            }
        }
        if (mainWin)
            mainWin->populateNavigator();
    }
    else if (selected == verifyAct)
    {
        auto nodeOpt = model_->getNodeById(nodeId_);
        if (nodeOpt)
        {
            uint32_t expected = model_->computeNodeChecksum(*nodeOpt);
            bool match = (expected == nodeOpt->checksum);

            QString msg = QString("<b>Node:</b> %1 (ID %2)<br>"
                                  "<b>Status:</b> %3<br>"
                                  "<b>Reviewer:</b> %4<br><br>"
                                  "<b>Stored Checksum:</b> 0x%5<br>"
                                  "<b>Calculated Checksum:</b> 0x%6<br><br>"
                                  "<b>Integrity:</b> %7")
                .arg(QString::fromStdString(nodeOpt->name).toHtmlEscaped())
                .arg(nodeId_)
                .arg(QString::fromStdString(to_string(nodeOpt->status)).toUpper())
                .arg(nodeOpt->reviewer.empty() ? "<i>(None)</i>" : QString::fromStdString(nodeOpt->reviewer).toHtmlEscaped())
                .arg(QString::number(nodeOpt->checksum, 16).toUpper())
                .arg(QString::number(expected, 16).toUpper())
                .arg(match ? "<span style='color:#a3be8c;font-weight:bold;'>✓ Valid (Tamper-Free)</span>"
                           : "<span style='color:#bf616a;font-weight:bold;'>⚠ CHECKSUM MISMATCH (TAMPER DETECTED)</span>");

            auto s = model_->getContainerGovernanceSummary(nodeId_);
            if (s.totalChildren > 0)
            {
                msg += QString("<hr><b>Child Components Summary:</b><br>"
                               "• Child Nodes (%1): %2 approved, %3 changed, %4 new, %5 invalid<br>"
                               "• Internal Edges (%6): %7 approved, %8 changed, %9 new, %10 invalid<br>"
                               "<b>Rollup Status:</b> %11")
                    .arg(s.totalChildren).arg(s.approvedChildren).arg(s.changedChildren).arg(s.newChildren).arg(s.invalidChildren)
                    .arg(s.totalInternalEdges).arg(s.approvedEdges).arg(s.changedEdges).arg(s.newEdges).arg(s.invalidEdges)
                    .arg(QString::fromStdString(to_string(s.rollupStatus)).toUpper());
            }

            QWidget* parentWidget = (!scene()->views().isEmpty()) ? scene()->views().first() : nullptr;
            QMessageBox::information(
                parentWidget,
                "Governance Integrity Verification",
                msg);
        }
    }
    else if (selected == editAct)
    {
        NodeEditorDialog dlg(model_, nodeId_);
        if (dlg.exec() == QDialog::Accepted)
        {
            for (auto* it : scene()->items())
            {
                if (auto* ni = dynamic_cast<GraphNodeItem*>(it))
                {
                    ni->refreshGeometry();
                    ni->update();
                }
                else if (auto* ei = dynamic_cast<GraphEdgeItem*>(it))
                {
                    ei->refreshLayout();
                    ei->update();
                }
            }

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

    if (model_)
    {
        auto n = model_->getNodeById(nodeId_);
        if (n)
        {
            QString tip = QString("<b>%1</b> (%2)<br>"
                                  "Status: <b>%3</b><br>"
                                  "Reviewer: %4<br>"
                                  "Checksum: 0x%5")
                .arg(QString::fromStdString(n->name).toHtmlEscaped())
                .arg(QString::fromStdString(n->type).toHtmlEscaped())
                .arg(QString::fromStdString(to_string(n->status)).toUpper())
                .arg(n->reviewer.empty() ? "<i>(None)</i>" : QString::fromStdString(n->reviewer).toHtmlEscaped())
                .arg(QString::number(n->checksum, 16).toUpper());

            if (isContainer())
            {
                auto s = model_->getContainerGovernanceSummary(nodeId_);
                if (s.totalChildren > 0)
                {
                    tip += QString("<hr><b>Hierarchy Rollup Status:</b> %1<br>"
                                   "• Child Nodes (%2): %3 approved, %4 changed, %5 new, %6 invalid<br>"
                                   "• Internal Edges (%7): %8 approved, %9 changed, %10 new")
                        .arg(QString::fromStdString(to_string(s.rollupStatus)).toUpper())
                        .arg(s.totalChildren).arg(s.approvedChildren).arg(s.changedChildren).arg(s.newChildren).arg(s.invalidChildren)
                        .arg(s.totalInternalEdges).arg(s.approvedEdges).arg(s.changedEdges).arg(s.newEdges);
                    if (s.hasIssues)
                    {
                        tip += "<br><span style='color:#ebcb8b;'>⚠ Child components require review / approval</span>";
                    }
                }
            }

            setToolTip(tip);
        }
    }

    update();

    if (scene())
    {
        GraphEdgeItem::updateSceneEdges(scene());
    }
    else
    {
        for (const auto& e : edges_)
        {
            if (e)
                e->refreshPath();
        }
    }

    if (auto* parentContainer = dynamic_cast<GraphNodeItem*>(parentItem()))
    {
        parentContainer->refreshGeometry();
    }
}

QPainterPath GraphNodeItem::buildPreviewPath(const QPointF& targetScenePos, GraphNodeItem* targetNode) const
{
    QPainterPath path;

    QRectF srcRect = mapRectToScene(nodeRect());
    QPointF srcCenter = srcRect.center();

    QRectF dstRect;
    QPointF dstCenter;

    if (targetNode && targetNode != this)
    {
        dstRect = targetNode->mapRectToScene(targetNode->nodeRect());
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
    prepareGeometryChange();

    if (isContainer())
        calculateContainerRect();
    else
        calculateNodeRect();

    update();

    for (QGraphicsItem* child : childItems())
    {
        if (auto* nodeChild = dynamic_cast<GraphNodeItem*>(child))
            nodeChild->onThemeChanged();
    }

    for (const auto& e : edges_)
    {
        if (e)
            e->refreshLayout();
    }
}