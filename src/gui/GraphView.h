#pragma once

#include <QGraphicsView>
#include <QPointF>

class GraphNodeItem;

class GraphView : public QGraphicsView
{
    Q_OBJECT

public:
    enum class Mode {
        View,
        Add,
        Arch,
        Connect
    };

    explicit GraphView(QWidget* parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return mode_; }

signals:
    void requestAddNode(QPointF scenePos);
    void requestConnectNodes(qulonglong srcId, qulonglong dstId);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    Mode mode_ = Mode::View;
    GraphNodeItem* connectStartNode_ = nullptr;
};
