#pragma once

#include <QMainWindow>
#include <QTreeView>
#include <QGraphicsScene>
#include <QStandardItemModel>
#include <QToolBar>
#include <QActionGroup>
#include <QDockWidget>

#include "ArchitectureModel.h"
#include "DbManagerSQLite.h"
#include "GraphView.h"

class GraphNodeItem;

enum class ItemType : int
{
    Category = 0,
    Layer    = 1,
    Node     = 2
};

namespace NavRole
{
    constexpr int Id   = Qt::UserRole + 1;
    constexpr int Type = Qt::UserRole + 2;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:

    explicit MainWindow(
        QWidget* parent = nullptr);

    void setDb(std::string);

    ~MainWindow();

private:

    void setupUi();

    void setupMenu();

    void setupToolbar();

    void setupConnections();

    void populateNavigator();

    void renderGraph(
        const GraphSnapshot& snap);

    void saveLayout();

    void setGraphMode(
        GraphView::Mode mode);

    void handleAddNodeAtPosition(
        QPointF pos);

    void handleConnectNodes(
        qulonglong srcId,
        qulonglong dstId);

    void openDatabase();

    void createNewNode();

    void createNewLayer();

    void onTreeItemDoubleClicked(
        const QModelIndex& index);

    void onTreeItemClicked(
        const QModelIndex& index);

    /*
     * DISPLAY
     */

    void onSelectionChanged();

    void alignHorizontal();

    void alignVertical();

private:

    /*
     * NAVIGATOR
     */

    QTreeView* navigator_{nullptr};

    QDockWidget* architectureDock_{nullptr};

    QStandardItemModel* navModel_{nullptr};

    /*
     * GRAPH
     */

    QGraphicsScene* scene_{nullptr};

    GraphView* graphView_{nullptr};

    GraphNodeItem* primaryNode_{nullptr};

    bool isRendering_{false};

    /*
     * TOOLBAR
     */

    QToolBar* graphToolBar_{nullptr};

    QAction* actionView_{nullptr};

    QAction* actionAdd_{nullptr};

    QAction* actionArch_{nullptr};

    QAction* actionConn_{nullptr};

    QAction* actionLayout_{nullptr};

    /*
     * DATA
     */

    DbManagerSQLite db_;

    ArchitectureModel* model_{nullptr};

    private slots:
    void deleteSelected();
};
