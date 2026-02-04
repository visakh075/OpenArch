
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

#include "gui/LayerEditorWidget.h"
#include "gui/NodeEditorWidget.h"
#include "gui/RelationEditorWidget.h"
enum class TreeItemKind {
    Root = 0,
    Layer,
    Node
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void openDatabase();
    void onTreeSelectionChanged(const QModelIndex& current);

    // Layers
    void onSaveLayer(const LayerData& layer);
    void onAddNodeToLayer(const QString& nodeName);
    void onRemoveNodeFromLayer(const QString& nodeName);

    // Nodes
    void onSaveNode(const NodeData& node);
    void onAddLayerToNode(const QString& layerName);
    void onRemoveLayerFromNode(const QString& layerName);

private:
    void setupUi();
    void setupMenu();
    void setupConnections();
    void populateNavigator();

    void renderGraph(const GraphSnapshot& snap);

    void hideAllEditors();
    void showLayerEditor(const LayerData& layer);
    void showNodeEditor(const NodeData& node);

    void createNewLayer();
    void createNewNode();

    std::optional<LayerData> currentLayer_;
    std::optional<LayerData> currentLayer() const;

    std::optional<NodeData> currentNode_;

    QTreeView* navigator_{nullptr};
    QStandardItemModel* navModel_{nullptr};
    QGraphicsView* graphView_{nullptr};
    QGraphicsScene* scene_{nullptr};

    NodeEditorWidget*     nodeEditor_{nullptr};
    LayerEditorWidget*   layerEditor_{nullptr};
    RelationEditorWidget* relationEditor_{nullptr};

    DbManagerSQLite db_;
    ArchitectureModel* model_{nullptr};
};
