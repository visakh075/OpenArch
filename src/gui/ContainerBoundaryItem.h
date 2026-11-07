#pragma once

#include <QGraphicsItem>
#include <QObject>
#include <QString>
#include "core/Types.h"

class ContainerBoundaryItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    explicit ContainerBoundaryItem(NodeId containerId, const QString& containerName,
                                   qreal width, qreal height, bool isLocked,
                                   QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void setDimensions(qreal width, qreal height, bool isLocked);
    QRectF boundaryRect() const { return rect_; }
    bool isLocked() const { return isLocked_; }

signals:
    void dimensionsChanged(qreal newWidth, qreal newHeight);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    QRectF resizeHandleRect() const;

    NodeId containerId_{0};
    QString containerName_;
    QRectF rect_{0, 0, 800, 600};
    bool isLocked_{false};

    bool isResizing_{false};
    QPointF resizeStartPos_;
    QSizeF resizeStartSize_;
};
