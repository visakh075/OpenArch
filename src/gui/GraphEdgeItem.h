#pragma once

#include <QGraphicsObject>
#include "core/ArchitectureModel.h"

class GraphEdgeItem : public QGraphicsObject {
    Q_OBJECT
public:
    GraphEdgeItem(ArchitectureModel* model,
                  const EdgeData& edge,
                  QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    void setEndpoints(const QPointF& src,
                      const QPointF& dst);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    ArchitectureModel* model_;
    EdgeId edgeId_;
    NodeId srcId_;
    NodeId dstId_;
    QString label_;

    QPointF src_;
    QPointF dst_;
    bool hovered_{false};
};
