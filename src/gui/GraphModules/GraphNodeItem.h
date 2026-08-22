#pragma once
#include <QGraphicsObject>
#include <QPointer>
#include <qpaintdevice.h>
#include <vector>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>

class GraphView;

#include "ArchitectureModel.h"
class GraphEdgeItem;
class GraphNodeItem : public QGraphicsObject {
    Q_OBJECT
public:
    /* Added to identify the type of object when qgraphicsitem_cast is used */
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    GraphNodeItem(ArchitectureModel* model, NodeId nodeId, QGraphicsItem* parent=nullptr);
    ~GraphNodeItem();

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
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    
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
    // std::unordered_set<GraphEdgeItem*> edges_;
    // std::unordered_set<QPointer<GraphEdgeItem>> edges_;
    std::vector<QPointer<GraphEdgeItem>> edges_;

};
