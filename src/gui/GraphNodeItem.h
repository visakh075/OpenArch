#pragma once
#include <QGraphicsObject>
#include <unordered_set>
#include "core/ArchitectureModel.h"
class GraphEdgeItem;
class GraphNodeItem : public QGraphicsObject {
    Q_OBJECT
public:
    GraphNodeItem(ArchitectureModel* model, NodeId nodeId, QGraphicsItem* parent=nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
    void addEdge(GraphEdgeItem* edge);
    NodeId nodeId() const { return nodeId_; }
    QPointF currentPosition() const;
    QPointF center() const;
    QRectF rect() const;
    void setPrimary(bool p);
    bool isPrimary() const;
    QString displayText() const;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;
private:
    ArchitectureModel* model_;
    NodeId nodeId_;
    bool hovered_ = false;
    bool isPrimary_ = false;
    // QList<GraphEdgeItem*> edges_;
    std::unordered_set<GraphEdgeItem*> edges_;

};
