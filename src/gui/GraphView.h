#pragma once

#include <QGraphicsView>
#include <QPointF>
#include <QContextMenuEvent>
#include <QPushButton>
#include <QButtonGroup>

class GraphNodeItem;

class GraphView : public QGraphicsView
{
    Q_OBJECT

public:
    enum class Mode {
        View,
        Edit,
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
    void exportToInteractiveHtml(const QString& filePath = QString());
    void updateOverlayShortcutHints();

signals:
    void modeChanged(GraphView::Mode mode);
    void requestAddNode(QPointF scenePos);
    void requestAddLayer();
    void requestConnectNodes(qulonglong srcId, qulonglong dstId);
    void deleteRequested();

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupModeOverlay();
    void updateOverlayPosition();
    void updateOverlayStyle();
    void updateOverlayActiveState();

    bool isPanning_{false};
    bool spacePressed_{false};
    QPoint lastPanPoint_;

    Mode mode_{Mode::View};

    QWidget* modeOverlay_{nullptr};
    QPushButton* btnView_{nullptr};
    QPushButton* btnEdit_{nullptr};
    QButtonGroup* modeButtonGroup_{nullptr};
};