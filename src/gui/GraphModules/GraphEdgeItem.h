#ifndef GRAPHEDGEITEM_H
#define GRAPHEDGEITEM_H

#include <QGraphicsObject>
#include <QPen>
#include <QPainterPath>
#include <QPointer>
#include <QGraphicsSceneContextMenuEvent>
#include <QStringList>
#include <vector>
#include <optional>

#include "core/ArchitectureModel.h"

class GraphNodeItem;

enum class EdgeRoutingAlgorithm
{
    SmartOrthogonal = 0,   // Obstacle-avoiding orthogonal A* with rounded corners
    DirectOrthogonal = 1,  // Classic stepped Manhattan (L/Z step)
    SmoothBezier = 2,      // Smooth cubic Bezier spline
    StraightLine = 3,      // Direct straight vector
    Octilinear = 4,        // 45-degree chamfered / transit style
    BusHighway = 5,        // Central highway trunk routing
    CircuitBoard = 6       // Sharp 90-degree rectilinear PCB traces
};

class GraphEdgeItem : public QGraphicsObject
{
    Q_OBJECT

public:
    enum class Port
    {
        Top,
        Bottom,
        Left,
        Right
    };

    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    EdgeId edgeId() const { return e_id; }
    GraphEdgeItem(ArchitectureModel* model,
                  const EdgeId id,
                  GraphNodeItem* src,
                  GraphNodeItem* dst,
                  QGraphicsItem* parent = nullptr);
    virtual ~GraphEdgeItem() override;

    QRectF boundingRect() const override;
    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem*,
               QWidget*) override;

    QPainterPath shape() const override;

    void refreshLayout();
    void updateEndpoints();
    void refreshPath();
    const QPainterPath& edgePath() const { return cachedPath_; }
    QString title() const { return cachedTitle_; }
    const QRect& titleRect() const { return cachedTitleRect_; }
    ArchitectureModel* model() const { return model_; }
    GraphNodeItem* srcNode() const;
    GraphNodeItem* dstNode() const;
    GraphNodeItem* effectiveSrcNode() const;
    GraphNodeItem* effectiveDstNode() const;

    bool isConsolidated() const { return isConsolidated_; }
    const std::vector<EdgeId>& consolidatedEdgeIds() const { return consolidatedEdgeIds_; }
    void setConsolidatedEdges(const std::vector<GraphEdgeItem*>& edges);
    Status consolidatedStatus() const { return consolidatedStatus_; }
    bool consolidatedHasIssues() const { return consolidatedHasIssues_; }

    void setParallelInfo(int index, int count) { parallelIndex_ = index; parallelCount_ = count; }
    int parallelIndex() const { return parallelIndex_; }
    int parallelCount() const { return parallelCount_; }

    void setWaypointsAndBuildPath(
        const std::vector<QPointF>& sceneWaypoints,
        const std::vector<QPointF>& sceneBridgeHops = {});

    static EdgeRoutingAlgorithm globalRoutingAlgorithm();
    static void setGlobalRoutingAlgorithm(EdgeRoutingAlgorithm algo);
    static QString routingAlgorithmName(EdgeRoutingAlgorithm algo);
    static const std::vector<EdgeRoutingAlgorithm>& availableRoutingAlgorithms();

    static bool lineJumpsEnabled();
    static void setLineJumpsEnabled(bool enabled);

    std::optional<EdgeRoutingAlgorithm> routingAlgorithmOverride() const { return routingOverride_; }
    void setRoutingAlgorithmOverride(std::optional<EdgeRoutingAlgorithm> algo);
    EdgeRoutingAlgorithm effectiveRoutingAlgorithm() const;

    bool isHovered() const { return hovered_; }

    static void updateSceneEdges(QGraphicsScene* scene);

public slots:
    void onThemeChanged();

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    ArchitectureModel* model_{nullptr};
    EdgeId e_id{0};
    
    QPainterPath cachedPath_;
    QString cachedTitle_;
    QRect cachedTitleRect_;
    QRectF cachedBounds_;

    QPainterPath buildPath() const;
    QPointF portScenePosition(GraphNodeItem* node, Port port) const;
    void autoSelectPorts(Port& srcPort, Port& dstPort) const;

    std::vector<QPointF> computeObstacleFreePath(
        const QPointF& srcPoint, Port srcPort,
        const QPointF& dstPoint, Port dstPort,
        const std::vector<QRectF>& obstacles,
        qreal channelOffset,
        const std::vector<QLineF>& existingEdgeSegments = {}) const;

    static QPainterPath buildRoundedPath(
        const std::vector<QPointF>& localWaypoints,
        qreal cornerRadius,
        qreal arrowRetractDist,
        const std::vector<QPointF>& localBridgeHops = {});

    void setCustomPath(
        const QPainterPath& localPath,
        const QPointF& arrowTip,
        double arrowAngle);

    std::vector<QPointF> computeDirectOrthogonalPath(
        const QPointF& srcPoint, Port srcPort,
        const QPointF& dstPoint, Port dstPort,
        qreal channelOffset) const;

    std::vector<QPointF> computeOctilinearPath(
        const QPointF& srcPoint, Port srcPort,
        const QPointF& dstPoint, Port dstPort,
        qreal channelOffset) const;

    std::vector<QPointF> computeBusHighwayPath(
        const QPointF& srcPoint, Port srcPort,
        const QPointF& dstPoint, Port dstPort,
        qreal channelOffset) const;

    void buildSmoothBezierPath(
        const QPointF& srcPoint, Port srcPort,
        const QPointF& dstPoint, Port dstPort,
        qreal channelOffset, qreal arrowWidth);

    void buildStraightLinePath(
        const QPointF& srcPoint,
        const QPointF& dstPoint,
        qreal channelOffset, qreal arrowWidth);

    QPointer<GraphNodeItem> src_;
    QPointer<GraphNodeItem> dst_;

    mutable Port srcPort_;
    mutable Port dstPort_;

    QPen normalPen_;
    QPen highlightPen_;

    bool hovered_{false};

    int parallelIndex_{0};
    int parallelCount_{1};
    mutable QPointF cachedArrowTip_;
    mutable double cachedArrowAngle_{0.0};

    std::optional<EdgeRoutingAlgorithm> routingOverride_;
    static EdgeRoutingAlgorithm s_globalRoutingAlgorithm;
    static bool s_lineJumpsEnabled;

    bool isConsolidated_{false};
    std::vector<EdgeId> consolidatedEdgeIds_;
    QStringList consolidatedTypes_;
    Status consolidatedStatus_{Status::Approved};
    bool consolidatedHasIssues_{false};
};

#endif