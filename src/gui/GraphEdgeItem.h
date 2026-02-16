#pragma once
#include <QGraphicsObject>
#include "core/ArchitectureModel.h"
class GraphNodeItem;
class GraphEdgeItem : public QGraphicsObject {
    Q_OBJECT
public:
    GraphEdgeItem(ArchitectureModel* model,const EdgeData& edge,
                  GraphNodeItem* src,GraphNodeItem* dst,QGraphicsItem* parent=nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
    void updateEndpoints();
protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;
private:
    ArchitectureModel* model_;
    EdgeId edgeId_;
    QString label_;
    GraphNodeItem* src_;
    GraphNodeItem* dst_;
    QPointF srcPos_, dstPos_;
    bool hovered_{false};
};
