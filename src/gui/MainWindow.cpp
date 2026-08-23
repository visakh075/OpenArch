#include "MainWindow.h"

#include <QSplitter>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMessageBox>
#include <QShortcut>
#include "NodeEditorDialog.h"
#include "LayerEditorDialog.h"
#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"
#include "GraphThemeManager.h"
#include "ThemeEditorDock.h"
#include <QInputDialog>
#include <unordered_set>
#include <QScrollBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenu();
    setupToolbar();
    setupConnections();

    
}

MainWindow::~MainWindow()
{
    // Disconnect scene signals before teardown so selection changes don't fire during item destruction
    if (scene_)
        scene_->disconnect(this);

    delete model_;
}

void MainWindow::setupUi()
{
    /*
     * NAVIGATION MODEL
     */

    navModel_ =
        new QStandardItemModel(this);

    navModel_->setHorizontalHeaderLabels(
        {"Architecture"});

    /*
     * NAVIGATOR
     */

    navigator_ =
        new QTreeView;

    navigator_->setModel(
        navModel_);

    /*
     * ARCHITECTURE DOCK
     */

    architectureDock_ =
        new QDockWidget(
            "Architecture",
            this);

    architectureDock_->setObjectName(
        "ArchitectureDock");

    architectureDock_->setWidget(
        navigator_);

    addDockWidget(
        Qt::LeftDockWidgetArea,
        architectureDock_);

    /*
     * GRAPH SCENE
     */

    scene_ =
        new QGraphicsScene(this);

    /*
     * GRAPH VIEW
     */

    graphView_ =
        new GraphView(this);

    graphView_->setScene(
        scene_);

    graphView_->setViewportUpdateMode(
        QGraphicsView::FullViewportUpdate);

    graphView_->setRenderHint(
        QPainter::Antialiasing);

    graphView_->setInteractive(true);

    /*
     * CENTRAL WIDGET
     */

    setCentralWidget(
        graphView_);

    /*
     * THEME EDITOR
     */

    ThemeEditorDock* themeDock =
        new ThemeEditorDock(this);

    themeDock->setObjectName(
        "ThemeDock");

    addDockWidget(
        Qt::RightDockWidgetArea,
        themeDock);

    /*
     * LIVE THEME UPDATE
     */

    connect(
        GraphThemeManager::instance(),
        &GraphThemeManager::themeChanged,
        this,
        [this]()
        {
            scene_->update();
        });

    /*
     * DOCK CONFIG
     */

    setDockNestingEnabled(true);
}

void MainWindow::setupToolbar()
{
    graphToolBar_ = addToolBar("Graph Modes");

    actionView_ = graphToolBar_->addAction("View (V)");
    actionEdit_ = graphToolBar_->addAction("Edit (E)");    
    // actionAdd_  = graphToolBar_->addAction("Add (A)");
    // actionArch_ = graphToolBar_->addAction("Arch (R)");
    // actionConn_ = graphToolBar_->addAction("Connect (C)");

    actionView_->setCheckable(true);
    // actionAdd_->setCheckable(true);
    // actionArch_->setCheckable(true);
    // actionConn_->setCheckable(true);
    actionEdit_->setCheckable(true); 

    QActionGroup* group = new QActionGroup(this);
    group->addAction(actionView_);
    // group->addAction(actionAdd_);
    // group->addAction(actionArch_);
    // group->addAction(actionConn_);
    group->addAction(actionEdit_);

    actionView_->setChecked(true);

    connect(actionView_, &QAction::triggered,
            this, [this]() { setGraphMode(GraphView::Mode::View); });

    connect(actionEdit_, &QAction::triggered,
        this, [this]() { setGraphMode(GraphView::Mode::Edit); });
    // connect(actionAdd_, &QAction::triggered,
    //         this, [this]() { setGraphMode(GraphView::Mode::Add); });

    // connect(actionArch_, &QAction::triggered,
    //         this, [this]() { setGraphMode(GraphView::Mode::Arch); });

    // connect(actionConn_, &QAction::triggered,
            // this, [this]() { setGraphMode(GraphView::Mode::Connect); });
    

    actionView_->setShortcut(Qt::Key_V);
    actionEdit_->setShortcut(Qt::Key_L);
    // actionAdd_->setShortcut(Qt::Key_A);
    // actionArch_->setShortcut(Qt::Key_R);
    // actionConn_->setShortcut(Qt::Key_C);
}

void MainWindow::setupMenu()
{
    auto* fileMenu = menuBar()->addMenu("&File");

    fileMenu->addAction("Open DB", this, &MainWindow::openDatabase);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction("Add Node", this, &MainWindow::createNewNode);
    editMenu->addAction("Add Layer", this, &MainWindow::createNewLayer);
    editMenu->addAction("Save Layout", this, &MainWindow::saveLayout);

    editMenu->addSeparator();

    auto* alignH = editMenu->addAction("Align Horizontal", this, &MainWindow::alignHorizontal);
    alignH->setShortcut(Qt::CTRL | Qt::Key_H);

    auto* alignV = editMenu->addAction("Align Vertical", this, &MainWindow::alignVertical);
    alignV->setShortcut(Qt::CTRL | Qt::Key_V);

    QAction* exportCurrentAction =
    new QAction(
        "Export Current View",
        this);

    connect(
        exportCurrentAction,
        &QAction::triggered,
        this,
        [this]()
        {
            graphView_->exportToSvg(
                GraphView::ExportMode::CurrentView);
        });

    fileMenu->addAction(exportCurrentAction);

    QAction* exportWholeAction =
    new QAction(
        "Export Whole Diagram",
        this);

    connect(
        exportWholeAction,
        &QAction::triggered,
        this,
        [this]()
        {
            graphView_->exportToSvg(
                GraphView::ExportMode::WholeScene);
        });

    fileMenu->addAction(exportWholeAction);
    

    QAction* moveAction =
    new QAction(
        "Move Selection To...",
        this);

    connect(
    moveAction,
    &QAction::triggered,
    this,
    [this]()
    {
        bool okX = false;
        bool okY = false;

        double x =
            QInputDialog::getDouble(
                this,
                "Move Selection",
                "Target X:",
                0.0,
                -100000,
                100000,
                2,
                &okX);

        if (!okX)
            return;

        double y =
            QInputDialog::getDouble(
                this,
                "Move Selection",
                "Target Y:",
                0.0,
                -100000,
                100000,
                2,
                &okY);

        if (!okY)
            return;

        graphView_->moveSelectionTo(
            QPointF(x, y));
    });

        editMenu->addAction(moveAction);
}

void MainWindow::setupConnections()
{
    connect(navigator_, &QTreeView::doubleClicked,
            this, &MainWindow::onTreeItemDoubleClicked);

    connect(navigator_, &QTreeView::clicked,
            this, &MainWindow::onTreeItemClicked);

    connect(graphView_, &GraphView::requestAddNode,
            this, &MainWindow::handleAddNodeAtPosition);

    connect(graphView_, &GraphView::requestAddLayer,
            this, &MainWindow::createNewLayer);

    connect(graphView_, &GraphView::requestConnectNodes,
            this, &MainWindow::handleConnectNodes);

    connect(graphView_, &GraphView::deleteRequested,
            this, &MainWindow::deleteSelected);

    connect(scene_, &QGraphicsScene::selectionChanged,
            this, &MainWindow::onSelectionChanged);

    connect(GraphThemeManager::instance(),
            &GraphThemeManager::themeChanged,
            scene_,
            [this]() { scene_->update(); });

    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::deleteSelected);

    auto* backspaceShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    connect(backspaceShortcut, &QShortcut::activated, this, &MainWindow::deleteSelected);
}

void MainWindow::openDatabase()
{
    QString file = QFileDialog::getOpenFileName(
        this, "Open DB", "", "SQLite DB (*.db)");

    if (file.isEmpty())
        return;

    db_.close();
    auto r = db_.open(file.toStdString());
    if (!r.ok) {
        QMessageBox::critical(this, "Error",
                              QString::fromStdString(r.message));
        return;
    }

    delete model_;
    model_ = new ArchitectureModel(db_);

    populateNavigator();
    renderGraph(model_->extractGraph(std::nullopt));

}

void MainWindow::populateNavigator()
{
    navModel_->clear();
    navModel_->setHorizontalHeaderLabels({"Architecture"});

    auto* root = navModel_->invisibleRootItem();

    auto* nodesRoot = new QStandardItem("Nodes");
    nodesRoot->setData(static_cast<int>(ItemType::Category), NavRole::Type);

    for (const auto& n : model_->nodes()) {
        auto* item = new QStandardItem(QString::fromStdString(n.name));
        item->setData(static_cast<qulonglong>(n.id), NavRole::Id);
        item->setData(static_cast<int>(ItemType::Node), NavRole::Type);
        nodesRoot->appendRow(item);
    }

    auto* layersRoot = new QStandardItem("Layers");
    layersRoot->setData(static_cast<int>(ItemType::Category), NavRole::Type);

    for (const auto& l : model_->layers()) {
        auto* item = new QStandardItem(QString::fromStdString(l.name));
        item->setData(static_cast<qulonglong>(l.id), NavRole::Id);
        item->setData(static_cast<int>(ItemType::Layer), NavRole::Type);
        layersRoot->appendRow(item);
    }

    root->appendRow(nodesRoot);
    root->appendRow(layersRoot);
    navigator_->expandAll();
}

void MainWindow::onTreeItemDoubleClicked(const QModelIndex& index)
{
    if (!model_)
        return;

    auto* item = navModel_->itemFromIndex(index);
    if (!item)
        return;

    ItemType type =
        static_cast<ItemType>(item->data(NavRole::Type).toInt());

    qulonglong id =
        item->data(NavRole::Id).toULongLong();

    switch (type) {

    case ItemType::Node: {
        NodeEditorDialog dlg(model_, id, this);
        dlg.exec();

        populateNavigator();
        renderGraph(model_->extractGraph(std::nullopt));
        break;
    }

    case ItemType::Layer: {
        LayerEditorDialog dlg(model_, id, this);
        dlg.exec();

        populateNavigator();
        renderGraph(model_->extractGraph(
            static_cast<LayerId>(id)));
        break;
    }

    default:
        renderGraph(model_->extractGraph(std::nullopt));
        break;
    }
}

void MainWindow::onTreeItemClicked(const QModelIndex& index)
{
    if (!model_ || !index.isValid())
        return;

    auto* item = navModel_->itemFromIndex(index);
    if (!item)
        return;

    ItemType type =
        static_cast<ItemType>(item->data(NavRole::Type).toInt());

    if (type == ItemType::Layer) {
        LayerId layerId =
            static_cast<LayerId>(item->data(NavRole::Id).toULongLong());

        renderGraph(model_->extractGraph(layerId));
    }
    else {
        // Node or Category → full graph
        renderGraph(model_->extractGraph(std::nullopt));
    }
}

void MainWindow::createNewNode()
{
    if (!model_)
        return;

    NodeEditorDialog dlg(model_, /* nodeId */ 0, this);
    dlg.exec();

    populateNavigator();
    renderGraph(model_->extractGraph(std::nullopt));
}

void MainWindow::createNewLayer()
{
    if (!model_)
        return;

    LayerEditorDialog dlg(model_, /* layerId */ 0, this);
    dlg.exec();

    populateNavigator();
    renderGraph(model_->extractGraph(std::nullopt));
}

void MainWindow::renderGraph(const GraphSnapshot& snap)
{
    if (!model_ || !scene_ || !graphView_)
        return;

    // 1. Record EXACT scrollbar positions (no coordinate mapping drift)
    int hVal = graphView_->horizontalScrollBar()->value();
    int vVal = graphView_->verticalScrollBar()->value();

    isRendering_ = true;
    scene_->blockSignals(true);
    graphView_->setUpdatesEnabled(false);

    // 2. Index existing scene items
    std::unordered_map<NodeId, GraphNodeItem*> existingNodes;
    std::unordered_map<EdgeId, GraphEdgeItem*> existingEdges;

    for (QGraphicsItem* item : scene_->items())
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            existingNodes[node->nodeId()] = node;
        else if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
            existingEdges[edge->edgeId()] = edge;
    }

    // 3. Reconcile Nodes
    std::unordered_set<NodeId> snapshotNodeIds;
    std::unordered_map<NodeId, GraphNodeItem*> currentNodes;

    int i = 0;
    for (const auto& n : snap.nodes)
    {
        snapshotNodeIds.insert(n.id);
        GraphNodeItem* nodeItem = nullptr;

        auto it = existingNodes.find(n.id);
        if (it != existingNodes.end())
        {
            nodeItem = it->second;
            existingNodes.erase(it);
        }
        else
        {
            nodeItem = new GraphNodeItem(model_, n.id);

            auto mode = graphView_->mode();
            bool selectable = (mode == GraphView::Mode::Edit || mode == GraphView::Mode::Arch);
            bool movable    = (mode == GraphView::Mode::Edit);

            nodeItem->setFlag(QGraphicsItem::ItemIsSelectable, selectable);
            nodeItem->setFlag(QGraphicsItem::ItemIsMovable, movable);
            nodeItem->setAcceptHoverEvents(true);

            auto nodeOpt = model_->getNodeById(n.id);
            bool restored = false;

            if (nodeOpt && !nodeOpt->metadata.empty())
            {
                QJsonDocument doc = QJsonDocument::fromJson(
                    QString::fromStdString(nodeOpt->metadata).toUtf8());

                if (doc.isObject())
                {
                    QJsonObject obj = doc.object();
                    if (obj.contains("x") && obj.contains("y"))
                    {
                        nodeItem->setPos(obj["x"].toDouble(), obj["y"].toDouble());
                        restored = true;
                    }
                }
            }

            if (!restored)
                nodeItem->setPos((i % 5) * 180, (i / 5) * 120);

            scene_->addItem(nodeItem);
        }

        currentNodes[n.id] = nodeItem;
        ++i;
    }

    // 4. Reconcile Edges
    std::unordered_set<EdgeId> snapshotEdgeIds;
    for (const auto& e : snap.edges)
    {
        snapshotEdgeIds.insert(e.id);

        auto it = existingEdges.find(e.id);
        if (it != existingEdges.end())
        {
            it->second->updateEndpoints();
            existingEdges.erase(it);
        }
        else
        {
            auto srcIt = currentNodes.find(e.srcNode);
            auto dstIt = currentNodes.find(e.dstNode);

            if (srcIt != currentNodes.end() && dstIt != currentNodes.end())
            {
                auto* edgeItem = new GraphEdgeItem(
                    model_,
                    e.id,
                    srcIt->second,
                    dstIt->second);

                scene_->addItem(edgeItem);
            }
        }
    }

    // 5. Remove unused items
    for (auto& pair : existingEdges)
    {
        scene_->removeItem(pair.second);
        delete pair.second;
    }

    for (auto& pair : existingNodes)
    {
        if (primaryNode_ == pair.second)
            primaryNode_ = nullptr;

        scene_->removeItem(pair.second);
        delete pair.second;
    }

    scene_->blockSignals(false);
    graphView_->setUpdatesEnabled(true);
    isRendering_ = false;

    // 6. Restore identical scrollbar values
    graphView_->horizontalScrollBar()->setValue(hVal);
    graphView_->verticalScrollBar()->setValue(vVal);
}

void MainWindow::saveLayout()
{
    if (!model_) return;

    for (auto* item : scene_->items()) {

        auto* nodeItem = dynamic_cast<GraphNodeItem*>(item);
        if (!nodeItem)
            continue;

        NodeId id = nodeItem->nodeId();   // expose getter
        QPointF p = nodeItem->pos();

        QJsonObject obj;
        obj["x"] = p.x();
        obj["y"] = p.y();

        QJsonDocument doc(obj);
        model_->setNodeMetadata(
            id,
            doc.toJson(QJsonDocument::Compact).toStdString()
        );
    }

    statusBar()->showMessage("Layout saved", 2000);
}

void MainWindow::handleAddNodeAtPosition(QPointF pos)
{
    if (!model_)
        return;

    NodeEditorDialog dlg(model_, 0, this);

    if (dlg.exec() != QDialog::Accepted)
        return;

    auto nodes = model_->nodes();
    if (nodes.empty())
        return;

    NodeData& newNode = nodes.back();
    NodeId newId = newNode.id;

    // Save position
    QJsonObject obj;
    obj["x"] = pos.x();
    obj["y"] = pos.y();

    QJsonDocument doc(obj);
    model_->setNodeMetadata(
        newId,
        doc.toJson(QJsonDocument::Compact).toStdString()
    );

    // If a layer selected → auto membership
    QModelIndex index = navigator_->currentIndex();
    if (index.isValid()) {
        ItemType type =
            static_cast<ItemType>(index.data(NavRole::Type).toInt());

        if (type == ItemType::Layer) {
            LayerId layerId =
                static_cast<LayerId>(
                    index.data(NavRole::Id).toULongLong());

            model_->addNodeToLayer(newId, layerId);

            populateNavigator();
            renderGraph(model_->extractGraph(layerId));
            return;
        }
    }

    populateNavigator();
    renderGraph(model_->extractGraph(std::nullopt));
}

void MainWindow::handleConnectNodes(qulonglong srcId,
                                    qulonglong dstId)
{
    if (!model_)
        return;

    if (srcId == dstId)
        return;

    // Must have a layer selected
    QModelIndex index = navigator_->currentIndex();
    if (!index.isValid())
        return;

    ItemType type =
        static_cast<ItemType>(index.data(NavRole::Type).toInt());

    if (type != ItemType::Layer) {
        statusBar()->showMessage("Select a layer first", 2000);
        return;
    }

    LayerId layerId =
        static_cast<LayerId>(
            index.data(NavRole::Id).toULongLong());

    EdgeData e;
    e.srcNode = srcId;
    e.dstNode = dstId;
    e.srcLayer = layerId;
    e.dstLayer = layerId;
    e.edgeType = "default";

    EdgeId newId;
    auto r = model_->addEdge(e, newId);

    if (!r.ok)
        return;

    renderGraph(model_->extractGraph(layerId));
}
void MainWindow::deleteSelected()
{
    if (!scene_ || !model_)
        return;

    const auto selected = scene_->selectedItems();
    if (selected.isEmpty())
        return;

    // Confirm deletion with the user
    auto reply = QMessageBox::question(
        this,
        "Confirm Delete",
        QString("Are you sure you want to delete %1 selected item(s)?").arg(selected.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    // 1. Separate selected nodes and edges
    std::vector<GraphNodeItem*> nodesToDelete;
    std::vector<GraphEdgeItem*> edgesToDelete;

    for (QGraphicsItem* item : selected)
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            nodesToDelete.push_back(node);
        else if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
            edgesToDelete.push_back(edge);
    }

    // 2. Delete selected Edges first (standalone edge selection)
    for (auto* edge : edgesToDelete)
    {
        if (!edge)
            continue;

        EdgeId eId = edge->edgeId();
        model_->deleteEdge(eId); // Delete from model & DB[cite: 11]

        scene_->removeItem(edge);
        delete edge; // Destructor unlinks from src_ and dst_[cite: 1, 2]
    }

    // 3. Delete selected Nodes (and cascade to connected edges)
    for (auto* node : nodesToDelete)
    {
        if (!node)
            continue;

        NodeId nId = node->nodeId();

        // Remove all edges in the scene connected to this node
        const auto allItems = scene_->items();
        for (QGraphicsItem* item : allItems)
        {
            if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
            {
                auto edgeData = model_->getEdgeById(edge->edgeId());
                if (edgeData && (edgeData->srcNode == nId || edgeData->dstNode == nId))
                {
                    model_->deleteEdge(edge->edgeId()); //[cite: 11]
                    scene_->removeItem(edge);
                    delete edge;
                }
            }
        }

        // Delete the node from the model and scene
        model_->deleteNode(nId); //[cite: 11]
        
        if (primaryNode_ == node)
            primaryNode_ = nullptr;

        scene_->removeItem(node);
        delete node;
    }

    // 4. Refresh selection state
    onSelectionChanged();
}

void MainWindow::setGraphMode(GraphView::Mode mode)
{
    if (!graphView_)
        return;

    graphView_->setMode(mode);

    QString text;

    switch (mode) {
    case GraphView::Mode::View:
        text = "Mode: View";
        break;
    case GraphView::Mode::Edit:
        text = "Mode: Layout";
        break;
    case GraphView::Mode::Add:
        text = "Mode: Add";
        break;
    case GraphView::Mode::Arch:
        text = "Mode: Arch";
        break;
    case GraphView::Mode::Connect:
        text = "Mode: Connect";
        break;
    }

    for (auto* item : scene_->items())
    {
        if (auto* node = qgraphicsitem_cast<GraphNodeItem*>(item))
        {
            bool selectable = (mode == GraphView::Mode::Edit || mode == GraphView::Mode::Arch);
            bool movable    = (mode == GraphView::Mode::Edit);

            node->setFlag(QGraphicsItem::ItemIsSelectable, selectable);
            node->setFlag(QGraphicsItem::ItemIsMovable, movable);
        }
    }
    statusBar()->showMessage(text);
}

/*
void MainWindow::onSelectionChanged()
{
    if (!scene_)
        return;

    const auto selected = scene_->selectedItems();

    // 1. If nothing is selected, clear and return
    if (selected.isEmpty())
    {
        primaryNode_ = nullptr;
        if (statusBar())
            statusBar()->showMessage("No selection");
        return;
    }

    // 2. Safely find if a real GraphNodeItem is selected using C++ dynamic_cast
    GraphNodeItem* newPrimary = nullptr;
    for (QGraphicsItem* item : selected)
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
        {
            if (node == primaryNode_.data())
            {
                newPrimary = node;
                break;
            }
            if (!newPrimary)
            {
                newPrimary = node;
            }
        }
    }

    primaryNode_ = newPrimary;

    // 3. Update primary highlight across nodes
    for (QGraphicsItem* item : scene_->items())
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
        {
            node->setPrimary(primaryNode_ && (node == primaryNode_.data()));
        }
    }

    // 4. Update status bar only if a valid node was selected
    if (statusBar())
    {
        if (primaryNode_)
        {
            QString text = primaryNode_->displayTitle();
            text.replace("\n", " | ");

            QString msg = QString("Primary: %1 | Selected: %2")
                              .arg(text)
                              .arg(selected.size());

            statusBar()->showMessage(msg);
        }
        else
        {
            // An edge or non-node item is selected
            statusBar()->showMessage(QString("Selected item count: %1").arg(selected.size()));
        }
    }
}
*/

// void MainWindow::onSelectionChanged()
// {
//     if (!scene_)
//         return;

//     const auto selected = scene_->selectedItems();

//     // 1. If nothing is selected, clear and return
//     if (selected.isEmpty())
//     {
//         primaryNode_ = nullptr;
//         if (statusBar())
//             statusBar()->showMessage("No selection");
//         return;
//     }

//     // 2. Identify the primary node using dynamic_cast
//     GraphNodeItem* newPrimary = nullptr;
//     for (QGraphicsItem* item : selected)
//     {
//         if (auto* node = dynamic_cast<GraphNodeItem*>(item))
//         {
//             if (node == primaryNode_)
//             {
//                 newPrimary = node;
//                 break;
//             }
//             if (!newPrimary)
//             {
//                 newPrimary = node;
//             }
//         }
//     }

//     primaryNode_ = newPrimary;

//     // 3. Update primary state across nodes
//     for (QGraphicsItem* item : scene_->items())
//     {
//         if (auto* node = dynamic_cast<GraphNodeItem*>(item))
//         {
//             node->setPrimary(primaryNode_ && (node == primaryNode_));
//         }
//     }

//     // 4. Update status bar
//     if (statusBar())
//     {
//         if (primaryNode_)
//         {
//             QString text = primaryNode_->displayTitle();
//             text.replace("\n", " | ");

//             QString msg = QString("Primary: %1 | Selected: %2")
//                               .arg(text)
//                               .arg(selected.size());

//             statusBar()->showMessage(msg);
//         }
//         else
//         {
//             statusBar()->showMessage(QString("Selected items: %1").arg(selected.size()));
//         }
//     }
// }

void MainWindow::onSelectionChanged()
{
    if (isRendering_ || !scene_)
        return;

    // Guard against running when MainWindow is tearing down
    auto* bar = statusBar();
    if (!bar)
        return;

    const auto selected = scene_->selectedItems();

    // 1. If nothing is selected, clear and return
    if (selected.isEmpty())
    {
        primaryNode_ = nullptr;
        bar->showMessage("No selection");
        return;
    }

    // 2. Identify the primary node safely using dynamic_cast
    GraphNodeItem* newPrimary = nullptr;
    for (QGraphicsItem* item : selected)
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
        {
            if (node == primaryNode_)
            {
                newPrimary = node;
                break;
            }
            if (!newPrimary)
            {
                newPrimary = node;
            }
        }
    }

    primaryNode_ = newPrimary;

    // 3. Update primary highlight across nodes
    for (QGraphicsItem* item : scene_->items())
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
        {
            node->setPrimary(primaryNode_ && (node == primaryNode_));
        }
    }

    // 4. Update status bar safely
    if (primaryNode_)
    {
        QString text = primaryNode_->displayTitle();
        text.replace("\n", " | ");

        QString msg = QString("Primary: %1 | Selected: %2")
                          .arg(text)
                          .arg(selected.size());

        bar->showMessage(msg);
    }
    else
    {
        bar->showMessage(QString("Selected items: %1").arg(selected.size()));
    }
}

void MainWindow::alignHorizontal()
{
    if (!primaryNode_)
        return;

    auto selected = scene_->selectedItems();
    if (selected.size() < 2)
        return;

    qreal y = primaryNode_->pos().y();

    for (auto* item : selected)
    {
        auto* node = qgraphicsitem_cast<GraphNodeItem*>(item);
        if (!node || node == primaryNode_)
            continue;

        node->setPos(node->pos().x(), y);
    }

    statusBar()->showMessage("Aligned horizontally", 2000);
}
void MainWindow::alignVertical()
{
    if (!primaryNode_)
        return;

    auto selected = scene_->selectedItems();
    if (selected.size() < 2)
        return;

    qreal x = primaryNode_->pos().x();

    for (auto* item : selected)
    {
        auto* node = qgraphicsitem_cast<GraphNodeItem*>(item);
        if (!node || node == primaryNode_)
            continue;

        node->setPos(x, node->pos().y());
    }

    statusBar()->showMessage("Aligned vertically", 2000);
}

void MainWindow::setDb(std::string db_path)
{
    if(!db_path.empty())
    {
        // close current database
        db_.close();
        // open as new database
        db_.open(db_path);

        // delete model
        if(model_ !=nullptr)
        {
            delete model_;
        }
        
        // create Model
        model_ = new ArchitectureModel(db_);

        populateNavigator();
        renderGraph(model_->extractGraph(std::nullopt));
    }
}
