#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"
#include <QPainter>
#include <QCursor>
static constexpr qreal NODE_WIDTH=140, NODE_HEIGHT=60, RADIUS=8;
GraphNodeItem::GraphNodeItem(ArchitectureModel* m, NodeId id, QGraphicsItem* p)
:QGraphicsObject(p),model_(m),nodeId_(id){
 setAcceptHoverEvents(true);
 setFlags(ItemIsMovable|ItemSendsGeometryChanges);
}
QRectF GraphNodeItem::boundingRect() const { return {0,0,NODE_WIDTH,NODE_HEIGHT}; }
QString GraphNodeItem::displayText() const{
 auto n=model_->getNodeById(nodeId_);
 if(!n) return "<deleted>";
 return QString::fromStdString(n->name)+"\n"+QString::fromStdString(n->type);
}
void GraphNodeItem::paint(QPainter* p,const QStyleOptionGraphicsItem*,QWidget*){
 p->setRenderHint(QPainter::Antialiasing);
 p->setPen(QPen(Qt::black, hovered_?2:1));
 p->setBrush(Qt::white);
 p->drawRoundedRect(boundingRect(),RADIUS,RADIUS);
 p->drawText(boundingRect().adjusted(6,6,-6,-6),Qt::AlignCenter,displayText());
}
void GraphNodeItem::addEdge(GraphEdgeItem* e){ edges_.insert(e); }
QVariant GraphNodeItem::itemChange(GraphicsItemChange c,const QVariant& v){
 if(c==ItemPositionHasChanged) for(auto* e:edges_) e->updateEndpoints();
 return QGraphicsObject::itemChange(c,v);
}
void GraphNodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent*){ hovered_=true; setCursor(QCursor(Qt::PointingHandCursor)); update();}
void GraphNodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*){ hovered_=false; unsetCursor(); update();}
