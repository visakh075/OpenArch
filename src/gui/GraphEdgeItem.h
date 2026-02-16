#ifndef GRAPHEDGEITEM_H
#define GRAPHEDGEITEM_H

#include <QGraphicsObject>
#include <QPen>
#include <QPainterPath>
#include <QPainterPathStroker>

#include "core/ArchitectureModel.h"

class GraphNodeItem;

class GraphEdgeItem : public QGraphicsObject
{
    Q_OBJECT

public:
    GraphEdgeItem(ArchitectureModel* model,
                  const EdgeData& edge,
                  GraphNodeItem* src,
                  GraphNodeItem* dst,
                  QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    QPainterPath shape() const override;

    void updateEndpoints();   // called when node moves

private:
    QPointF sourceCenter() const;
    QPointF destCenter() const;
    QPainterPath buildPath() const;

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    ArchitectureModel* model_;
    EdgeData edge_;

    GraphNodeItem* src_;
    GraphNodeItem* dst_;

    QPen normalPen_;
    QPen highlightPen_;

    bool hovered_ = false;
};

#endif
