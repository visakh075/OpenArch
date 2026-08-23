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
        Layout,
        Add,
        Arch,
        Connect
    };
    enum class ExportMode
    {
        CurrentView,
        WholeScene
    };



    explicit GraphView(QWidget* parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return mode_; }
    void exportToSvg(ExportMode mode);
    void moveSelectionTo(const QPointF& target);
signals:
    void requestAddNode(QPointF scenePos);
    void requestConnectNodes(qulonglong srcId, qulonglong dstId);
    void deleteRequested();

protected:
    void drawBackground(
    QPainter* painter,
    const QRectF& rect) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    bool isPanning_ = false;
    bool spacePressed_ = false;
    QPoint lastPanPoint_;

    Mode mode_ = Mode::View;
    GraphNodeItem* connectStartNode_ = nullptr;
};
