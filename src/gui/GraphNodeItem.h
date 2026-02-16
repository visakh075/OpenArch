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
protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;
private:
    ArchitectureModel* model_;
    NodeId nodeId_;
    bool hovered_{false};
    std::unordered_set<GraphEdgeItem*> edges_;
    QString displayText() const;
};
