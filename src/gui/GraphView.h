#pragma once

#include <QGraphicsView>

class GraphView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit GraphView(QWidget* parent = nullptr);

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    double currentScale_ = 1.0;
    const double minScale_ = 0.2;
    const double maxScale_ = 4.0;
};
