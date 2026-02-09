#pragma once

#include <QGraphicsObject>
#include "core/ArchitectureModel.h"

class GraphNodeItem : public QGraphicsObject {
    Q_OBJECT
public:
    GraphNodeItem(ArchitectureModel* model,
                  NodeId nodeId,
                  QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    ArchitectureModel* model_;
    NodeId nodeId_;
    bool hovered_{false};

    QString displayText() const;
};
