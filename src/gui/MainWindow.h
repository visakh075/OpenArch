#pragma once

#include <QMainWindow>
#include <QTreeView>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QStandardItemModel>

#include "core/ArchitectureModel.h"
#include "db/DbManagerSQLite.h"

enum class ItemType : int {
    Category = 0,
    Layer    = 1,
    Node     = 2
};

namespace NavRole {
    constexpr int Id   = Qt::UserRole + 1;
    constexpr int Type = Qt::UserRole + 2;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void openDatabase();
    void onTreeItemDoubleClicked(const QModelIndex& index);
    void onTreeItemClicked(const QModelIndex& index);

    void createNewNode();
    void createNewLayer();



private:
    void setupUi();
    void setupMenu();
    void setupConnections();
    void populateNavigator();
    void renderGraph(const GraphSnapshot& snap);

    QTreeView* navigator_{nullptr};
    QStandardItemModel* navModel_{nullptr};
    QGraphicsView* graphView_{nullptr};
    QGraphicsScene* scene_{nullptr};

    DbManagerSQLite db_;
    ArchitectureModel* model_{nullptr};
};