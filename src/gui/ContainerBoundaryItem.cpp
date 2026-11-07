#include "ContainerBoundaryItem.h"
#include "theme/ApplicationThemeManager.h"

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>

ContainerBoundaryItem::ContainerBoundaryItem(NodeId containerId, const QString& containerName,
                                             qreal width, qreal height, bool isLocked,
                                             QGraphicsItem* parent)
    : QGraphicsItem(parent),
      containerId_(containerId),
      containerName_(containerName),
      rect_(0, 0, std::max(200.0, width), std::max(150.0, height)),
      isLocked_(isLocked)
{
    setZValue(-50);
    setAcceptHoverEvents(true);
}

QRectF ContainerBoundaryItem::boundingRect() const
{
    return rect_.adjusted(-10, -10, 20, 20);
}

QRectF ContainerBoundaryItem::resizeHandleRect() const
{
    return QRectF(rect_.right() - 16, rect_.bottom() - 16, 16, 16);
}

void ContainerBoundaryItem::setDimensions(qreal width, qreal height, bool isLocked)
{
    prepareGeometryChange();
    rect_.setSize(QSizeF(std::max(200.0, width), std::max(150.0, height)));
    isLocked_ = isLocked;
    update();
}

void ContainerBoundaryItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    const auto colors = ApplicationThemeManager::currentThemeColors();

    painter->setRenderHint(QPainter::Antialiasing);

    QColor borderColor = colors.accent.isValid() ? colors.accent : QColor("#5e81ac");
    borderColor.setAlpha(140);

    QColor fillColor = colors.accent.isValid() ? colors.accent : QColor("#5e81ac");
    fillColor.setAlpha(colors.isDark ? 8 : 14);

    // Draw workspace fill & dashed boundary
    QPen borderPen(borderColor, 1.5, Qt::DashLine);
    borderPen.setDashPattern({6, 4});
    painter->setPen(borderPen);
    painter->setBrush(fillColor);
    painter->drawRoundedRect(rect_, 8.0, 8.0);

    // Draw Top-Left Workspace Badge
    QString badgeText = QString("📦 %1 Workspace: %2 × %3 px (%4)")
        .arg(containerName_)
        .arg(static_cast<int>(rect_.width()))
        .arg(static_cast<int>(rect_.height()))
        .arg(isLocked_ ? "Locked" : "Auto-Fit");

    QFont font = painter->font();
    font.setPointSize(9);
    font.setBold(true);
    painter->setFont(font);

    QFontMetrics fm(font);
    int textW = fm.horizontalAdvance(badgeText);
    QRectF badgeRect(rect_.left() + 10, rect_.top() + 8, textW + 16, 20);

    QColor badgeBg = colors.window;
    badgeBg.setAlpha(220);
    painter->setPen(QPen(borderColor, 1.0));
    painter->setBrush(badgeBg);
    painter->drawRoundedRect(badgeRect, 4.0, 4.0);

    painter->setPen(colors.text);
    painter->drawText(badgeRect, Qt::AlignCenter, badgeText);

    // Draw Bottom-Right Resize Grip Handle [ ◢ ]
    QColor gripColor = colors.accent.isValid() ? colors.accent : QColor("#88c0d0");
    if (isResizing_) gripColor = gripColor.lighter(130);

    painter->setPen(QPen(gripColor, 1.5));
    // Draw 3 diagonal lines in corner
    qreal r = rect_.right() - 4;
    qreal b = rect_.bottom() - 4;
    painter->drawLine(QPointF(r - 10, b), QPointF(r, b - 10));
    painter->drawLine(QPointF(r - 6, b), QPointF(r, b - 6));
    painter->drawLine(QPointF(r - 2, b), QPointF(r, b - 2));
}

void ContainerBoundaryItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && resizeHandleRect().contains(event->pos()))
    {
        isResizing_ = true;
        resizeStartPos_ = event->scenePos();
        resizeStartSize_ = rect_.size();
        event->accept();
        return;
    }
    QGraphicsItem::mousePressEvent(event);
}

void ContainerBoundaryItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (isResizing_)
    {
        QPointF delta = event->scenePos() - resizeStartPos_;
        qreal newW = std::max(200.0, resizeStartSize_.width() + delta.x());
        qreal newH = std::max(150.0, resizeStartSize_.height() + delta.y());

        prepareGeometryChange();
        rect_.setSize(QSizeF(newW, newH));
        update();
        event->accept();
        return;
    }
    QGraphicsItem::mouseMoveEvent(event);
}

void ContainerBoundaryItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (isResizing_)
    {
        isResizing_ = false;
        emit dimensionsChanged(rect_.width(), rect_.height());
        event->accept();
        return;
    }
    QGraphicsItem::mouseReleaseEvent(event);
}

void ContainerBoundaryItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    if (resizeHandleRect().contains(event->pos()))
    {
        setCursor(Qt::SizeFDiagCursor);
    }
    else
    {
        setCursor(Qt::ArrowCursor);
    }
    QGraphicsItem::hoverMoveEvent(event);
}
