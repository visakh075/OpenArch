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
#include <queue>
#include <vector>

EdgeRoutingAlgorithm GraphEdgeItem::s_globalRoutingAlgorithm = EdgeRoutingAlgorithm::SmartOrthogonal;
bool GraphEdgeItem::s_lineJumpsEnabled = true;

EdgeRoutingAlgorithm GraphEdgeItem::globalRoutingAlgorithm()
{
    return s_globalRoutingAlgorithm;
}

void GraphEdgeItem::setGlobalRoutingAlgorithm(EdgeRoutingAlgorithm algo)
{
    s_globalRoutingAlgorithm = algo;
}

bool GraphEdgeItem::lineJumpsEnabled()
{
    return s_lineJumpsEnabled;
}

void GraphEdgeItem::setLineJumpsEnabled(bool enabled)
{
    s_lineJumpsEnabled = enabled;
}

QString GraphEdgeItem::routingAlgorithmName(EdgeRoutingAlgorithm algo)
{
    switch (algo)
    {
        case EdgeRoutingAlgorithm::SmartOrthogonal:  return "Smart Orthogonal (A* Avoidance)";
        case EdgeRoutingAlgorithm::DirectOrthogonal: return "Direct Orthogonal (Manhattan)";
        case EdgeRoutingAlgorithm::CircuitBoard:     return "Circuit Board (Sharp 90°)";
        case EdgeRoutingAlgorithm::SmoothBezier:     return "Smooth Curved (Bezier Spline)";
        case EdgeRoutingAlgorithm::StraightLine:     return "Straight Line (Direct Vector)";
        case EdgeRoutingAlgorithm::Octilinear:       return "Octilinear (45° Chamfer / Transit)";
        case EdgeRoutingAlgorithm::BusHighway:        return "Bus Highway (Central Trunk)";
    }
    return "Unknown";
}

const std::vector<EdgeRoutingAlgorithm>& GraphEdgeItem::availableRoutingAlgorithms()
{
    static const std::vector<EdgeRoutingAlgorithm> s_algos = {
        EdgeRoutingAlgorithm::SmartOrthogonal,
        EdgeRoutingAlgorithm::DirectOrthogonal,
        EdgeRoutingAlgorithm::CircuitBoard,
        EdgeRoutingAlgorithm::SmoothBezier,
        EdgeRoutingAlgorithm::StraightLine,
        EdgeRoutingAlgorithm::Octilinear,
        EdgeRoutingAlgorithm::BusHighway
    };
    return s_algos;
}

void GraphEdgeItem::setRoutingAlgorithmOverride(std::optional<EdgeRoutingAlgorithm> algo)
{
    routingOverride_ = algo;
    refreshPath();
}

EdgeRoutingAlgorithm GraphEdgeItem::effectiveRoutingAlgorithm() const
{
    if (routingOverride_.has_value())
        return *routingOverride_;
    return s_globalRoutingAlgorithm;
}

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

GraphNodeItem* GraphEdgeItem::srcNode() const
{
    return src_.data();
}

GraphNodeItem* GraphEdgeItem::dstNode() const
{
    return dst_.data();
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

    // Group edges connecting the same pair of effective nodes (unordered pair)
    struct NodePair {
        GraphNodeItem* a;
        GraphNodeItem* b;
        bool operator<(const NodePair& o) const {
            if (a != o.a) return a < o.a;
            return b < o.b;
        }
    };

    std::map<NodePair, std::vector<GraphEdgeItem*>> groups;

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

        GraphNodeItem* n1 = std::min(s, d);
        GraphNodeItem* n2 = std::max(s, d);
        groups[{n1, n2}].push_back(edge);
    }

    std::vector<GraphEdgeItem*> visibleEdges;

    for (auto& pair : groups)
    {
        auto& edgeList = pair.second;
        if (edgeList.empty())
            continue;

        GraphNodeItem* s = edgeList.front()->effectiveSrcNode();
        GraphNodeItem* d = edgeList.front()->effectiveDstNode();

        bool isFoldedCase = (s && s->isContainer() && s->isFolded()) ||
                            (d && d->isContainer() && d->isFolded());

        if (isFoldedCase)
        {
            GraphEdgeItem* primary = edgeList.front();
            primary->setConsolidatedEdges(edgeList);
            primary->setParallelInfo(0, 1);
            primary->setVisible(true);
            visibleEdges.push_back(primary);

            for (size_t i = 1; i < edgeList.size(); ++i)
            {
                edgeList[i]->setConsolidatedEdges({});
                edgeList[i]->setVisible(false);
            }
        }
        else
        {
            int count = static_cast<int>(edgeList.size());
            for (int i = 0; i < count; ++i)
            {
                edgeList[i]->setConsolidatedEdges({});
                edgeList[i]->setVisible(true);
                edgeList[i]->setParallelInfo(i, count);
                visibleEdges.push_back(edgeList[i]);
            }
        }
    }

    if (visibleEdges.empty())
        return;

    // 1. Determine ports for each visible edge
    std::map<GraphEdgeItem*, Port> edgeSrcPort;
    std::map<GraphEdgeItem*, Port> edgeDstPort;
    for (auto* edge : visibleEdges)
    {
        Port sp, dp;
        edge->autoSelectPorts(sp, dp);
        edgeSrcPort[edge] = sp;
        edgeDstPort[edge] = dp;
    }

    // 2. Global Port Allocation across all edges at each node
    struct EndpointRef {
        GraphEdgeItem* edge;
        bool isSource;
        qreal sortKey;
    };
    std::map<std::pair<GraphNodeItem*, Port>, std::vector<EndpointRef>> nodePortMap;

    for (auto* edge : visibleEdges)
    {
        auto* s = edge->effectiveSrcNode();
        auto* d = edge->effectiveDstNode();
        QPointF sCenter = s->mapToScene(s->nodeRect()).boundingRect().center();
        QPointF dCenter = d->mapToScene(d->nodeRect()).boundingRect().center();

        Port sp = edgeSrcPort[edge];
        qreal sKey = (sp == Port::Left || sp == Port::Right) ? dCenter.y() : dCenter.x();
        nodePortMap[{s, sp}].push_back({ edge, true, sKey });

        Port dp = edgeDstPort[edge];
        qreal dKey = (dp == Port::Left || dp == Port::Right) ? sCenter.y() : sCenter.x();
        nodePortMap[{d, dp}].push_back({ edge, false, dKey });
    }

    std::map<GraphEdgeItem*, QPointF> edgeSrcPt;
    std::map<GraphEdgeItem*, QPointF> edgeDstPt;

    for (auto& entry : nodePortMap)
    {
        GraphNodeItem* node = entry.first.first;
        Port port = entry.first.second;
        auto& list = entry.second;
        if (list.empty()) continue;

        std::sort(list.begin(), list.end(), [](const EndpointRef& a, const EndpointRef& b) {
            if (std::abs(a.sortKey - b.sortKey) > 1.0)
                return a.sortKey < b.sortKey;
            return a.edge < b.edge;
        });

        QRectF rect = node->mapToScene(node->nodeRect()).boundingRect();
        QPointF c = rect.center();
        int K = static_cast<int>(list.size());
        qreal availWidth = std::max(10.0, rect.width() - 20.0);
        qreal availHeight = std::max(10.0, rect.height() - 20.0);
        qreal spacingH = (K > 1) ? std::min(14.0, availWidth / (K - 1)) : 14.0;
        qreal spacingV = (K > 1) ? std::min(14.0, availHeight / (K - 1)) : 14.0;

        for (int k = 0; k < K; ++k)
        {
            QPointF pt;
            switch (port)
            {
                case Port::Top:
                {
                    qreal offset = (k - (K - 1) / 2.0) * spacingH;
                    qreal x = std::clamp(c.x() + offset, rect.left() + 10.0, rect.right() - 10.0);
                    pt = QPointF(x, rect.top());
                    break;
                }
                case Port::Bottom:
                {
                    qreal offset = (k - (K - 1) / 2.0) * spacingH;
                    qreal x = std::clamp(c.x() + offset, rect.left() + 10.0, rect.right() - 10.0);
                    pt = QPointF(x, rect.bottom());
                    break;
                }
                case Port::Left:
                {
                    qreal offset = (k - (K - 1) / 2.0) * spacingV;
                    qreal y = std::clamp(c.y() + offset, rect.top() + 10.0, rect.bottom() - 10.0);
                    pt = QPointF(rect.left(), y);
                    break;
                }
                case Port::Right:
                {
                    qreal offset = (k - (K - 1) / 2.0) * spacingV;
                    qreal y = std::clamp(c.y() + offset, rect.top() + 10.0, rect.bottom() - 10.0);
                    pt = QPointF(rect.right(), y);
                    break;
                }
            }

            if (list[k].isSource)
                edgeSrcPt[list[k].edge] = pt;
            else
                edgeDstPt[list[k].edge] = pt;
        }
    }

    // 3. Obstacle collection
    std::vector<std::pair<GraphNodeItem*, QRectF>> allNodeObstacles;
    for (QGraphicsItem* item : scene->items())
    {
        auto* node = dynamic_cast<GraphNodeItem*>(item);
        if (!node || !node->isVisible())
            continue;
        QRectF r = node->mapToScene(node->nodeRect()).boundingRect();
        if (r.width() > 0 && r.height() > 0)
        {
            allNodeObstacles.push_back({ node, r });
        }
    }

    // 4. Initial path calculation for each visible edge
    const auto& theme = GraphThemeManager::instance()->theme();
    qreal defaultArrowWidth = theme.edge.normal.arrow.width;

    std::map<GraphEdgeItem*, std::vector<QPointF>> edgeWaypoints;
    std::vector<QLineF> routedSegments;

    for (auto* edge : visibleEdges)
    {
        auto* s = edge->effectiveSrcNode();
        auto* d = edge->effectiveDstNode();
        if (!s || !d || s == d) continue;

        QPointF srcPt = edgeSrcPt[edge];
        QPointF dstPt = edgeDstPt[edge];
        Port sp = edgeSrcPort[edge];
        Port dp = edgeDstPort[edge];

        qreal lane = 0.0;
        if (edge->parallelCount() > 1)
        {
            lane = edge->parallelIndex() - (edge->parallelCount() - 1) / 2.0;
        }
        qreal channelOffset = lane * 14.0;

        const auto* state = edge->isSelected() ? &theme.edge.selected
                          : (edge->isHovered() ? &theme.edge.hover : &theme.edge.normal);
        qreal arrowWidth = state ? state->arrow.width : defaultArrowWidth;

        EdgeRoutingAlgorithm algo = edge->effectiveRoutingAlgorithm();

        if (algo == EdgeRoutingAlgorithm::SmoothBezier)
        {
            edge->buildSmoothBezierPath(srcPt, sp, dstPt, dp, channelOffset, arrowWidth);
            routedSegments.emplace_back(srcPt, dstPt);
            continue;
        }
        if (algo == EdgeRoutingAlgorithm::StraightLine)
        {
            edge->buildStraightLinePath(srcPt, dstPt, channelOffset, arrowWidth);
            routedSegments.emplace_back(srcPt, dstPt);
            continue;
        }

        if (algo == EdgeRoutingAlgorithm::SmartOrthogonal)
        {
            std::vector<QRectF> obstacles;
            for (const auto& pair : allNodeObstacles)
            {
                GraphNodeItem* node = pair.first;
                if (node == s || node == d) continue;

                bool isAnc = false;
                const QGraphicsItem* p1 = s->parentItem();
                while (p1) { if (p1 == node) { isAnc = true; break; } p1 = p1->parentItem(); }
                if (isAnc) continue;

                const QGraphicsItem* p2 = d->parentItem();
                while (p2) { if (p2 == node) { isAnc = true; break; } p2 = p2->parentItem(); }
                if (isAnc) continue;

                obstacles.push_back(pair.second);
            }
            edgeWaypoints[edge] = edge->computeObstacleFreePath(srcPt, sp, dstPt, dp, obstacles, channelOffset, routedSegments);
        }
        else if (algo == EdgeRoutingAlgorithm::DirectOrthogonal || algo == EdgeRoutingAlgorithm::CircuitBoard)
        {
            edgeWaypoints[edge] = edge->computeDirectOrthogonalPath(srcPt, sp, dstPt, dp, channelOffset);
        }
        else if (algo == EdgeRoutingAlgorithm::Octilinear)
        {
            edgeWaypoints[edge] = edge->computeOctilinearPath(srcPt, sp, dstPt, dp, channelOffset);
        }
        else if (algo == EdgeRoutingAlgorithm::BusHighway)
        {
            edgeWaypoints[edge] = edge->computeBusHighwayPath(srcPt, sp, dstPt, dp, channelOffset);
        }

        if (edgeWaypoints[edge].size() < 2)
        {
            edgeWaypoints[edge] = { srcPt, dstPt };
        }

        const auto& w = edgeWaypoints[edge];
        for (size_t k = 0; k + 1 < w.size(); ++k)
        {
            routedSegments.emplace_back(w[k], w[k + 1]);
        }
    }

    // 5. Global Multi-Edge Segment De-collision (Disjoint Set Union across all edges)
    struct DSU {
        std::vector<int> parent;
        DSU(int n) : parent(n) {
            for (int i = 0; i < n; ++i) parent[i] = i;
        }
        int find(int i) {
            if (parent[i] == i) return i;
            return parent[i] = find(parent[i]);
        }
        void unite(int i, int j) {
            int rootI = find(i);
            int rootJ = find(j);
            if (rootI != rootJ) parent[rootI] = rootJ;
        }
    };

    // 5a. Horizontal intermediate segments
    struct HorizSegRef {
        GraphEdgeItem* edge;
        int segIdx;
        qreal y;
        qreal minX;
        qreal maxX;
    };
    std::vector<HorizSegRef> horizSegs;

    for (auto* edge : visibleEdges)
    {
        auto it = edgeWaypoints.find(edge);
        if (it == edgeWaypoints.end()) continue;
        auto& w = it->second;
        if (w.size() < 3) continue;

        for (size_t k = 1; k + 1 < w.size() - 1; ++k)
        {
            if (std::abs(w[k].y() - w[k + 1].y()) < 1.0)
            {
                qreal minX = std::min(w[k].x(), w[k + 1].x());
                qreal maxX = std::max(w[k].x(), w[k + 1].x());
                if (maxX - minX > 6.0)
                {
                    horizSegs.push_back({ edge, static_cast<int>(k), w[k].y(), minX, maxX });
                }
            }
        }
    }

    if (!horizSegs.empty())
    {
        int numH = static_cast<int>(horizSegs.size());
        DSU dsuH(numH);

        for (int i = 0; i < numH; ++i)
        {
            for (int j = i + 1; j < numH; ++j)
            {
                if (horizSegs[i].edge == horizSegs[j].edge)
                    continue;

                if (std::abs(horizSegs[i].y - horizSegs[j].y) < 6.0)
                {
                    qreal overlap = std::min(horizSegs[i].maxX, horizSegs[j].maxX) -
                                    std::max(horizSegs[i].minX, horizSegs[j].minX);
                    if (overlap > 8.0)
                    {
                        dsuH.unite(i, j);
                    }
                }
            }
        }

        std::map<int, std::vector<int>> hGroups;
        for (int i = 0; i < numH; ++i)
        {
            hGroups[dsuH.find(i)].push_back(i);
        }

        const qreal trackSpacing = 14.0;
        for (auto& grpPair : hGroups)
        {
            auto& indices = grpPair.second;
            if (indices.size() <= 1) continue;

            std::sort(indices.begin(), indices.end(), [&](int a, int b) {
                if (horizSegs[a].edge != horizSegs[b].edge)
                    return horizSegs[a].edge < horizSegs[b].edge;
                return horizSegs[a].segIdx < horizSegs[b].segIdx;
            });

            int P = static_cast<int>(indices.size());
            for (int p = 0; p < P; ++p)
            {
                qreal dy = (p - (P - 1) / 2.0) * trackSpacing;
                auto* edge = horizSegs[indices[p]].edge;
                int k = horizSegs[indices[p]].segIdx;
                edgeWaypoints[edge][k].ry() += dy;
                edgeWaypoints[edge][k + 1].ry() += dy;
            }
        }
    }

    // 5b. Vertical intermediate segments
    struct VertSegRef {
        GraphEdgeItem* edge;
        int segIdx;
        qreal x;
        qreal minY;
        qreal maxY;
    };
    std::vector<VertSegRef> vertSegs;

    for (auto* edge : visibleEdges)
    {
        auto it = edgeWaypoints.find(edge);
        if (it == edgeWaypoints.end()) continue;
        auto& w = it->second;
        if (w.size() < 3) continue;

        for (size_t k = 1; k + 1 < w.size() - 1; ++k)
        {
            if (std::abs(w[k].x() - w[k + 1].x()) < 1.0)
            {
                qreal minY = std::min(w[k].y(), w[k + 1].y());
                qreal maxY = std::max(w[k].y(), w[k + 1].y());
                if (maxY - minY > 6.0)
                {
                    vertSegs.push_back({ edge, static_cast<int>(k), w[k].x(), minY, maxY });
                }
            }
        }
    }

    if (!vertSegs.empty())
    {
        int numV = static_cast<int>(vertSegs.size());
        DSU dsuV(numV);

        for (int i = 0; i < numV; ++i)
        {
            for (int j = i + 1; j < numV; ++j)
            {
                if (vertSegs[i].edge == vertSegs[j].edge)
                    continue;

                if (std::abs(vertSegs[i].x - vertSegs[j].x) < 6.0)
                {
                    qreal overlap = std::min(vertSegs[i].maxY, vertSegs[j].maxY) -
                                    std::max(vertSegs[i].minY, vertSegs[j].minY);
                    if (overlap > 8.0)
                    {
                        dsuV.unite(i, j);
                    }
                }
            }
        }

        std::map<int, std::vector<int>> vGroups;
        for (int i = 0; i < numV; ++i)
        {
            vGroups[dsuV.find(i)].push_back(i);
        }

        const qreal trackSpacing = 14.0;
        for (auto& grpPair : vGroups)
        {
            auto& indices = grpPair.second;
            if (indices.size() <= 1) continue;

            std::sort(indices.begin(), indices.end(), [&](int a, int b) {
                if (vertSegs[a].edge != vertSegs[b].edge)
                    return vertSegs[a].edge < vertSegs[b].edge;
                return vertSegs[a].segIdx < vertSegs[b].segIdx;
            });

            int Q = static_cast<int>(indices.size());
            for (int q = 0; q < Q; ++q)
            {
                qreal dx = (q - (Q - 1) / 2.0) * trackSpacing;
                auto* edge = vertSegs[indices[q]].edge;
                int k = vertSegs[indices[q]].segIdx;
                edgeWaypoints[edge][k].rx() += dx;
                edgeWaypoints[edge][k + 1].rx() += dx;
            }
        }
    }

    // 5c. Calculate Bridge Hops for crossings between horizontal and vertical segments
    std::map<GraphEdgeItem*, std::vector<QPointF>> edgeBridgeHops;

    if (s_lineJumpsEnabled)
    {
        for (auto* edgeA : visibleEdges)
        {
            auto itA = edgeWaypoints.find(edgeA);
            if (itA == edgeWaypoints.end()) continue;
            const auto& wA = itA->second;
            if (wA.size() < 2) continue;

            for (auto* edgeB : visibleEdges)
            {
                if (edgeA == edgeB) continue;
                auto itB = edgeWaypoints.find(edgeB);
                if (itB == edgeWaypoints.end()) continue;
                const auto& wB = itB->second;
                if (wB.size() < 2) continue;

                for (size_t i = 0; i + 1 < wA.size(); ++i)
                {
                    const QPointF& p1 = wA[i];
                    const QPointF& p2 = wA[i + 1];
                    if (std::abs(p1.y() - p2.y()) > 1.0) continue;

                    qreal yA = p1.y();
                    qreal minXA = std::min(p1.x(), p2.x());
                    qreal maxXA = std::max(p1.x(), p2.x());

                    for (size_t j = 0; j + 1 < wB.size(); ++j)
                    {
                        const QPointF& q1 = wB[j];
                        const QPointF& q2 = wB[j + 1];
                        if (std::abs(q1.x() - q2.x()) > 1.0) continue;

                        qreal xB = q1.x();
                        qreal minYB = std::min(q1.y(), q2.y());
                        qreal maxYB = std::max(q1.y(), q2.y());

                        if (xB > minXA + 8.0 && xB < maxXA - 8.0 &&
                            yA > minYB + 4.0 && yA < maxYB - 4.0)
                        {
                            edgeBridgeHops[edgeA].push_back(QPointF(xB, yA));
                        }
                    }
                }
            }
        }
    }

    // 6. Build final rounded paths for all visible edges
    for (auto* edge : visibleEdges)
    {
        auto it = edgeWaypoints.find(edge);
        if (it != edgeWaypoints.end() && !it->second.empty())
        {
            edge->setWaypointsAndBuildPath(it->second, edgeBridgeHops[edge]);
        }
    }
}

QPointF GraphEdgeItem::portScenePosition(GraphNodeItem* node, Port port) const
{
    if (!node)
        return QPointF();

    QRectF rect = node->mapToScene(node->nodeRect()).boundingRect();
    QPointF c = rect.center();

    qreal lane = 0.0;
    if (parallelCount_ > 1)
    {
        lane = parallelIndex_ - (parallelCount_ - 1) / 2.0;
    }
    qreal offset = lane * 14.0;

    switch (port)
    {
        case Port::Top:
        {
            qreal x = std::clamp(c.x() + offset, rect.left() + 10.0, rect.right() - 10.0);
            return QPointF(x, rect.top());
        }
        case Port::Bottom:
        {
            qreal x = std::clamp(c.x() + offset, rect.left() + 10.0, rect.right() - 10.0);
            return QPointF(x, rect.bottom());
        }
        case Port::Left:
        {
            qreal y = std::clamp(c.y() + offset, rect.top() + 10.0, rect.bottom() - 10.0);
            return QPointF(rect.left(), y);
        }
        case Port::Right:
        {
            qreal y = std::clamp(c.y() + offset, rect.top() + 10.0, rect.bottom() - 10.0);
            return QPointF(rect.right(), y);
        }
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

    double gapRight  = dstRect.left()  - srcRect.right();
    double gapLeft   = srcRect.left()  - dstRect.right();
    double gapBottom = dstRect.top()   - srcRect.bottom();
    double gapTop    = srcRect.top()   - dstRect.bottom();

    if (gapRight > 10.0 && gapBottom <= 40.0 && gapTop <= 40.0)
    {
        srcPort = Port::Right;
        dstPort = Port::Left;
        return;
    }
    if (gapLeft > 10.0 && gapBottom <= 40.0 && gapTop <= 40.0)
    {
        srcPort = Port::Left;
        dstPort = Port::Right;
        return;
    }
    if (gapBottom > 10.0 && gapRight <= 40.0 && gapLeft <= 40.0)
    {
        srcPort = Port::Bottom;
        dstPort = Port::Top;
        return;
    }
    if (gapTop > 10.0 && gapRight <= 40.0 && gapLeft <= 40.0)
    {
        srcPort = Port::Top;
        dstPort = Port::Bottom;
        return;
    }

    QPointF srcCenter = srcRect.center();
    QPointF dstCenter = dstRect.center();

    double dx = dstCenter.x() - srcCenter.x();
    double dy = dstCenter.y() - srcCenter.y();

    if (std::abs(dx) >= std::abs(dy))
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

std::vector<QPointF> GraphEdgeItem::computeObstacleFreePath(
    const QPointF& srcPoint, Port srcPort,
    const QPointF& dstPoint, Port dstPort,
    const std::vector<QRectF>& obstacles,
    qreal channelOffset,
    const std::vector<QLineF>& existingEdgeSegments) const
{
    QPointF srcNormal(0, 0);
    switch (srcPort)
    {
        case Port::Right:  srcNormal = QPointF(1, 0); break;
        case Port::Left:   srcNormal = QPointF(-1, 0); break;
        case Port::Bottom: srcNormal = QPointF(0, 1); break;
        case Port::Top:    srcNormal = QPointF(0, -1); break;
    }

    QPointF dstNormal(0, 0);
    switch (dstPort)
    {
        case Port::Right:  dstNormal = QPointF(1, 0); break;
        case Port::Left:   dstNormal = QPointF(-1, 0); break;
        case Port::Bottom: dstNormal = QPointF(0, 1); break;
        case Port::Top:    dstNormal = QPointF(0, -1); break;
    }

    const qreal stubLen = 28.0;
    QPointF startStub = srcPoint + srcNormal * stubLen;
    QPointF endStub = dstPoint + dstNormal * stubLen;

    auto segmentIntersectsRect = [](const QPointF& p1, const QPointF& p2, const QRectF& r) -> bool {
        if (r.width() <= 0 || r.height() <= 0)
            return false;

        if (std::abs(p1.y() - p2.y()) < 1e-4)
        {
            qreal y = p1.y();
            if (y <= r.top() || y >= r.bottom())
                return false;
            qreal minX = std::min(p1.x(), p2.x());
            qreal maxX = std::max(p1.x(), p2.x());
            return std::max(minX, r.left()) < std::min(maxX, r.right());
        }
        if (std::abs(p1.x() - p2.x()) < 1e-4)
        {
            qreal x = p1.x();
            if (x <= r.left() || x >= r.right())
                return false;
            qreal minY = std::min(p1.y(), p2.y());
            qreal maxY = std::max(p1.y(), p2.y());
            return std::max(minY, r.top()) < std::min(maxY, r.bottom());
        }
        return false;
    };

    QRectF searchBox = QRectF(srcPoint, dstPoint).normalized().adjusted(-120.0, -120.0, 120.0, 120.0);
    std::vector<QRectF> relevantObs;
    for (const auto& obs : obstacles)
    {
        if (obs.intersects(searchBox))
        {
            relevantObs.push_back(obs);
        }
    }

    std::vector<QRectF> hardObstacles;
    hardObstacles.reserve(relevantObs.size() + 2);
    for (const auto& obs : relevantObs)
    {
        hardObstacles.push_back(obs.adjusted(-1.0, -1.0, 1.0, 1.0));
    }

    GraphNodeItem* sNode = effectiveSrcNode();
    GraphNodeItem* dNode = effectiveDstNode();
    if (sNode)
    {
        QRectF sR = sNode->mapToScene(sNode->nodeRect()).boundingRect();
        hardObstacles.push_back(sR.adjusted(2.0, 2.0, -2.0, -2.0));
    }
    if (dNode)
    {
        QRectF dR = dNode->mapToScene(dNode->nodeRect()).boundingRect();
        hardObstacles.push_back(dR.adjusted(2.0, 2.0, -2.0, -2.0));
    }

    const qreal M = 18.0;

    std::vector<qreal> xCoords;
    std::vector<qreal> yCoords;

    xCoords.push_back(srcPoint.x());
    xCoords.push_back(startStub.x());
    xCoords.push_back(endStub.x());
    xCoords.push_back(dstPoint.x());
    xCoords.push_back((startStub.x() + endStub.x()) / 2.0 + channelOffset);

    yCoords.push_back(srcPoint.y());
    yCoords.push_back(startStub.y());
    yCoords.push_back(endStub.y());
    yCoords.push_back(dstPoint.y());
    yCoords.push_back((startStub.y() + endStub.y()) / 2.0 + channelOffset);

    for (const auto& obs : relevantObs)
    {
        xCoords.push_back(obs.left() - M);
        xCoords.push_back(obs.right() + M);
        yCoords.push_back(obs.top() - M);
        yCoords.push_back(obs.bottom() + M);
    }

    for (const auto& seg : existingEdgeSegments)
    {
        if (std::abs(seg.p1().y() - seg.p2().y()) < 1.0)
        {
            yCoords.push_back(seg.p1().y() - 14.0);
            yCoords.push_back(seg.p1().y() + 14.0);
        }
        else if (std::abs(seg.p1().x() - seg.p2().x()) < 1.0)
        {
            xCoords.push_back(seg.p1().x() - 14.0);
            xCoords.push_back(seg.p1().x() + 14.0);
        }
    }

    if (sNode)
    {
        QRectF sR = sNode->mapToScene(sNode->nodeRect()).boundingRect();
        xCoords.push_back(sR.left() - M);
        xCoords.push_back(sR.right() + M);
        yCoords.push_back(sR.top() - M);
        yCoords.push_back(sR.bottom() + M);
    }
    if (dNode)
    {
        QRectF dR = dNode->mapToScene(dNode->nodeRect()).boundingRect();
        xCoords.push_back(dR.left() - M);
        xCoords.push_back(dR.right() + M);
        yCoords.push_back(dR.top() - M);
        yCoords.push_back(dR.bottom() + M);
    }

    auto deduplicate = [](std::vector<qreal>& coords) {
        std::sort(coords.begin(), coords.end());
        std::vector<qreal> result;
        for (qreal v : coords)
        {
            if (result.empty() || std::abs(v - result.back()) > 3.0)
            {
                result.push_back(v);
            }
        }
        coords = result;
    };

    deduplicate(xCoords);
    deduplicate(yCoords);

    int numX = static_cast<int>(xCoords.size());
    int numY = static_cast<int>(yCoords.size());

    auto findClosest = [](const std::vector<qreal>& coords, qreal val) -> int {
        int bestIdx = 0;
        qreal bestDist = 1e18;
        for (int i = 0; i < static_cast<int>(coords.size()); ++i)
        {
            qreal d = std::abs(coords[i] - val);
            if (d < bestDist)
            {
                bestDist = d;
                bestIdx = i;
            }
        }
        return bestIdx;
    };

    int startXi = findClosest(xCoords, startStub.x());
    int startYi = findClosest(yCoords, startStub.y());
    int endXi = findClosest(xCoords, endStub.x());
    int endYi = findClosest(yCoords, endStub.y());

    int totalStates = numX * numY * 3;
    std::vector<qreal> dist(totalStates, 1e18);
    std::vector<int> cameFrom(totalStates, -1);

    struct PQNode {
        int state;
        qreal cost;
        bool operator>(const PQNode& o) const { return cost > o.cost; }
    };

    std::priority_queue<PQNode, std::vector<PQNode>, std::greater<PQNode>> pq;

    int initialDir = 0;
    if (srcPort == Port::Left || srcPort == Port::Right) initialDir = 1;
    else if (srcPort == Port::Top || srcPort == Port::Bottom) initialDir = 2;

    int startState = (startXi * numY + startYi) * 3 + initialDir;
    dist[startState] = 0.0;
    pq.push({startState, 0.0});

    const int dxs[4] = {1, -1, 0, 0};
    const int dys[4] = {0, 0, 1, -1};
    const int dirs[4] = {1, 1, 2, 2};

    const qreal BEND_PENALTY = 45.0;

    int finalEndState = -1;
    int iterations = 0;
    const int maxIterations = 5000;

    while (!pq.empty() && iterations++ < maxIterations)
    {
        PQNode top = pq.top();
        pq.pop();

        int u = top.state;
        if (top.cost > dist[u])
            continue;

        int curDir = u % 3;
        int tmp = u / 3;
        int curYi = tmp % numY;
        int curXi = tmp / numY;

        if (curXi == endXi && curYi == endYi)
        {
            finalEndState = u;
            break;
        }

        QPointF curPt(xCoords[curXi], yCoords[curYi]);

        for (int k = 0; k < 4; ++k)
        {
            int nextXi = curXi + dxs[k];
            int nextYi = curYi + dys[k];

            if (nextXi < 0 || nextXi >= numX || nextYi < 0 || nextYi >= numY)
                continue;

            QPointF nextPt(xCoords[nextXi], yCoords[nextYi]);

            bool collides = false;
            for (const auto& hardObs : hardObstacles)
            {
                if (segmentIntersectsRect(curPt, nextPt, hardObs))
                {
                    collides = true;
                    break;
                }
            }
            if (collides)
                continue;

            qreal stepDist = std::hypot(nextPt.x() - curPt.x(), nextPt.y() - curPt.y());
            int newDir = dirs[k];
            qreal penalty = (curDir != 0 && curDir != newDir) ? BEND_PENALTY : 0.0;

            for (const auto& obs : relevantObs)
            {
                QRectF softBox = obs.adjusted(-M + 2.0, -M + 2.0, M - 2.0, M - 2.0);
                if (segmentIntersectsRect(curPt, nextPt, softBox))
                {
                    penalty += stepDist * 0.4;
                    break;
                }
            }

            for (const auto& seg : existingEdgeSegments)
            {
                bool isHorizStep = (dirs[k] == 1);
                bool isHorizSeg = std::abs(seg.p1().y() - seg.p2().y()) < 1.0;

                if (isHorizStep && isHorizSeg && std::abs(curPt.y() - seg.p1().y()) < 8.0)
                {
                    qreal stepMinX = std::min(curPt.x(), nextPt.x());
                    qreal stepMaxX = std::max(curPt.x(), nextPt.x());
                    qreal segMinX = std::min(seg.p1().x(), seg.p2().x());
                    qreal segMaxX = std::max(seg.p1().x(), seg.p2().x());
                    if (std::max(stepMinX, segMinX) < std::min(stepMaxX, segMaxX) - 2.0)
                    {
                        penalty += 160.0;
                    }
                }

                bool isVertStep = (dirs[k] == 2);
                bool isVertSeg = std::abs(seg.p1().x() - seg.p2().x()) < 1.0;

                if (isVertStep && isVertSeg && std::abs(curPt.x() - seg.p1().x()) < 8.0)
                {
                    qreal stepMinY = std::min(curPt.y(), nextPt.y());
                    qreal stepMaxY = std::max(curPt.y(), nextPt.y());
                    qreal segMinY = std::min(seg.p1().y(), seg.p2().y());
                    qreal segMaxY = std::max(seg.p1().y(), seg.p2().y());
                    if (std::max(stepMinY, segMinY) < std::min(stepMaxY, segMaxY) - 2.0)
                    {
                        penalty += 160.0;
                    }
                }

                if ((isHorizStep && isVertSeg) || (isVertStep && isHorizSeg))
                {
                    QPointF intersectPt;
                    if (QLineF(curPt, nextPt).intersects(seg, &intersectPt) == QLineF::BoundedIntersection)
                    {
                        penalty += 75.0;
                    }
                }
            }

            qreal newCost = dist[u] + stepDist + penalty;
            int nextState = (nextXi * numY + nextYi) * 3 + newDir;

            if (newCost < dist[nextState])
            {
                dist[nextState] = newCost;
                cameFrom[nextState] = u;

                qreal h = std::abs(xCoords[nextXi] - xCoords[endXi]) +
                          std::abs(yCoords[nextYi] - yCoords[endYi]);
                pq.push({nextState, newCost + h});
            }
        }
    }

    std::vector<QPointF> rawWaypoints;
    rawWaypoints.push_back(srcPoint);
    rawWaypoints.push_back(startStub);

    if (finalEndState != -1)
    {
        std::vector<QPointF> astarPts;
        int curr = finalEndState;
        while (curr != -1 && curr != startState)
        {
            int tmp = curr / 3;
            int yi = tmp % numY;
            int xi = tmp / numY;
            astarPts.push_back(QPointF(xCoords[xi], yCoords[yi]));
            curr = cameFrom[curr];
        }
        std::reverse(astarPts.begin(), astarPts.end());
        for (const auto& p : astarPts)
        {
            rawWaypoints.push_back(p);
        }
    }
    else
    {
        if (srcPort == Port::Left || srcPort == Port::Right)
        {
            qreal midX = (startStub.x() + endStub.x()) / 2.0 + channelOffset;
            rawWaypoints.push_back(QPointF(midX, startStub.y()));
            rawWaypoints.push_back(QPointF(midX, endStub.y()));
        }
        else
        {
            qreal midY = (startStub.y() + endStub.y()) / 2.0 + channelOffset;
            rawWaypoints.push_back(QPointF(startStub.x(), midY));
            rawWaypoints.push_back(QPointF(endStub.x(), midY));
        }
    }

    rawWaypoints.push_back(endStub);
    rawWaypoints.push_back(dstPoint);

    std::vector<QPointF> waypoints;
    for (const auto& pt : rawWaypoints)
    {
        if (waypoints.empty())
        {
            waypoints.push_back(pt);
            continue;
        }
        if (std::hypot(pt.x() - waypoints.back().x(), pt.y() - waypoints.back().y()) < 0.5)
            continue;
        waypoints.push_back(pt);
    }

    bool changed = true;
    while (changed && waypoints.size() >= 3)
    {
        changed = false;
        for (size_t i = 1; i + 1 < waypoints.size(); ++i)
        {
            const QPointF& pPrev = waypoints[i - 1];
            const QPointF& pCurr = waypoints[i];
            const QPointF& pNext = waypoints[i + 1];

            bool colinX = (std::abs(pPrev.x() - pCurr.x()) < 1.0 && std::abs(pCurr.x() - pNext.x()) < 1.0);
            bool colinY = (std::abs(pPrev.y() - pCurr.y()) < 1.0 && std::abs(pCurr.y() - pNext.y()) < 1.0);

            if (colinX || colinY)
            {
                waypoints.erase(waypoints.begin() + i);
                changed = true;
                break;
            }
        }
    }

    if (waypoints.size() < 2)
    {
        waypoints = { srcPoint, dstPoint };
    }

    return waypoints;
}

QPainterPath GraphEdgeItem::buildRoundedPath(
    const std::vector<QPointF>& localWaypoints,
    qreal cornerRadius,
    qreal arrowRetractDist,
    const std::vector<QPointF>& localBridgeHops)
{
    QPainterPath path;
    if (localWaypoints.empty())
        return path;

    if (localWaypoints.size() == 1)
    {
        path.moveTo(localWaypoints[0]);
        path.lineTo(localWaypoints[0] + QPointF(1, 0));
        return path;
    }

    size_t n = localWaypoints.size();
    std::vector<QPointF> pts = localWaypoints;

    QPointF lastDiff = pts[n - 1] - pts[n - 2];
    qreal lastLen = std::hypot(lastDiff.x(), lastDiff.y());
    if (lastLen > arrowRetractDist + 1.0)
    {
        pts[n - 1] = pts[n - 2] + (lastDiff / lastLen) * (lastLen - arrowRetractDist);
    }

    const qreal hopRadius = 5.5;

    auto drawSegmentWithHops = [&](const QPointF& pA, const QPointF& pB) {
        bool isHoriz = std::abs(pA.y() - pB.y()) < 1.0;
        if (!isHoriz || localBridgeHops.empty() || !s_lineJumpsEnabled)
        {
            path.lineTo(pB);
            return;
        }

        qreal minX = std::min(pA.x(), pB.x());
        qreal maxX = std::max(pA.x(), pB.x());
        std::vector<qreal> hops;
        for (const auto& h : localBridgeHops)
        {
            if (std::abs(h.y() - pA.y()) < 2.5)
            {
                if (h.x() > minX + hopRadius + 3.0 && h.x() < maxX - hopRadius - 3.0)
                {
                    hops.push_back(h.x());
                }
            }
        }

        if (hops.empty())
        {
            path.lineTo(pB);
            return;
        }

        bool goingRight = pB.x() > pA.x();
        if (goingRight)
        {
            std::sort(hops.begin(), hops.end());
        }
        else
        {
            std::sort(hops.begin(), hops.end(), std::greater<qreal>());
        }

        std::vector<qreal> filtered;
        for (qreal hx : hops)
        {
            if (filtered.empty() || std::abs(hx - filtered.back()) >= hopRadius * 2.2)
            {
                filtered.push_back(hx);
            }
        }

        for (qreal hx : filtered)
        {
            if (goingRight)
            {
                path.lineTo(hx - hopRadius, pA.y());
                path.cubicTo(
                    QPointF(hx - hopRadius * 0.55, pA.y() - hopRadius * 1.35),
                    QPointF(hx + hopRadius * 0.55, pA.y() - hopRadius * 1.35),
                    QPointF(hx + hopRadius, pA.y()));
            }
            else
            {
                path.lineTo(hx + hopRadius, pA.y());
                path.cubicTo(
                    QPointF(hx + hopRadius * 0.55, pA.y() - hopRadius * 1.35),
                    QPointF(hx - hopRadius * 0.55, pA.y() - hopRadius * 1.35),
                    QPointF(hx - hopRadius, pA.y()));
            }
        }

        path.lineTo(pB);
    };

    if (n == 2)
    {
        path.moveTo(pts[0]);
        drawSegmentWithHops(pts[0], pts[1]);
        return path;
    }

    path.moveTo(pts[0]);
    QPointF currentPt = pts[0];

    for (size_t i = 1; i + 1 < n; ++i)
    {
        QPointF vIn = pts[i] - pts[i - 1];
        QPointF vOut = pts[i + 1] - pts[i];
        qreal lenIn = std::hypot(vIn.x(), vIn.y());
        qreal lenOut = std::hypot(vOut.x(), vOut.y());

        if (lenIn < 1.0 || lenOut < 1.0 || cornerRadius < 1.0)
        {
            drawSegmentWithHops(currentPt, pts[i]);
            currentPt = pts[i];
            continue;
        }

        qreal dot = (vIn.x() * vOut.x() + vIn.y() * vOut.y()) / (lenIn * lenOut);
        if (std::abs(dot) > 0.3)
        {
            drawSegmentWithHops(currentPt, pts[i]);
            currentPt = pts[i];
            continue;
        }

        qreal r = std::min({cornerRadius, lenIn / 2.0, lenOut / 2.0});
        if (r < 1.0)
        {
            drawSegmentWithHops(currentPt, pts[i]);
            currentPt = pts[i];
            continue;
        }

        QPointF startCorner = pts[i] - (vIn / lenIn) * r;
        QPointF endCorner = pts[i] + (vOut / lenOut) * r;

        drawSegmentWithHops(currentPt, startCorner);
        path.quadTo(pts[i], endCorner);
        currentPt = endCorner;
    }

    drawSegmentWithHops(currentPt, pts.back());
    if (path.elementCount() < 2)
    {
        path.clear();
        path.moveTo(pts.front());
        path.lineTo(pts.back());
    }
    return path;
}

void GraphEdgeItem::setCustomPath(const QPainterPath& localPath, const QPointF& arrowTip, double arrowAngle)
{
    prepareGeometryChange();
    cachedPath_ = localPath;
    cachedArrowTip_ = arrowTip;
    cachedArrowAngle_ = arrowAngle;

    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphEdgeState* State = isSelected() ? &theme.edge.selected
                                : (hovered_ ? &theme.edge.hover : &theme.edge.normal);

    qreal extraMargin = State->lineWidth + State->label.offset + State->label.paddingY + 30.0;
    cachedBounds_ = cachedPath_.boundingRect();
    cachedBounds_.adjust(-extraMargin, -extraMargin, extraMargin, extraMargin);
    update();
}

std::vector<QPointF> GraphEdgeItem::computeDirectOrthogonalPath(
    const QPointF& srcPoint, Port srcPort,
    const QPointF& dstPoint, Port dstPort,
    qreal channelOffset) const
{
    QPointF srcNormal(0, 0);
    switch (srcPort)
    {
        case Port::Right:  srcNormal = QPointF(1, 0); break;
        case Port::Left:   srcNormal = QPointF(-1, 0); break;
        case Port::Bottom: srcNormal = QPointF(0, 1); break;
        case Port::Top:    srcNormal = QPointF(0, -1); break;
    }

    QPointF dstNormal(0, 0);
    switch (dstPort)
    {
        case Port::Right:  dstNormal = QPointF(1, 0); break;
        case Port::Left:   dstNormal = QPointF(-1, 0); break;
        case Port::Bottom: dstNormal = QPointF(0, 1); break;
        case Port::Top:    dstNormal = QPointF(0, -1); break;
    }

    const qreal stubLen = 24.0;
    QPointF startStub = srcPoint + srcNormal * stubLen;
    QPointF endStub = dstPoint + dstNormal * stubLen;

    std::vector<QPointF> rawWaypoints;
    rawWaypoints.push_back(srcPoint);
    rawWaypoints.push_back(startStub);

    bool srcHoriz = (srcPort == Port::Left || srcPort == Port::Right);
    bool dstHoriz = (dstPort == Port::Left || dstPort == Port::Right);

    if (srcHoriz && dstHoriz)
    {
        bool facesForward = (srcPort == Port::Right && dstPort == Port::Left && endStub.x() >= startStub.x()) ||
                             (srcPort == Port::Left && dstPort == Port::Right && endStub.x() <= startStub.x());

        if (facesForward)
        {
            qreal midX = (startStub.x() + endStub.x()) / 2.0 + channelOffset;
            rawWaypoints.push_back(QPointF(midX, startStub.y()));
            rawWaypoints.push_back(QPointF(midX, endStub.y()));
        }
        else
        {
            qreal loopY;
            if (std::abs(startStub.y() - endStub.y()) < 30.0)
            {
                loopY = std::min(startStub.y(), endStub.y()) - 45.0 + channelOffset;
            }
            else
            {
                loopY = (startStub.y() + endStub.y()) / 2.0 + channelOffset;
            }
            rawWaypoints.push_back(QPointF(startStub.x(), loopY));
            rawWaypoints.push_back(QPointF(endStub.x(), loopY));
        }
    }
    else if (!srcHoriz && !dstHoriz)
    {
        bool facesForward = (srcPort == Port::Bottom && dstPort == Port::Top && endStub.y() >= startStub.y()) ||
                             (srcPort == Port::Top && dstPort == Port::Bottom && endStub.y() <= startStub.y());

        if (facesForward)
        {
            qreal midY = (startStub.y() + endStub.y()) / 2.0 + channelOffset;
            rawWaypoints.push_back(QPointF(startStub.x(), midY));
            rawWaypoints.push_back(QPointF(endStub.x(), midY));
        }
        else
        {
            qreal loopX;
            if (std::abs(startStub.x() - endStub.x()) < 30.0)
            {
                loopX = std::max(startStub.x(), endStub.x()) + 45.0 + channelOffset;
            }
            else
            {
                loopX = (startStub.x() + endStub.x()) / 2.0 + channelOffset;
            }
            rawWaypoints.push_back(QPointF(loopX, startStub.y()));
            rawWaypoints.push_back(QPointF(loopX, endStub.y()));
        }
    }
    else if (srcHoriz && !dstHoriz)
    {
        rawWaypoints.push_back(QPointF(endStub.x() + channelOffset, startStub.y()));
        rawWaypoints.push_back(QPointF(endStub.x() + channelOffset, endStub.y()));
    }
    else // !srcHoriz && dstHoriz
    {
        rawWaypoints.push_back(QPointF(startStub.x(), endStub.y() + channelOffset));
        rawWaypoints.push_back(QPointF(endStub.x(), endStub.y() + channelOffset));
    }

    rawWaypoints.push_back(endStub);
    rawWaypoints.push_back(dstPoint);

    std::vector<QPointF> waypoints;
    for (const auto& pt : rawWaypoints)
    {
        if (waypoints.empty()) { waypoints.push_back(pt); continue; }
        if (std::hypot(pt.x() - waypoints.back().x(), pt.y() - waypoints.back().y()) < 0.5)
            continue;
        waypoints.push_back(pt);
    }

    bool changed = true;
    while (changed && waypoints.size() >= 3)
    {
        changed = false;
        for (size_t i = 1; i + 1 < waypoints.size(); ++i)
        {
            const QPointF& pPrev = waypoints[i - 1];
            const QPointF& pCurr = waypoints[i];
            const QPointF& pNext = waypoints[i + 1];

            bool colinX = (std::abs(pPrev.x() - pCurr.x()) < 1.0 && std::abs(pCurr.x() - pNext.x()) < 1.0);
            bool colinY = (std::abs(pPrev.y() - pCurr.y()) < 1.0 && std::abs(pCurr.y() - pNext.y()) < 1.0);

            if (colinX || colinY)
            {
                waypoints.erase(waypoints.begin() + i);
                changed = true;
                break;
            }
        }
    }

    if (waypoints.size() < 2)
    {
        waypoints = { srcPoint, dstPoint };
    }

    return waypoints;
}

std::vector<QPointF> GraphEdgeItem::computeOctilinearPath(
    const QPointF& srcPoint, Port srcPort,
    const QPointF& dstPoint, Port dstPort,
    qreal channelOffset) const
{
    QPointF srcNormal(0, 0);
    switch (srcPort)
    {
        case Port::Right:  srcNormal = QPointF(1, 0); break;
        case Port::Left:   srcNormal = QPointF(-1, 0); break;
        case Port::Bottom: srcNormal = QPointF(0, 1); break;
        case Port::Top:    srcNormal = QPointF(0, -1); break;
    }

    QPointF dstNormal(0, 0);
    switch (dstPort)
    {
        case Port::Right:  dstNormal = QPointF(1, 0); break;
        case Port::Left:   dstNormal = QPointF(-1, 0); break;
        case Port::Bottom: dstNormal = QPointF(0, 1); break;
        case Port::Top:    dstNormal = QPointF(0, -1); break;
    }

    const qreal stubLen = 22.0;
    QPointF startStub = srcPoint + srcNormal * stubLen;
    QPointF endStub = dstPoint + dstNormal * stubLen;

    qreal dx = endStub.x() - startStub.x();
    qreal dy = endStub.y() - startStub.y();
    qreal adx = std::abs(dx);
    qreal ady = std::abs(dy);

    std::vector<QPointF> rawWaypoints;
    rawWaypoints.push_back(srcPoint);
    rawWaypoints.push_back(startStub);

    bool srcHoriz = (srcPort == Port::Left || srcPort == Port::Right);
    bool dstHoriz = (dstPort == Port::Left || dstPort == Port::Right);

    if (srcHoriz && dstHoriz && ((srcPort == Port::Right && dx > 0) || (srcPort == Port::Left && dx < 0)))
    {
        qreal diag = std::min(adx, ady);
        qreal leadX = (adx - diag) / 2.0;
        qreal sgnX = (dx >= 0) ? 1.0 : -1.0;
        qreal sgnY = (dy >= 0) ? 1.0 : -1.0;

        QPointF p1(startStub.x() + sgnX * leadX, startStub.y() + channelOffset);
        QPointF p2(p1.x() + sgnX * diag, p1.y() + sgnY * diag);

        rawWaypoints.push_back(p1);
        rawWaypoints.push_back(p2);
    }
    else if (!srcHoriz && !dstHoriz && ((srcPort == Port::Bottom && dy > 0) || (srcPort == Port::Top && dy < 0)))
    {
        qreal diag = std::min(adx, ady);
        qreal leadY = (ady - diag) / 2.0;
        qreal sgnX = (dx >= 0) ? 1.0 : -1.0;
        qreal sgnY = (dy >= 0) ? 1.0 : -1.0;

        QPointF p1(startStub.x() + channelOffset, startStub.y() + sgnY * leadY);
        QPointF p2(p1.x() + sgnX * diag, p1.y() + sgnY * diag);

        rawWaypoints.push_back(p1);
        rawWaypoints.push_back(p2);
    }
    else
    {
        QPointF corner(endStub.x() + channelOffset, startStub.y() + channelOffset);
        qreal chamfer = std::min(28.0, std::min(adx, ady) / 2.0);
        qreal sgnX = (dx >= 0) ? 1.0 : -1.0;
        qreal sgnY = (dy >= 0) ? 1.0 : -1.0;

        QPointF p1 = corner - QPointF(sgnX * chamfer, 0);
        QPointF p2 = corner + QPointF(0, sgnY * chamfer);

        rawWaypoints.push_back(p1);
        rawWaypoints.push_back(p2);
    }

    rawWaypoints.push_back(endStub);
    rawWaypoints.push_back(dstPoint);

    std::vector<QPointF> waypoints;
    for (const auto& pt : rawWaypoints)
    {
        if (waypoints.empty()) { waypoints.push_back(pt); continue; }
        if (std::hypot(pt.x() - waypoints.back().x(), pt.y() - waypoints.back().y()) < 0.5)
            continue;
        waypoints.push_back(pt);
    }

    if (waypoints.size() < 2)
    {
        waypoints = { srcPoint, dstPoint };
    }

    return waypoints;
}

std::vector<QPointF> GraphEdgeItem::computeBusHighwayPath(
    const QPointF& srcPoint, Port srcPort,
    const QPointF& dstPoint, Port dstPort,
    qreal channelOffset) const
{
    QPointF srcNormal(0, 0);
    switch (srcPort)
    {
        case Port::Right:  srcNormal = QPointF(1, 0); break;
        case Port::Left:   srcNormal = QPointF(-1, 0); break;
        case Port::Bottom: srcNormal = QPointF(0, 1); break;
        case Port::Top:    srcNormal = QPointF(0, -1); break;
    }

    QPointF dstNormal(0, 0);
    switch (dstPort)
    {
        case Port::Right:  dstNormal = QPointF(1, 0); break;
        case Port::Left:   dstNormal = QPointF(-1, 0); break;
        case Port::Bottom: dstNormal = QPointF(0, 1); break;
        case Port::Top:    dstNormal = QPointF(0, -1); break;
    }

    const qreal stubLen = 22.0;
    QPointF startStub = srcPoint + srcNormal * stubLen;
    QPointF endStub = dstPoint + dstNormal * stubLen;

    std::vector<QPointF> rawWaypoints;
    rawWaypoints.push_back(srcPoint);
    rawWaypoints.push_back(startStub);

    bool srcHoriz = (srcPort == Port::Left || srcPort == Port::Right);
    bool dstHoriz = (dstPort == Port::Left || dstPort == Port::Right);

    if (srcHoriz && dstHoriz)
    {
        qreal spineX = (startStub.x() + endStub.x()) / 2.0 + channelOffset;
        rawWaypoints.push_back(QPointF(spineX, startStub.y()));
        rawWaypoints.push_back(QPointF(spineX, endStub.y()));
    }
    else if (!srcHoriz && !dstHoriz)
    {
        qreal spineY = (startStub.y() + endStub.y()) / 2.0 + channelOffset;
        rawWaypoints.push_back(QPointF(startStub.x(), spineY));
        rawWaypoints.push_back(QPointF(endStub.x(), spineY));
    }
    else
    {
        qreal dx = std::abs(dstPoint.x() - srcPoint.x());
        qreal dy = std::abs(dstPoint.y() - srcPoint.y());
        if (dx >= dy)
        {
            qreal spineX = (startStub.x() + endStub.x()) / 2.0 + channelOffset;
            rawWaypoints.push_back(QPointF(spineX, startStub.y()));
            rawWaypoints.push_back(QPointF(spineX, endStub.y()));
        }
        else
        {
            qreal spineY = (startStub.y() + endStub.y()) / 2.0 + channelOffset;
            rawWaypoints.push_back(QPointF(startStub.x(), spineY));
            rawWaypoints.push_back(QPointF(endStub.x(), spineY));
        }
    }

    rawWaypoints.push_back(endStub);
    rawWaypoints.push_back(dstPoint);

    std::vector<QPointF> waypoints;
    for (const auto& pt : rawWaypoints)
    {
        if (waypoints.empty()) { waypoints.push_back(pt); continue; }
        if (std::hypot(pt.x() - waypoints.back().x(), pt.y() - waypoints.back().y()) < 0.5)
            continue;
        waypoints.push_back(pt);
    }

    if (waypoints.size() < 2)
    {
        waypoints = { srcPoint, dstPoint };
    }

    return waypoints;
}

void GraphEdgeItem::buildSmoothBezierPath(
    const QPointF& srcPoint, Port srcPort,
    const QPointF& dstPoint, Port dstPort,
    qreal channelOffset, qreal arrowWidth)
{
    QPointF srcNormal(0, 0);
    switch (srcPort)
    {
        case Port::Right:  srcNormal = QPointF(1, 0); break;
        case Port::Left:   srcNormal = QPointF(-1, 0); break;
        case Port::Bottom: srcNormal = QPointF(0, 1); break;
        case Port::Top:    srcNormal = QPointF(0, -1); break;
    }

    QPointF dstNormal(0, 0);
    switch (dstPort)
    {
        case Port::Right:  dstNormal = QPointF(1, 0); break;
        case Port::Left:   dstNormal = QPointF(-1, 0); break;
        case Port::Bottom: dstNormal = QPointF(0, 1); break;
        case Port::Top:    dstNormal = QPointF(0, -1); break;
    }

    qreal dx = dstPoint.x() - srcPoint.x();
    qreal dy = dstPoint.y() - srcPoint.y();
    qreal dist = std::hypot(dx, dy);

    qreal curvature = std::clamp(dist * 0.45, 40.0, 280.0);

    QPointF chordNormal(0, 0);
    if (dist > 1.0)
    {
        chordNormal = QPointF(-dy / dist, dx / dist);
    }

    QPointF cp1 = srcPoint + srcNormal * curvature + chordNormal * channelOffset;
    QPointF cp2 = dstPoint + dstNormal * curvature + chordNormal * channelOffset;

    QPointF approach = dstPoint - cp2;
    qreal appLen = std::hypot(approach.x(), approach.y());
    if (appLen < 1.0)
    {
        approach = -dstNormal;
        appLen = 1.0;
    }
    QPointF appUnit = approach / appLen;

    double arrowAngle = std::atan2(-approach.y(), approach.x());
    QPointF tip = mapFromScene(dstPoint);

    QPointF endPt = dstPoint - appUnit * arrowWidth;
    QPointF cp2Retracted = cp2 - appUnit * (arrowWidth * 0.4);

    QPainterPath path;
    path.moveTo(mapFromScene(srcPoint));
    path.cubicTo(mapFromScene(cp1), mapFromScene(cp2Retracted), mapFromScene(endPt));

    setCustomPath(path, tip, arrowAngle);
}

void GraphEdgeItem::buildStraightLinePath(
    const QPointF& srcPoint,
    const QPointF& dstPoint,
    qreal channelOffset, qreal arrowWidth)
{
    qreal dx = dstPoint.x() - srcPoint.x();
    qreal dy = dstPoint.y() - srcPoint.y();
    qreal dist = std::hypot(dx, dy);

    QPointF unitVec = (dist > 1.0) ? QPointF(dx / dist, dy / dist) : QPointF(1, 0);
    QPointF normalVec(-unitVec.y(), unitVec.x());

    QPointF pStart = srcPoint + normalVec * channelOffset;
    QPointF pEnd = dstPoint + normalVec * channelOffset;

    QPointF tip = mapFromScene(pEnd);
    double arrowAngle = std::atan2(-unitVec.y(), unitVec.x());

    QPointF pRetract = (dist > arrowWidth + 2.0) ? (pEnd - unitVec * arrowWidth) : pStart;

    QPainterPath path;
    path.moveTo(mapFromScene(pStart));
    path.lineTo(mapFromScene(pRetract));

    setCustomPath(path, tip, arrowAngle);
}

QPainterPath GraphEdgeItem::buildPath() const
{
    GraphNodeItem* sNode = effectiveSrcNode();
    GraphNodeItem* dNode = effectiveDstNode();
    if (!sNode || !dNode || sNode == dNode)
    {
        cachedArrowTip_ = QPointF();
        cachedArrowAngle_ = 0.0;
        return QPainterPath();
    }

    autoSelectPorts(srcPort_, dstPort_);

    QPointF srcPt = portScenePosition(sNode, srcPort_);
    QPointF dstPt = portScenePosition(dNode, dstPort_);

    qreal lane = 0.0;
    if (parallelCount_ > 1)
    {
        lane = parallelIndex_ - (parallelCount_ - 1) / 2.0;
    }
    qreal channelOffset = lane * 14.0;

    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphEdgeState* State = isSelected() ? &theme.edge.selected
                                : (hovered_ ? &theme.edge.hover : &theme.edge.normal);
    qreal arrowWidth = State->arrow.width;

    EdgeRoutingAlgorithm algo = effectiveRoutingAlgorithm();

    if (algo == EdgeRoutingAlgorithm::SmoothBezier)
    {
        const_cast<GraphEdgeItem*>(this)->buildSmoothBezierPath(srcPt, srcPort_, dstPt, dstPort_, channelOffset, arrowWidth);
        return cachedPath_;
    }

    if (algo == EdgeRoutingAlgorithm::StraightLine)
    {
        const_cast<GraphEdgeItem*>(this)->buildStraightLinePath(srcPt, dstPt, channelOffset, arrowWidth);
        return cachedPath_;
    }

    std::vector<QPointF> sceneWaypoints;
    if (algo == EdgeRoutingAlgorithm::SmartOrthogonal)
    {
        std::vector<QRectF> obstacles;
        if (sNode->scene())
        {
            for (QGraphicsItem* item : sNode->scene()->items())
            {
                auto* node = dynamic_cast<GraphNodeItem*>(item);
                if (!node || !node->isVisible() || node == sNode || node == dNode)
                    continue;

                bool isAnc = false;
                const QGraphicsItem* p1 = sNode->parentItem();
                while (p1) { if (p1 == node) { isAnc = true; break; } p1 = p1->parentItem(); }
                if (isAnc) continue;

                const QGraphicsItem* p2 = dNode->parentItem();
                while (p2) { if (p2 == node) { isAnc = true; break; } p2 = p2->parentItem(); }
                if (isAnc) continue;

                QRectF r = node->mapToScene(node->nodeRect()).boundingRect();
                if (r.width() > 0 && r.height() > 0)
                    obstacles.push_back(r);
            }
        }
        sceneWaypoints = computeObstacleFreePath(
            srcPt, srcPort_, dstPt, dstPort_, obstacles, channelOffset);
    }
    else if (algo == EdgeRoutingAlgorithm::DirectOrthogonal || algo == EdgeRoutingAlgorithm::CircuitBoard)
    {
        sceneWaypoints = computeDirectOrthogonalPath(srcPt, srcPort_, dstPt, dstPort_, channelOffset);
    }
    else if (algo == EdgeRoutingAlgorithm::Octilinear)
    {
        sceneWaypoints = computeOctilinearPath(srcPt, srcPort_, dstPt, dstPort_, channelOffset);
    }
    else if (algo == EdgeRoutingAlgorithm::BusHighway)
    {
        sceneWaypoints = computeBusHighwayPath(srcPt, srcPort_, dstPt, dstPort_, channelOffset);
    }

    if (sceneWaypoints.size() < 2)
    {
        sceneWaypoints = { srcPt, dstPt };
    }

    std::vector<QPointF> localWaypoints;
    localWaypoints.reserve(sceneWaypoints.size());
    for (const auto& pt : sceneWaypoints)
    {
        localWaypoints.push_back(mapFromScene(pt));
    }

    QPointF tip = localWaypoints.back();
    QPointF prev = localWaypoints[localWaypoints.size() - 2];
    double angle = std::atan2(-(tip.y() - prev.y()), tip.x() - prev.x());

    cachedArrowTip_ = tip;
    cachedArrowAngle_ = angle;

    qreal cornerRadius = (algo == EdgeRoutingAlgorithm::CircuitBoard) ? 0.0 : ((algo == EdgeRoutingAlgorithm::Octilinear) ? 4.0 : 8.0);
    QPainterPath path = buildRoundedPath(localWaypoints, cornerRadius, arrowWidth);
    if (path.elementCount() < 2)
    {
        path.clear();
        path.moveTo(localWaypoints.front());
        path.lineTo(localWaypoints.back());
    }
    return path;
}

void GraphEdgeItem::setWaypointsAndBuildPath(
    const std::vector<QPointF>& sceneWaypoints,
    const std::vector<QPointF>& sceneBridgeHops)
{
    std::vector<QPointF> validWaypoints = sceneWaypoints;
    if (validWaypoints.size() < 2)
    {
        GraphNodeItem* s = effectiveSrcNode();
        GraphNodeItem* d = effectiveDstNode();
        if (s && d && s != d)
        {
            Port sp = Port::Right, dp = Port::Left;
            autoSelectPorts(sp, dp);
            validWaypoints = { portScenePosition(s, sp), portScenePosition(d, dp) };
        }
    }
    if (validWaypoints.size() < 2)
    {
        return;
    }

    std::vector<QPointF> localWaypoints;
    localWaypoints.reserve(validWaypoints.size());
    for (const auto& pt : validWaypoints)
    {
        localWaypoints.push_back(mapFromScene(pt));
    }

    std::vector<QPointF> localBridgeHops;
    localBridgeHops.reserve(sceneBridgeHops.size());
    for (const auto& pt : sceneBridgeHops)
    {
        localBridgeHops.push_back(mapFromScene(pt));
    }

    const auto& theme = GraphThemeManager::instance()->theme();
    const GraphEdgeState* State = isSelected() ? &theme.edge.selected
                                : (hovered_ ? &theme.edge.hover : &theme.edge.normal);

    qreal arrowWidth = State->arrow.width;
    QPointF tip = localWaypoints.back();
    QPointF prev = localWaypoints[localWaypoints.size() - 2];
    double angle = std::atan2(-(tip.y() - prev.y()), tip.x() - prev.x());

    auto algo = effectiveRoutingAlgorithm();
    qreal cornerRadius = (algo == EdgeRoutingAlgorithm::CircuitBoard) ? 0.0 : ((algo == EdgeRoutingAlgorithm::Octilinear) ? 4.0 : 8.0);
    QPainterPath path = buildRoundedPath(localWaypoints, cornerRadius, arrowWidth, localBridgeHops);
    if (path.elementCount() < 2)
    {
        path.clear();
        path.moveTo(localWaypoints.front());
        path.lineTo(localWaypoints.back());
    }
    setCustomPath(path, tip, angle);
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

    if (cachedPath_.elementCount() < 2)
    {
        refreshPath();
        if (cachedPath_.elementCount() < 2)
        {
            Port sp = Port::Right, dp = Port::Left;
            autoSelectPorts(sp, dp);
            QPointF pA = mapFromScene(portScenePosition(sNode, sp));
            QPointF pB = mapFromScene(portScenePosition(dNode, dp));
            cachedPath_.clear();
            cachedPath_.moveTo(pA);
            cachedPath_.lineTo(pB);
            cachedArrowTip_ = pB;
            cachedArrowAngle_ = std::atan2(-(pB.y() - pA.y()), pB.x() - pA.x());
        }
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
    painter->drawPath(cachedPath_);

    // Draw Arrowhead at the calculated tip and orientation
    double angle = cachedArrowAngle_;
    double arrowWidth = arrow.width;
    double arrowHeight = arrow.height;
    QPointF arrowTip = cachedArrowTip_;

    QPointF arrowP1 = arrowTip - QPointF(
        std::cos(angle) * arrowWidth - std::sin(angle) * arrowHeight / 2.0,
        -std::sin(angle) * arrowWidth - std::cos(angle) * arrowHeight / 2.0);

    QPointF arrowP2 = arrowTip - QPointF(
        std::cos(angle) * arrowWidth + std::sin(angle) * arrowHeight / 2.0,
        -std::sin(angle) * arrowWidth + std::cos(angle) * arrowHeight / 2.0);

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
    QPointF p1 = cachedPath_.pointAtPercent(t);
    QPointF p2 = cachedPath_.pointAtPercent(t + 0.01);

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

    auto* routingSubMenu = menu.addMenu("Routing Style");
    auto* rGrp = new QActionGroup(&menu);

    auto saveRoutingMetadata = [this](std::optional<EdgeRoutingAlgorithm> optAlgo) {
        setRoutingAlgorithmOverride(optAlgo);
        if (model_)
        {
            auto opt = model_->getEdgeById(e_id);
            if (opt)
            {
                EdgeData edge = *opt;
                QJsonDocument metaDoc = QJsonDocument::fromJson(QString::fromStdString(edge.metadata).toUtf8());
                QJsonObject metaObj = metaDoc.isObject() ? metaDoc.object() : QJsonObject();
                if (!optAlgo.has_value()) {
                    metaObj.remove("routing");
                } else {
                    switch (*optAlgo) {
                        case EdgeRoutingAlgorithm::SmartOrthogonal:  metaObj["routing"] = "smart"; break;
                        case EdgeRoutingAlgorithm::DirectOrthogonal: metaObj["routing"] = "direct"; break;
                        case EdgeRoutingAlgorithm::CircuitBoard:     metaObj["routing"] = "circuit"; break;
                        case EdgeRoutingAlgorithm::SmoothBezier:     metaObj["routing"] = "bezier"; break;
                        case EdgeRoutingAlgorithm::StraightLine:     metaObj["routing"] = "straight"; break;
                        case EdgeRoutingAlgorithm::Octilinear:       metaObj["routing"] = "octilinear"; break;
                        case EdgeRoutingAlgorithm::BusHighway:        metaObj["routing"] = "bus"; break;
                    }
                }
                edge.metadata = QString::fromUtf8(QJsonDocument(metaObj).toJson(QJsonDocument::Compact)).toStdString();
                model_->updateEdge(edge);
            }
        }
        if (scene())
        {
            updateSceneEdges(scene());
            scene()->invalidate(QRectF(), QGraphicsScene::AllLayers);
        }
    };

    QString globalName = routingAlgorithmName(globalRoutingAlgorithm());
    auto* defAct = routingSubMenu->addAction(QString("Default (Global: %1)").arg(globalName));
    defAct->setCheckable(true);
    rGrp->addAction(defAct);
    if (!routingOverride_.has_value()) defAct->setChecked(true);
    connect(defAct, &QAction::triggered, this, [saveRoutingMetadata]() {
        saveRoutingMetadata(std::nullopt);
    });

    routingSubMenu->addSeparator();

    for (auto algo : availableRoutingAlgorithms())
    {
        auto* act = routingSubMenu->addAction(routingAlgorithmName(algo));
        act->setCheckable(true);
        rGrp->addAction(act);
        if (routingOverride_.has_value() && *routingOverride_ == algo)
            act->setChecked(true);
        connect(act, &QAction::triggered, this, [saveRoutingMetadata, algo]() {
            saveRoutingMetadata(algo);
        });
    }

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

        if (!edge->metadata.empty())
        {
            QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(edge->metadata).toUtf8());
            if (doc.isObject())
            {
                QJsonObject obj = doc.object();
                if (obj.contains("routing"))
                {
                    QString r = obj["routing"].toString();
                    if (r == "smart") routingOverride_ = EdgeRoutingAlgorithm::SmartOrthogonal;
                    else if (r == "direct") routingOverride_ = EdgeRoutingAlgorithm::DirectOrthogonal;
                    else if (r == "circuit") routingOverride_ = EdgeRoutingAlgorithm::CircuitBoard;
                    else if (r == "bezier") routingOverride_ = EdgeRoutingAlgorithm::SmoothBezier;
                    else if (r == "straight") routingOverride_ = EdgeRoutingAlgorithm::StraightLine;
                    else if (r == "octilinear") routingOverride_ = EdgeRoutingAlgorithm::Octilinear;
                    else if (r == "bus") routingOverride_ = EdgeRoutingAlgorithm::BusHighway;
                    else routingOverride_ = std::nullopt;
                }
                else
                {
                    routingOverride_ = std::nullopt;
                }
            }
        }
    }

    QFont font;
    font.setPointSize(theme.edge.normal.label.fontSize);
    font.setBold(theme.edge.normal.label.bold);
    QFontMetrics fm(font);
    cachedTitleRect_ = fm.boundingRect(cachedTitle_);

    refreshPath();
}