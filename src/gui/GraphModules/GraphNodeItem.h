#pragma once
#include <QGraphicsObject>
#include <unordered_set>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>

class GraphView; // forward declaration

#include "ArchitectureModel.h"
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
    QString displayTitle() const;
    QString displayType() const;
    
    void setEditable(bool enabled);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    ArchitectureModel* model_;
    NodeId nodeId_;
    bool hovered_ = false;
    bool isPrimary_ = false;
    // QList<GraphEdgeItem*> edges_;
    std::unordered_set<GraphEdgeItem*> edges_;

};
