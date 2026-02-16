#include "GraphEdgeItem.h"
#include "GraphNodeItem.h"
#include <QPainter>
#include <QCursor>
#include <QtMath>
GraphEdgeItem::GraphEdgeItem(ArchitectureModel* m,const EdgeData& e,GraphNodeItem* s,GraphNodeItem* d,QGraphicsItem* p)
:QGraphicsObject(p),model_(m),edgeId_(e.id),label_(QString::fromStdString(e.edgeType)),src_(s),dst_(d){
 setAcceptHoverEvents(true); setZValue(-1);
 src_->addEdge(this); dst_->addEdge(this);
 updateEndpoints();
}
void GraphEdgeItem::updateEndpoints(){
 prepareGeometryChange();
 srcPos_=src_->sceneBoundingRect().center();
 dstPos_=dst_->sceneBoundingRect().center();
}
QRectF GraphEdgeItem::boundingRect() const{
 return QRectF(srcPos_,dstPos_).normalized().adjusted(-20,-20,20,20);
}
void GraphEdgeItem::paint(QPainter* p,const QStyleOptionGraphicsItem*,QWidget*){
 p->setRenderHint(QPainter::Antialiasing);
 QPen pen(hovered_?Qt::darkBlue:Qt::black,1.5);
 p->setPen(pen);
 p->drawLine(srcPos_,dstPos_);
 QLineF l(srcPos_,dstPos_);
 double a=std::atan2(-l.dy(),l.dx());
 constexpr qreal A=10;
 QPointF p1=dstPos_-QPointF(std::cos(a+M_PI/6)*A,-std::sin(a+M_PI/6)*A);
 QPointF p2=dstPos_-QPointF(std::cos(a-M_PI/6)*A,-std::sin(a-M_PI/6)*A);
 p->setBrush(pen.color());
 p->drawPolygon(QPolygonF()<<dstPos_<<p1<<p2);
 p->drawText((srcPos_+dstPos_)/2+QPointF(4,-4),label_);
}
void GraphEdgeItem::hoverEnterEvent(QGraphicsSceneHoverEvent*){ hovered_=true; setCursor(QCursor(Qt::PointingHandCursor)); update();}
void GraphEdgeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*){ hovered_=false; unsetCursor(); update();}
