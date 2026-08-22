#ifndef GRAPHEDGEITEM_H
#define GRAPHEDGEITEM_H

#include <QGraphicsObject>
#include <QPen>
#include <QPainterPath>
#include <QPointer>

#include "ArchitectureModel.h"

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

    /* Added to identify the type of object when qgraphicsitem_cast is used */
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    GraphEdgeItem(ArchitectureModel* model,
                const EdgeId id,
                GraphNodeItem* src,
                GraphNodeItem* dst,
                QGraphicsItem* parent = nullptr);
    ~GraphEdgeItem();
    QRectF boundingRect() const override;
    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem*,
               QWidget*) override;

    QPainterPath shape() const override;

    void refreshLayout();
    void updateEndpoints();
    void refreshPath();

private:
    QPainterPath cachedPath_;
    QString cachedTitle_;
    QRect cachedTitleRect_;
    QRectF cachedBounds_;

    // QRectF cachedBounds_;
    QPainterPath buildPath() const;
    
    QPointF portScenePosition(GraphNodeItem* node,
                              Port port) const;

    void autoSelectPorts(Port& srcPort,
                         Port& dstPort) const;

    ArchitectureModel* model_;
    
    EdgeId e_id;

    // GraphNodeItem* src_;
    // GraphNodeItem* dst_;
    QPointer<GraphNodeItem> src_;
    QPointer<GraphNodeItem> dst_;

    mutable Port srcPort_;
    mutable Port dstPort_;

    QPen normalPen_;
    QPen highlightPen_;

    bool hovered_ = false;

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

};

#endif
