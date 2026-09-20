#pragma once

#include <QGraphicsObject>
#include <QFont>
#include <QRectF>
#include <QPainterPath>
#include <vector>

#include "core/Types.h"
#include "core/ArchitectureModel.h"

class GraphEdgeItem;
class GraphView;
class QGraphicsPathItem;

class GraphNodeItem : public QGraphicsObject
{
    Q_OBJECT

public:
    enum class ContainerSizing { AutoFit, Manual };

    GraphNodeItem(ArchitectureModel *m, NodeId id, QGraphicsItem *p = nullptr);
    ~GraphNodeItem() override;

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    QPainterPath shape() const override;
    void refreshGeometry();

    QPointF currentPosition() const;
    QPointF center() const;
    QRectF rect() const;
    QRectF headerRect() const { return cachedHeaderRect_; }
    QRectF titleRect() const { return cachedTitleRect_; }
    QRectF bodyRect() const { return cachedBodyRect_; }

    NodeId nodeId() const { return nodeId_; }
    ArchitectureModel* model() const { return model_; }

    QString displayTitle() const;
    QString displayType() const;

    void setPrimary(bool p);
    bool isPrimary() const;
    void setEditable(bool enabled);

    bool isContainer() const;
    void adoptChild(GraphNodeItem* child);
    void releaseChild(GraphNodeItem* child);
    void setManualContainerSize(qreal w, qreal h);
    void setContainerSizing(ContainerSizing mode);

    void addEdge(GraphEdgeItem* e);
    void removeEdge(GraphEdgeItem* e);
    const std::vector<GraphEdgeItem*>& edges() const { return edges_; }

    void onThemeChanged();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    QPointF pressScenePos_;
    bool isDraggingNode_{false};
    QPointF dragStartScenePos_;

    QRectF calculateNodeRect();
    QRectF calculateContainerRect();
    QPainterPath buildPreviewPath(const QPointF& targetScenePos, GraphNodeItem* targetNode = nullptr) const;

    ArchitectureModel* model_{nullptr};
    NodeId nodeId_{0};
    std::vector<GraphEdgeItem*> edges_;

    bool hovered_{false};
    bool isPrimary_{false};

    // Edge creation interaction
    bool isConnecting_{false};
    QGraphicsPathItem* tempPathItem_{nullptr};

    // Container geometry
    ContainerSizing sizingMode_{ContainerSizing::AutoFit};
    qreal manualWidth_{320.0};
    qreal manualHeight_{220.0};
    QRectF cachedHeaderRect_;

    // Cached layout metrics
    QRectF cachedRect_;
    QRectF cachedTitleRect_;
    QRectF cachedBodyRect_;
    QFont  cachedTitleFont_;
    QFont  cachedBodyFont_;
};