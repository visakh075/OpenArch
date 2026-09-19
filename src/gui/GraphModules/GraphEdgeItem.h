#ifndef GRAPHEDGEITEM_H
#define GRAPHEDGEITEM_H

#include <QGraphicsObject>
#include <QPen>
#include <QPainterPath>
#include <QPointer>
#include <QGraphicsSceneContextMenuEvent>

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

    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    EdgeId edgeId() const { return e_id; }
    GraphEdgeItem(ArchitectureModel* model,
                  const EdgeId id,
                  GraphNodeItem* src,
                  GraphNodeItem* dst,
                  QGraphicsItem* parent = nullptr);
    virtual ~GraphEdgeItem() override;

    QRectF boundingRect() const override;
    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem*,
               QWidget*) override;

    QPainterPath shape() const override;

    void refreshLayout();
    void updateEndpoints();
    void refreshPath();
    const QPainterPath& edgePath() const { return cachedPath_; }
    QString title() const { return cachedTitle_; }

public slots:
    void onThemeChanged();

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    ArchitectureModel* model_{nullptr};
    EdgeId e_id{0};
    
    QPainterPath cachedPath_;
    QString cachedTitle_;
    QRect cachedTitleRect_;
    QRectF cachedBounds_;

    QPainterPath buildPath() const;
    QPointF portScenePosition(GraphNodeItem* node, Port port) const;
    void autoSelectPorts(Port& srcPort, Port& dstPort) const;

    QPointer<GraphNodeItem> src_;
    QPointer<GraphNodeItem> dst_;

    mutable Port srcPort_;
    mutable Port dstPort_;

    QPen normalPen_;
    QPen highlightPen_;

    bool hovered_{false};
};

#endif