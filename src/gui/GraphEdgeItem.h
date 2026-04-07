#ifndef GRAPHEDGEITEM_H
#define GRAPHEDGEITEM_H

#include <QGraphicsObject>
#include <QPen>
#include <QPainterPath>

#include "core/ArchitectureModel.h"

class GraphNodeItem;

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

    GraphEdgeItem(ArchitectureModel* model,
                  const EdgeData& edge,
                  GraphNodeItem* src,
                  GraphNodeItem* dst,
                  QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem*,
               QWidget*) override;

    QPainterPath shape() const override;

    void updateEndpoints();

private:
    QPainterPath buildPath() const;

    QPointF portScenePosition(GraphNodeItem* node,
                              Port port) const;

    void autoSelectPorts(Port& srcPort,
                         Port& dstPort) const;

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    ArchitectureModel* model_;

    
    EdgeData edge_;

    GraphNodeItem* src_;
    GraphNodeItem* dst_;

    mutable Port srcPort_;
    mutable Port dstPort_;

    QPen normalPen_;
    QPen highlightPen_;

    bool hovered_ = false;
};

#endif
