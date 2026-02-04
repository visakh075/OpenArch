
#pragma once

#include <QMainWindow>
#include <QTreeView>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QStandardItemModel>
#include <optional>

#include "core/ArchitectureModel.h"
#include "core/GraphSnapshot.h"
#include "db/DbManagerSQLite.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void openDatabase();
    void onTreeSelectionChanged(const QModelIndex& current);

private:
    void setupUi();
    void setupMenu();
    void populateNavigator();
    void renderGraph(const GraphSnapshot& snap);

    QTreeView* navigator_{nullptr};
    QStandardItemModel* navModel_{nullptr};
    QGraphicsView* graphView_{nullptr};
    QGraphicsScene* scene_{nullptr};

    DbManagerSQLite db_;
    ArchitectureModel* model_{nullptr};
};
