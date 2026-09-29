#include "GraphEdgeItem.h"
#include "GraphNodeItem.h"
#include "gui/GraphView.h"
#include "gui/EditorDialogs/EdgeEditorDialog.h"
#include "gui/theme/GraphThemeManager.h"
#include "gui/MainWindow.h"

#include <QPainter>
#include <QString>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainterPathStroker>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <map>

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
    setFlag(QGraphicsItem::ItemIsMovable, false);

    qreal baseZ = 1.5;
    if (src_ && dst_)
    {
        baseZ = std::max(src_->zValue(), dst_->zValue()) - 0.1;
    }
    setZValue(baseZ);

    e_id = id;

    if (src_)
        src_->addEdge(this);
    if (dst_)
        dst_->addEdge(this);

    normalPen_ = QPen(Qt::darkGray, 2);
    highlightPen_ = QPen(Qt::blue, 3);
    cachedPath_ = buildPath();

    connect(GraphThemeManager::instance(), &GraphThemeManager::themeChanged,
            this, &GraphEdgeItem::onThemeChanged);

    refreshLayout();
}

GraphEdgeItem::~GraphEdgeItem()
{
    if (src_)
        src_->removeEdge(this);
    if (dst_)
        dst_->removeEdge(this);
}

void GraphEdgeItem::onThemeChanged()
{
    prepareGeometryChange();
    refreshLayout();
    update();
}

GraphNodeItem* GraphEdgeItem::effectiveSrcNode() const
{
    return src_ ? src_->effectiveVisibleNode() : nullptr;
}

GraphNodeItem* GraphEdgeItem::effectiveDstNode() const
{
    return dst_ ? dst_->effectiveVisibleNode() : nullptr;
}

void GraphEdgeItem::setConsolidatedEdges(const std::vector<GraphEdgeItem*>& edges)
{
    consolidatedEdgeIds_.clear();
    consolidatedTypes_.clear();

    if (edges.size() <= 1)
    {
        isConsolidated_ = false;
        refreshLayout();
        return;
    }

    isConsolidated_ = true;
    bool hasInvalid = false;
    bool hasChanged = false;
    bool hasNew = false;
    bool allApproved = true;
    bool allReviewed = true;

    for (auto* e : edges)
    {
        if (!e) continue;
        consolidatedEdgeIds_.push_back(e->edgeId());

        if (e->model())
        {
            auto edgeOpt = e->model()->getEdgeById(e->edgeId());
            if (edgeOpt)
            {
                QString t = QString::fromStdString(edgeOpt->edgeType).trimmed();
                if (!t.isEmpty() && !consolidatedTypes_.contains(t))
                {
                    consolidatedTypes_.append(t);
                }

                Status s = edgeOpt->status;
                if (s == Status::Invalid) hasInvalid = true;
                if (s == Status::Changed) hasChanged = true;
                if (s == Status::New) hasNew = true;
                if (s != Status::Approved) allApproved = false;
                if (s != Status::Reviewed) allReviewed = false;
            }
        }
    }

    if (hasInvalid)
        consolidatedStatus_ = Status::Invalid;
    else if (hasChanged)
        consolidatedStatus_ = Status::Changed;
    else if (hasNew)
        consolidatedStatus_ = Status::New;
    else if (allApproved)
        consolidatedStatus_ = Status::Approved;
    else if (allReviewed)
        consolidatedStatus_ = Status::Reviewed;
    else
        consolidatedStatus_ = Status::New;

    consolidatedHasIssues_ = (hasInvalid || hasChanged);

    refreshLayout();
}

void GraphEdgeItem::updateSceneEdges(QGraphicsScene* scene)
{
    if (!scene)
        return;

    std::vector<GraphEdgeItem*> allEdges;
    for (QGraphicsItem* item : scene->items())
    {
        if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
        {
            allEdges.push_back(edge);
        }
    }

    std::map<std::pair<GraphNodeItem*, GraphNodeItem*>, std::vector<GraphEdgeItem*>> groups;

    for (auto* edge : allEdges)
    {
        GraphNodeItem* s = edge->effectiveSrcNode();
        GraphNodeItem* d = edge->effectiveDstNode();

        if (!s || !d || s == d)
        {
            edge->setConsolidatedEdges({});
            edge->setVisible(false);
            continue;
        }

        groups[{s, d}].push_back(edge);
    }

    for (auto& pair : groups)
    {
        auto& edgeList = pair.second;
        if (edgeList.empty())
            continue;

        GraphEdgeItem* primary = edgeList.front();
        primary->setConsolidatedEdges(edgeList);
        primary->setVisible(true);
        primary->updateEndpoints();

        for (size_t i = 1; i < edgeList.size(); ++i)
        {
            edgeList[i]->setConsolidatedEdges({});
            edgeList[i]->setVisible(false);
        }
    }
}

QPointF GraphEdgeItem::portScenePosition(GraphNodeItem* node, Port port) const
{
    if (!node)
        return QPointF();

    QRectF rect = node->mapToScene(node->nodeRect()).boundingRect();
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

void GraphEdgeItem::autoSelectPorts(Port& srcPort, Port& dstPort) const
{
    GraphNodeItem* sNode = effectiveSrcNode();
    GraphNodeItem* dNode = effectiveDstNode();
    if (!sNode || !dNode)
        return;

    QRectF srcRect = sNode->mapToScene(sNode->nodeRect()).boundingRect();
    QRectF dstRect = dNode->mapToScene(dNode->nodeRect()).boundingRect();

    QPointF srcCenter = srcRect.center();
    QPointF dstCenter = dstRect.center();

    double dx = dstCenter.x() - srcCenter.x();
    double dy = dstCenter.y() - srcCenter.y();

    if (std::abs(dx) > std::abs(dy))
    {
        srcPort = (dx > 0) ? Port::Right : Port::Left;
        dstPort = (dx > 0) ? Port::Left  : Port::Right;
    }
    else
    {
        srcPort = (dy > 0) ? Port::Bottom : Port::Top;
        dstPort = (dy > 0) ? Port::Top    : Port::Bottom;
    }
}

QPainterPath GraphEdgeItem::buildPath() const
{
    QPainterPath path;
    GraphNodeItem* sNode = effectiveSrcNode();
    GraphNodeItem* dNode = effectiveDstNode();
    if (!sNode || !dNode || sNode == dNode)
        return path;

    QRectF srcRect = sNode->mapToScene(sNode->nodeRect()).boundingRect();
    QRectF dstRect = dNode->mapToScene(dNode->nodeRect()).boundingRect();

    QPointF srcCenter = srcRect.center();
    QPointF dstCenter = dstRect.center();

    double gapRight  = dstRect.left()  - srcRect.right();
    double gapLeft   = srcRect.left()  - dstRect.right();
    double gapBottom = dstRect.top()   - srcRect.bottom();
    double gapTop    = srcRect.top()   - dstRect.bottom();

    QPointF SrcPoint, EndPoint;

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
    else
    {
        double dx = dstCenter.x() - srcCenter.x();
        double dy = dstCenter.y() - srcCenter.y();

        if (std::abs(dx) > std::abs(dy))
        {
            SrcPoint = (dx > 0) ? QPointF(srcRect.right(), srcCenter.y()) : QPointF(srcRect.left(), srcCenter.y());
            EndPoint = (dx > 0) ? QPointF(dstRect.left(), dstCenter.y())  : QPointF(dstRect.right(), dstCenter.y());
        }
        else
        {
            SrcPoint = (dy > 0) ? QPointF(srcCenter.x(), srcRect.bottom()) : QPointF(srcCenter.x(), srcRect.top());
            EndPoint = (dy > 0) ? QPointF(dstCenter.x(), dstRect.top())    : QPointF(dstCenter.x(), dstRect.bottom());
        }
    }

    SrcPoint = mapFromScene(SrcPoint);
    EndPoint = mapFromScene(EndPoint);

    path.moveTo(SrcPoint);

    double midX = (SrcPoint.x() + EndPoint.x()) / 2.0;
    double midY = (SrcPoint.y() + EndPoint.y()) / 2.0;

    bool overlapX = (gapRight <= 0 && gapLeft <= 0);
    bool overlapY = (gapBottom <= 0 && gapTop <= 0);

    if (overlapX && !overlapY)
    {
        path.lineTo(SrcPoint.x(), midY);
        path.lineTo(EndPoint.x(), midY);
    }
    else if (overlapY && !overlapX)
    {
        path.lineTo(midX, SrcPoint.y());
        path.lineTo(midX, EndPoint.y());
    }
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
    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphEdgeState* State = isSelected() ? &theme.edge.selected 
                                : (hovered_ ? &theme.edge.hover : &theme.edge.normal);

    qreal extraMargin = State->lineWidth + State->label.offset + State->label.paddingY + 30.0;
    QRectF rect = cachedPath_.boundingRect();
    rect.adjust(-extraMargin, -extraMargin, extraMargin, extraMargin);
    return rect;
}

void GraphEdgeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    GraphNodeItem* sNode = effectiveSrcNode();
    GraphNodeItem* dNode = effectiveDstNode();
    if (!sNode || !dNode || sNode == dNode)
        return;

    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphEdgeState* State = isSelected() ? &theme.edge.selected : (hovered_ ? &theme.edge.hover : &theme.edge.normal);

    const auto& arrow = State->arrow;
    const auto& label = State->label;

    painter->setRenderHint(QPainter::Antialiasing);

    QPainterPath fullPath = cachedPath_;
    if (fullPath.elementCount() < 2)
        return;

    QPainterPath::Element e1 = fullPath.elementAt(fullPath.elementCount() - 1);
    QPainterPath::Element e2 = fullPath.elementAt(fullPath.elementCount() - 2);

    QPointF last(e1.x, e1.y);
    QPointF prev(e2.x, e2.y);
    QLineF line(prev, last);

    QPointF arrowTip = line.p2();
    double angle = std::atan2(-line.dy(), line.dx());

    double arrowWidth = arrow.width;
    double arrowHeight = arrow.height;

    QPointF arrowP1 = arrowTip - QPointF(
        std::cos(angle) * arrowWidth - std::sin(angle) * arrowHeight / 2.0,
        -std::sin(angle) * arrowWidth - std::cos(angle) * arrowHeight / 2.0);

    QPointF arrowP2 = arrowTip - QPointF(
        std::cos(angle) * arrowWidth + std::sin(angle) * arrowHeight / 2.0,
        -std::sin(angle) * arrowWidth + std::cos(angle) * arrowHeight / 2.0);

    QPointF arrowBaseCenter = (arrowP1 + arrowP2) / 2.0;

    QPainterPath shortenedPath;
    for (int i = 0; i < fullPath.elementCount(); ++i)
    {
        auto e = fullPath.elementAt(i);
        QPointF p(e.x, e.y);

        if (i == 0)
            shortenedPath.moveTo(p);
        else if (i == fullPath.elementCount() - 1)
            shortenedPath.lineTo(arrowBaseCenter);
        else
            shortenedPath.lineTo(p);
    }

    QPen edgePen(State->lineColor);
    qreal effectiveLineWidth = (isConsolidated_ && consolidatedEdgeIds_.size() > 1) ? (State->lineWidth + 1.5) : State->lineWidth;
    edgePen.setWidthF(effectiveLineWidth);
    edgePen.setStyle(State->lineStyle);
    if (State->lineStyle == Qt::CustomDashLine && !State->dashPattern.isEmpty())
    {
        edgePen.setDashPattern(State->dashPattern);
    }
    edgePen.setJoinStyle(Qt::RoundJoin);
    edgePen.setCapStyle(Qt::RoundCap);

    painter->setPen(edgePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(shortenedPath);

    QPolygonF arrowHead;
    arrowHead << arrowTip << arrowP1 << arrowP2;

    QPen arrowPen(arrow.lineColor);
    arrowPen.setWidth(arrow.lineWidth);
    arrowPen.setJoinStyle(Qt::RoundJoin);

    painter->setPen(arrowPen);
    painter->setBrush(arrow.fillColor);
    painter->drawPolygon(arrowHead);

    QString title = cachedTitle_;
    bool hasTitle = !title.isEmpty();

    qreal t = 0.5;
    QPointF p1 = fullPath.pointAtPercent(t);
    QPointF p2 = fullPath.pointAtPercent(t + 0.01);

    double textAngle = std::atan2(p2.y() - p1.y(), p2.x() - p1.x());
    double degrees = textAngle * 180.0 / M_PI;

    if (degrees > 90 || degrees < -90)
        degrees += 180.0;

    qreal verticalDistance = 0.0;
    if (hasTitle)
    {
        QFont font;
        font.setPointSize(label.fontSize);
        font.setBold(label.bold);
        painter->setFont(font);

        QRect textRect = cachedTitleRect_;
        qreal badgeHalfHeight = (textRect.height() / 2.0) + label.paddingY;
        verticalDistance = (effectiveLineWidth / 2.0) + label.offset + badgeHalfHeight;

        QRect bgRect = textRect.adjusted(-label.paddingX, -label.paddingY, label.paddingX, label.paddingY);
        bgRect.moveCenter(QPoint(0, static_cast<int>(-verticalDistance)));

        painter->save();
        painter->translate(p1);
        painter->rotate(degrees);

        QPen bgPen(label.borderColor);
        bgPen.setWidth(label.borderWidth);

        painter->setPen(bgPen);
        painter->setBrush(label.backgroundColor);
        painter->drawRoundedRect(bgRect, label.radius, label.radius);

        painter->setPen(label.textColor);
        painter->drawText(bgRect, Qt::AlignCenter, title);
        painter->restore();
    }

    // Governance Status Badge (outside the edge line)
    if (GraphNodeItem::showGovernanceBadges() && model_)
    {
        auto edgeOpt = model_->getEdgeById(e_id);
        if (edgeOpt || isConsolidated_)
        {
            Status status = edgeOpt ? edgeOpt->status : Status::New;
            QString statusText;
            if (isConsolidated_ && consolidatedEdgeIds_.size() > 1)
            {
                status = consolidatedStatus_;
                statusText = QString::fromStdString(to_string(status)).toUpper();
                if (consolidatedHasIssues_)
                    statusText += " ⚠";
            }
            else if (edgeOpt)
            {
                statusText = QString::fromStdString(to_string(status)).toUpper();
            }

            if (!statusText.isEmpty())
            {
                QFont badgeFont;
                badgeFont.setPointSize(7);
                badgeFont.setBold(true);
                QFontMetrics bFm(badgeFont);

                qreal badgeW = std::max(42.0, (qreal)bFm.horizontalAdvance(statusText) + 10.0);
                qreal badgeH = 14.0;

                qreal govDist = (effectiveLineWidth / 2.0) + label.offset + (badgeH / 2.0);
                qreal govY = hasTitle ? govDist : -govDist;
                QRectF govBadgeRect(-badgeW / 2.0, govY - (badgeH / 2.0), badgeW, badgeH);

                QColor badgeColor = GraphNodeItem::governanceStatusColor(status);
                QColor badgeBg    = GraphNodeItem::governanceStatusBgColor(status);

                painter->save();
                painter->translate(p1);
                painter->rotate(degrees);

                QPainterPath badgePath;
                badgePath.addRoundedRect(govBadgeRect, 3.0, 3.0);

                // Dark base background for crisp contrast against any canvas background
                painter->fillPath(badgePath, QColor(36, 40, 52, 230));
                painter->fillPath(badgePath, badgeBg);

                QPen badgePen(badgeColor, 1.0);
                painter->setPen(badgePen);
                painter->drawPath(badgePath);

                painter->setFont(badgeFont);
                painter->setPen(badgeColor);
                painter->drawText(govBadgeRect, Qt::AlignCenter, statusText);
                painter->restore();
            }
        }
    }
}

QPainterPath GraphEdgeItem::shape() const
{
    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphEdgeState* State = isSelected() ? &theme.edge.selected 
                                : (hovered_ ? &theme.edge.hover : &theme.edge.normal);

    qreal tolerance = 5.0;
    qreal hitWidth = State->lineWidth + (tolerance * 2.0);

    QPainterPath result;
    QPainterPathStroker stroker;
    stroker.setWidth(hitWidth);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    result.addPath(stroker.createStroke(cachedPath_));

    QString title = cachedTitle_;
    bool hasTitle = !title.isEmpty();
    const auto& label = State->label;

    if (hasTitle)
    {
        QFont font;
        font.setPointSize(label.fontSize);
        font.setBold(label.bold);

        QRect textRect = cachedTitleRect_;
        QPainterPath path = cachedPath_;
        QPointF p = path.pointAtPercent(0.5);

        qreal badgeHalfHeight = (textRect.height() / 2.0) + label.paddingY;
        qreal verticalDistance = (State->lineWidth / 2.0) + label.offset + badgeHalfHeight;

        QRectF bgRect = textRect.adjusted(-label.paddingX, -label.paddingY, label.paddingX, label.paddingY);
        bgRect.moveCenter(QPointF(p.x(), p.y() - verticalDistance));

        QPainterPath labelPath;
        labelPath.addRoundedRect(bgRect, label.radius, label.radius);
        result.addPath(labelPath);
    }

    if (GraphNodeItem::showGovernanceBadges() && model_)
    {
        auto edgeOpt = model_->getEdgeById(e_id);
        if (edgeOpt)
        {
            QString statusText = QString::fromStdString(to_string(edgeOpt->status)).toUpper();
            QFont badgeFont;
            badgeFont.setPointSize(7);
            badgeFont.setBold(true);
            QFontMetrics bFm(badgeFont);

            qreal badgeW = std::max(42.0, (qreal)bFm.horizontalAdvance(statusText) + 10.0);
            qreal badgeH = 14.0;

            QPointF p = cachedPath_.pointAtPercent(0.5);
            qreal govDist = (State->lineWidth / 2.0) + label.offset + (badgeH / 2.0);
            qreal govY = hasTitle ? govDist : -govDist;

            QRectF govBadgeRect(-badgeW / 2.0, -badgeH / 2.0, badgeW, badgeH);
            govBadgeRect.moveCenter(QPointF(p.x(), p.y() + govY));

            QPainterPath badgePath;
            badgePath.addRoundedRect(govBadgeRect, 3.0, 3.0);
            result.addPath(badgePath);
        }
    }

    return result;
}

void GraphEdgeItem::updateEndpoints()
{
    GraphNodeItem* sNode = effectiveSrcNode();
    GraphNodeItem* dNode = effectiveDstNode();
    if (!sNode || !dNode || sNode == dNode)
    {
        prepareGeometryChange();
        cachedPath_ = QPainterPath();
        cachedBounds_ = QRectF();
        return;
    }

    if (!sNode->scene() || !dNode->scene())
        return;

    prepareGeometryChange();
    refreshPath();
}

void GraphEdgeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = true;
    setZValue(25.0);
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void GraphEdgeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = false;

    qreal defaultZ = 1.5;
    if (src_ && dst_)
    {
        defaultZ = std::max(src_->zValue(), dst_->zValue()) - 0.1;
    }

    setZValue(isSelected() ? 20.0 : defaultZ);
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

void GraphEdgeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (!model_)
        return;

    event->accept();
    EdgeEditorDialog dlg(model_, e_id);
    if (dlg.exec() == QDialog::Accepted)
    {
        refreshLayout();
        if (scene())
        {
            for (auto* it : scene()->items())
            {
                if (auto* ni = dynamic_cast<GraphNodeItem*>(it))
                {
                    ni->refreshGeometry();
                    ni->update();
                }
            }
            if (!scene()->views().isEmpty())
            {
                if (auto* mainWin = dynamic_cast<MainWindow*>(scene()->views().first()->window()))
                    mainWin->populateNavigator();
            }
        }
    }
}

void GraphEdgeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    event->ignore();
}

void GraphEdgeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    setSelected(true);

    MainWindow* mainWin = nullptr;
    if (scene() && !scene()->views().isEmpty())
    {
        mainWin = dynamic_cast<MainWindow*>(scene()->views().first()->window());
    }

    QMenu menu;
    QAction* editAct = menu.addAction("Edit");
    QAction* delAct = menu.addAction("Delete");

    menu.addSeparator();
    auto* govMenu = menu.addMenu("Governance");
    QAction* reviewAct  = govMenu->addAction("Mark as Reviewed...");
    QAction* approveAct = govMenu->addAction("Mark as Approved...");
    QAction* changeAct  = govMenu->addAction("Mark as Changed");
    QAction* invalidAct = govMenu->addAction("Mark as Invalid");
    govMenu->addSeparator();
    QAction* verifyAct  = govMenu->addAction("Verify Checksum Integrity");

    QAction* selected = menu.exec(event->screenPos());
    if (selected == editAct)
    {
        EdgeEditorDialog dlg(model_, e_id);
        if (dlg.exec() == QDialog::Accepted)
        {
            refreshLayout();
            if (scene())
            {
                for (auto* it : scene()->items())
                {
                    if (auto* ni = dynamic_cast<GraphNodeItem*>(it))
                    {
                        ni->refreshGeometry();
                        ni->update();
                    }
                }
            }
            if (mainWin)
                mainWin->populateNavigator();
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
            isApprove ? "Approve Edge(s)" : "Review Edge(s)",
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
                if (auto* edgeItem = dynamic_cast<GraphEdgeItem*>(item))
                {
                    if (edgeItem->isConsolidated() && edgeItem->consolidatedEdgeIds().size() > 1)
                    {
                        for (EdgeId cid : edgeItem->consolidatedEdgeIds())
                        {
                            model_->setEdgeGovernance(cid, newStatus, revStr);
                        }
                    }
                    else
                    {
                        model_->setEdgeGovernance(edgeItem->edgeId(), newStatus, revStr);
                    }
                    edgeItem->refreshLayout();
                }
            }

            if (scene())
            {
                for (auto* it : scene()->items())
                {
                    if (auto* ni = dynamic_cast<GraphNodeItem*>(it))
                    {
                        ni->refreshGeometry();
                        ni->update();
                    }
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
            if (auto* edgeItem = dynamic_cast<GraphEdgeItem*>(item))
            {
                if (edgeItem->isConsolidated() && edgeItem->consolidatedEdgeIds().size() > 1)
                {
                    for (EdgeId cid : edgeItem->consolidatedEdgeIds())
                    {
                        model_->setEdgeGovernance(cid, Status::Changed, "");
                    }
                }
                else
                {
                    model_->setEdgeGovernance(edgeItem->edgeId(), Status::Changed, "");
                }
                edgeItem->refreshLayout();
            }
        }
        if (scene())
        {
            for (auto* it : scene()->items())
            {
                if (auto* ni = dynamic_cast<GraphNodeItem*>(it))
                {
                    ni->refreshGeometry();
                    ni->update();
                }
            }
        }
        if (mainWin)
            mainWin->populateNavigator();
    }
    else if (selected == invalidAct)
    {
        for (auto* item : scene()->selectedItems())
        {
            if (auto* edgeItem = dynamic_cast<GraphEdgeItem*>(item))
            {
                if (edgeItem->isConsolidated() && edgeItem->consolidatedEdgeIds().size() > 1)
                {
                    for (EdgeId cid : edgeItem->consolidatedEdgeIds())
                    {
                        model_->setEdgeGovernance(cid, Status::Invalid, "");
                    }
                }
                else
                {
                    model_->setEdgeGovernance(edgeItem->edgeId(), Status::Invalid, "");
                }
                edgeItem->refreshLayout();
            }
        }
        if (scene())
        {
            for (auto* it : scene()->items())
            {
                if (auto* ni = dynamic_cast<GraphNodeItem*>(it))
                {
                    ni->refreshGeometry();
                    ni->update();
                }
            }
        }
        if (mainWin)
            mainWin->populateNavigator();
    }
    else if (selected == verifyAct)
    {
        if (isConsolidated_ && consolidatedEdgeIds_.size() > 1)
        {
            QString msg = QString("<b>Consolidated Edge (%1 Bundled Edges)</b><br><br>").arg(consolidatedEdgeIds_.size());
            for (EdgeId cid : consolidatedEdgeIds_)
            {
                auto edgeOpt = model_->getEdgeById(cid);
                if (edgeOpt)
                {
                    uint32_t expected = model_->computeEdgeChecksum(*edgeOpt);
                    bool match = (expected == edgeOpt->checksum);
                    msg += QString("<b>Edge #%1</b> (%2) — %3<br>"
                                   "Status: %4 | Reviewer: %5<br>"
                                   "Stored: 0x%6 | Calculated: 0x%7<br><br>")
                        .arg(cid)
                        .arg(QString::fromStdString(edgeOpt->edgeType).toHtmlEscaped())
                        .arg(match ? "<span style='color:#a3be8c;font-weight:bold;'>✓ Valid</span>"
                                   : "<span style='color:#bf616a;font-weight:bold;'>⚠ MISMATCH</span>")
                        .arg(QString::fromStdString(to_string(edgeOpt->status)).toUpper())
                        .arg(edgeOpt->reviewer.empty() ? "<i>(None)</i>" : QString::fromStdString(edgeOpt->reviewer).toHtmlEscaped())
                        .arg(QString::number(edgeOpt->checksum, 16).toUpper())
                        .arg(QString::number(expected, 16).toUpper());
                }
            }

            QWidget* parentWidget = (!scene()->views().isEmpty()) ? scene()->views().first() : nullptr;
            QMessageBox::information(parentWidget, "Governance Integrity Verification", msg);
        }
        else
        {
            auto edgeOpt = model_->getEdgeById(e_id);
            if (edgeOpt)
            {
                uint32_t expected = model_->computeEdgeChecksum(*edgeOpt);
                bool match = (expected == edgeOpt->checksum);

                QString msg = QString("<b>Edge:</b> ID %1 (%2)<br>"
                                      "<b>Status:</b> %3<br>"
                                      "<b>Reviewer:</b> %4<br><br>"
                                      "<b>Stored Checksum:</b> 0x%5<br>"
                                      "<b>Calculated Checksum:</b> 0x%6<br><br>"
                                      "<b>Integrity:</b> %7")
                    .arg(e_id)
                    .arg(QString::fromStdString(edgeOpt->edgeType).toHtmlEscaped())
                    .arg(QString::fromStdString(to_string(edgeOpt->status)).toUpper())
                    .arg(edgeOpt->reviewer.empty() ? "<i>(None)</i>" : QString::fromStdString(edgeOpt->reviewer).toHtmlEscaped())
                    .arg(QString::number(edgeOpt->checksum, 16).toUpper())
                    .arg(QString::number(expected, 16).toUpper())
                    .arg(match ? "<span style='color:#a3be8c;font-weight:bold;'>✓ Valid (Tamper-Free)</span>"
                               : "<span style='color:#bf616a;font-weight:bold;'>⚠ CHECKSUM MISMATCH (TAMPER DETECTED)</span>");

                QWidget* parentWidget = (!scene()->views().isEmpty()) ? scene()->views().first() : nullptr;
                QMessageBox::information(
                    parentWidget,
                    "Governance Integrity Verification",
                    msg);
            }
        }
    }
}

void GraphEdgeItem::refreshPath()
{
    prepareGeometryChange();
    cachedPath_ = buildPath();

    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphEdgeState* State = isSelected() ? &theme.edge.selected 
                                : (hovered_ ? &theme.edge.hover : &theme.edge.normal);

    qreal extraMargin = State->lineWidth + State->label.offset + State->label.paddingY + 30.0;
    cachedBounds_ = cachedPath_.boundingRect();
    cachedBounds_.adjust(-extraMargin, -extraMargin, extraMargin, extraMargin);
    update();
}

void GraphEdgeItem::refreshLayout()
{
    auto edge = model_ ? model_->getEdgeById(e_id) : std::nullopt;
    if (!edge && !isConsolidated_)
        return;

    const auto& theme = GraphThemeManager::instance()->theme();

    if (isConsolidated_ && consolidatedEdgeIds_.size() > 1)
    {
        QString typesStr = consolidatedTypes_.isEmpty() ? "Edge" : consolidatedTypes_.join(", ");
        cachedTitle_ = QString("%1 (%2)").arg(typesStr).arg(consolidatedEdgeIds_.size());

        QString tip = QString("<b>Consolidated Edge (%1 edges)</b><br>").arg(consolidatedEdgeIds_.size());
        for (EdgeId id : consolidatedEdgeIds_)
        {
            if (model_)
            {
                auto subEdge = model_->getEdgeById(id);
                if (subEdge)
                {
                    tip += QString("<br>• <b>Edge #%1</b> (%2): %3")
                        .arg(id)
                        .arg(QString::fromStdString(subEdge->edgeType).toHtmlEscaped())
                        .arg(QString::fromStdString(to_string(subEdge->status)).toUpper());
                }
            }
        }
        setToolTip(tip);
    }
    else if (edge)
    {
        cachedTitle_ = QString::fromStdString(edge->edgeType);
        QString tip = QString("<b>Edge #%1</b> (%2)<br>"
                              "Status: <b>%3</b><br>"
                              "Reviewer: %4<br>"
                              "Checksum: 0x%5")
            .arg(e_id)
            .arg(QString::fromStdString(edge->edgeType).toHtmlEscaped())
            .arg(QString::fromStdString(to_string(edge->status)).toUpper())
            .arg(edge->reviewer.empty() ? "<i>(None)</i>" : QString::fromStdString(edge->reviewer).toHtmlEscaped())
            .arg(QString::number(edge->checksum, 16).toUpper());
        setToolTip(tip);
    }

    QFont font;
    font.setPointSize(theme.edge.normal.label.fontSize);
    font.setBold(theme.edge.normal.label.bold);
    QFontMetrics fm(font);
    cachedTitleRect_ = fm.boundingRect(cachedTitle_);

    refreshPath();
}