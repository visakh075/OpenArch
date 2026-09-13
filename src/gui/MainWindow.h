#pragma once

#include <QMainWindow>
#include <QTreeView>
#include <QGraphicsScene>
#include <QStandardItemModel>
#include <QToolBar>
#include <QActionGroup>
#include <QDockWidget>
#include <memory>
#include <optional>
#include <QStackedWidget>
#include "WelcomeWidget.h"

#include "ArchitectureModel.h"
#include "DbManager.h"
#include "DbManagerSQLite.h"
#include "DbManagerJson.h"
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
    explicit MainWindow(QWidget* parent = nullptr);
    void setDb(const std::string& db_path);
    ~MainWindow();

private:
    void setupUi();
    void setupMenu();
    void setupToolbar();
    void setupConnections();
    void populateNavigator();
    void renderGraph(const GraphSnapshot& snap);
    void saveLayout();
    void setGraphMode(GraphView::Mode mode);
    void handleAddNodeAtPosition(QPointF pos);
    void handleConnectNodes(qulonglong srcId, qulonglong dstId);
    void openDatabase();
    void createNewNode();
    void createNewLayer();
    void onTreeItemDoubleClicked(const QModelIndex& index);
    void onTreeItemClicked(const QModelIndex& index);

    void onSelectionChanged();
    void alignHorizontal();
    void alignVertical();
    void distributeHorizontal();
    void distributeVertical();

    enum class AlignType {
        Left,
        Right,
        CenterH,
        Top,
        Bottom,
        CenterV
    };

    void alignNodes(AlignType type);
    NodeId cloneNodeRecursive(NodeId sourceId, std::optional<NodeId> newParentId, qreal offsetX, qreal offsetY);

    QTreeView* navigator_{nullptr};
    QDockWidget* architectureDock_{nullptr};
    QStandardItemModel* navModel_{nullptr};

    QGraphicsScene* scene_{nullptr};
    GraphView* graphView_{nullptr};
    GraphNodeItem* primaryNode_{nullptr};

    bool isRendering_{false};

    QToolBar* graphToolBar_{nullptr};
    QToolBar* layoutToolBar_{nullptr};

    QAction* actionView_{nullptr};
    QAction* actionAdd_{nullptr};
    QAction* actionArch_{nullptr};
    QAction* actionConn_{nullptr};
    QAction* actionEdit_{nullptr};


    QAction* actionAlignLeft_{nullptr};
    QAction* actionAlignCenterH_{nullptr};
    QAction* actionAlignRight_{nullptr};

    QAction* actionAlignTop_{nullptr};
    QAction* actionAlignCenterV_{nullptr};
    QAction* actionAlignBottom_{nullptr};

    QAction* actionDistH_{nullptr};
    QAction* actionDistV_{nullptr};
    QAction* actionConnect_{nullptr};

    QStackedWidget* centralStack_{nullptr};
    WelcomeWidget* welcomeWidget_{nullptr};
    void showWelcome();
    void showCanvas();
    void createNewDatabase();


    // Dynamic backend: prevents SQLite / JSON overwrite conflicts
    std::unique_ptr<DbManager> db_{nullptr};
    ArchitectureModel* model_{nullptr};

private slots:
    void deleteSelected();
    void copySelectedNode();
    void connectSelectedNodes();
    void loadThemeFromFile();
    void switchThemePreset(const QString& path);

};