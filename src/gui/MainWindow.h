#pragma once

#include <QMainWindow>
#include <QTreeView>
#include <QGraphicsScene>
#include <QStandardItemModel>
#include <QToolBar>
#include <QActionGroup>
#include <QDockWidget>
#include <QStackedWidget>
#include <QTimer>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <unordered_set>

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
    constexpr int Id      = Qt::UserRole + 1;
    constexpr int Type    = Qt::UserRole + 2;
    constexpr int Subtype = Qt::UserRole + 3;
}

class ArchitectureFilterProxyModel;
class QLineEdit;
class QComboBox;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void setDb(const std::string& db_path);
    bool hasClipboard() const { return !clipboardNodes_.empty() || !cutNodeIds_.empty(); }
    void pasteNodesAt(const std::optional<QPointF>& targetPos = std::nullopt);
    void populateNavigator();
    GraphView* graphView() const { return graphView_; }
    void exportToInteractiveHtml(const QString& filePath = QString());

public slots:
    void cutSelectedNodes();
    void copySelectedNodes();
    void copySelectedNode();
    void pasteNodes();

private slots:
    void saveLayout();
    void scheduleAutoSave();
    void loadThemeFromFile();
    void switchThemePreset(const QString& path);
    void onTreeItemDoubleClicked(const QModelIndex& index);
    void onTreeItemClicked(const QModelIndex& index);
    void onSelectionChanged();
    void handleAddNodeAtPosition(QPointF pos);
    void handleConnectNodes(qulonglong srcId, qulonglong dstId);
    void deleteSelected();
    void connectSelectedNodes();
    void openShortcutConfigDialog();
    void updateShortcutLabels();
    void openConvertDialog();
    void exportCurrentAsJson();
    void exportCurrentAsSqlite();

private:
    void setupUi();
    void setupMenu();
    void setupToolbar();
    void setupShortcuts();
    void setupConnections();
    void showWelcome();
    void showCanvas();
    void openDatabase();
    void createNewDatabase();
    void createNewNode();
    void createNewLayer();
    void renderGraph(const GraphSnapshot& snap);
    void setGraphMode(GraphView::Mode mode);

    enum class AlignType {
        Left,
        Right,
        CenterH,
        Top,
        Bottom,
        CenterV
    };
    void alignNodes(AlignType type);
    void alignHorizontal();
    void alignVertical();
    void distributeHorizontal();
    void distributeVertical();

    NodeId cloneNodeRecursive(NodeId sourceId,
                              std::optional<NodeId> newParentId,
                              qreal offsetX,
                              qreal offsetY);

    void updateNavStatus();

    QTreeView* navigator_{nullptr};
    QDockWidget* architectureDock_{nullptr};
    QStandardItemModel* navModel_{nullptr};
    ArchitectureFilterProxyModel* navProxyModel_{nullptr};
    QLineEdit* treeSearchEdit_{nullptr};
    QComboBox* treeFilterCombo_{nullptr};
    QLabel* treeStatusLabel_{nullptr};

    QGraphicsScene* scene_{nullptr};
    GraphView* graphView_{nullptr};
    GraphNodeItem* primaryNode_{nullptr};
    bool isRendering_{false};

    QStackedWidget* centralStack_{nullptr};
    WelcomeWidget* welcomeWidget_{nullptr};

    QToolBar* graphToolBar_{nullptr};
    QToolBar* layoutToolBar_{nullptr};

    QAction* actionView_{nullptr};
    QAction* actionEdit_{nullptr};
    QAction* actionConnect_{nullptr};
    QAction* actionCut_{nullptr};
    QAction* actionCopy_{nullptr};
    QAction* actionPaste_{nullptr};
    QAction* actionDuplicate_{nullptr};
    QAction* actionAddNode_{nullptr};
    QAction* actionAddLayer_{nullptr};
    QAction* actionSaveLayout_{nullptr};
    QAction* actionExportCurrent_{nullptr};
    QAction* actionExportWhole_{nullptr};
    QAction* actionExportHtml_{nullptr};
    QAction* actionConvert_{nullptr};
    QAction* actionExportJson_{nullptr};
    QAction* actionExportSqlite_{nullptr};
    QAction* duplicateBtn_{nullptr};

    std::string currentDbPath_;

    QAction* actionAlignLeft_{nullptr};
    QAction* actionAlignCenterH_{nullptr};
    QAction* actionAlignRight_{nullptr};
    QAction* actionAlignTop_{nullptr};
    QAction* actionAlignCenterV_{nullptr};
    QAction* actionAlignBottom_{nullptr};
    QAction* actionDistH_{nullptr};
    QAction* actionDistV_{nullptr};

    // --- Clipboard Data ---
    struct ClipboardNode {
        NodeData data;
        QPointF localPos;
        bool isRoot{false};
        std::vector<LayerId> layers;
    };
    std::vector<ClipboardNode> clipboardNodes_;
    int pasteOffsetMultiplier_{1};

    // --- Cut Buffer Data ---
    struct CutNodeRecord {
        NodeId id;
        QPointF originalScenePos;
        bool isRoot{false};
    };
    std::vector<CutNodeRecord> cutNodeIds_;

    QTimer* autoSaveTimer_{nullptr};
    std::unique_ptr<DbManager> db_{nullptr};
    ArchitectureModel* model_{nullptr};
};