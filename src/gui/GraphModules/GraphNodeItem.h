#pragma once
#include <QGraphicsObject>
#include <QPointer>
#include <vector>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>

class GraphView;
#include "ArchitectureModel.h"

class GraphEdgeItem;

class GraphNodeItem : public QGraphicsObject {
    Q_OBJECT
public:
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    GraphNodeItem(ArchitectureModel* model, NodeId nodeId, QGraphicsItem* parent = nullptr);
    ~GraphNodeItem() override;

    QRectF boundingRect() const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
    void addEdge(GraphEdgeItem* edge);
    void removeEdge(GraphEdgeItem* edge);
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
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    
private:
    ArchitectureModel* model_;
    NodeId nodeId_;
    
    mutable QRectF cachedRect_;
    mutable QRectF cachedTitleBounds_;
    mutable QRectF cachedBodyBounds_;

    QRectF calculateNodeRect() const;
    void refreshGeometry();
    
    bool hovered_ = false;
    bool isPrimary_ = false;
    bool isConnecting_ = false;

    QGraphicsPathItem* tempPathItem_ = nullptr;
    QPainterPath buildPreviewPath(const QPointF& targetScenePos, GraphNodeItem* targetNode = nullptr) const;

    QGraphicsLineItem* tempLine_ = nullptr;
    std::vector<QPointer<GraphEdgeItem>> edges_;
};