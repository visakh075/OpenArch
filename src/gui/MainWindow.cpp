#include "MainWindow.h"

#include <QSplitter>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
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
    if (scene_)
        scene_->disconnect(this);

    delete model_;
}

void MainWindow::setupUi()
{
    navModel_ = new QStandardItemModel(this);
    navModel_->setHorizontalHeaderLabels({"Architecture"});

    navigator_ = new QTreeView;
    navigator_->setModel(navModel_);

    architectureDock_ = new QDockWidget("Architecture", this);
    architectureDock_->setObjectName("ArchitectureDock");
    architectureDock_->setWidget(navigator_);

    addDockWidget(Qt::LeftDockWidgetArea, architectureDock_);

    scene_ = new QGraphicsScene(this);

    graphView_ = new GraphView(this);
    graphView_->setScene(scene_);
    graphView_->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    graphView_->setRenderHint(QPainter::Antialiasing);
    graphView_->setInteractive(true);

    setCentralWidget(graphView_);

    ThemeEditorDock* themeDock = new ThemeEditorDock(this);
    themeDock->setObjectName("ThemeDock");
    addDockWidget(Qt::RightDockWidgetArea, themeDock);

    connect(
        GraphThemeManager::instance(),
        &GraphThemeManager::themeChanged,
        this,
        [this]()
        {
            if (scene_)
            {
                for (QGraphicsItem* item : scene_->items())
                {
                    if (auto* node = dynamic_cast<GraphNodeItem*>(item))
                    {
                        node->onThemeChanged();
                    }
                    else if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
                    {
                        edge->refreshPath();
                    }
                }
                scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
            }

            if (graphView_)
            {
                graphView_->viewport()->update();
            }
        });

    setDockNestingEnabled(true);
}

void MainWindow::setupToolbar()
{
    graphToolBar_ = addToolBar("Graph Modes");

    actionView_ = graphToolBar_->addAction("View (V)");
    actionEdit_ = graphToolBar_->addAction("Edit (E)");    

    actionView_->setCheckable(true);
    actionEdit_->setCheckable(true); 

    QActionGroup* group = new QActionGroup(this);
    group->addAction(actionView_);
    group->addAction(actionEdit_);

    actionView_->setChecked(true);

    connect(actionView_, &QAction::triggered,
            this, [this]() { setGraphMode(GraphView::Mode::View); });

    connect(actionEdit_, &QAction::triggered,
            this, [this]() { setGraphMode(GraphView::Mode::Edit); });

    actionView_->setShortcut(Qt::Key_V);
    actionEdit_->setShortcut(Qt::Key_L);
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

    QAction* exportCurrentAction = new QAction("Export Current View", this);
    connect(exportCurrentAction, &QAction::triggered, this, [this]() {
        graphView_->exportToSvg(GraphView::ExportMode::CurrentView);
    });
    fileMenu->addAction(exportCurrentAction);

    QAction* exportWholeAction = new QAction("Export Whole Diagram", this);
    connect(exportWholeAction, &QAction::triggered, this, [this]() {
        graphView_->exportToSvg(GraphView::ExportMode::WholeScene);
    });
    fileMenu->addAction(exportWholeAction);

    QAction* moveAction = new QAction("Move Selection To...", this);
    connect(moveAction, &QAction::triggered, this, [this]() {
        bool okX = false;
        bool okY = false;

        double x = QInputDialog::getDouble(this, "Move Selection", "Target X:", 0.0, -100000, 100000, 2, &okX);
        if (!okX) return;

        double y = QInputDialog::getDouble(this, "Move Selection", "Target Y:", 0.0, -100000, 100000, 2, &okY);
        if (!okY) return;

        graphView_->moveSelectionTo(QPointF(x, y));
    });
    editMenu->addAction(moveAction);

    auto* themeMenu = menuBar()->addMenu("&Theme");
    QDir themeDir("themes");
    if (themeDir.exists()) {
        QStringList filters;
        filters << "*.json";
        QFileInfoList list = themeDir.entryInfoList(filters, QDir::Files);

        for (const QFileInfo& fi : list) {
            QString path = fi.absoluteFilePath();
            QString name = fi.baseName();
            themeMenu->addAction(name, this, [this, path]() {
                GraphThemeManager::instance()->load(path);
            });
        }
        themeMenu->addSeparator();
    }

    themeMenu->addAction("Open Theme File...", this, &MainWindow::loadThemeFromFile);
    themeMenu->addAction("Reset to Default", this, []() {
        GraphThemeManager::instance()->resetDefaults();
    });

    QAction* exportHtmlAction = new QAction("Export Interactive HTML...", this);
    connect(exportHtmlAction, &QAction::triggered, this, [this]() {
        graphView_->exportToInteractiveHtml();
    });
    fileMenu->addAction(exportHtmlAction);
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

void MainWindow::setDb(const std::string& db_path)
{
    if (db_path.empty())
        return;

    if (scene_)
    {
        scene_->blockSignals(true);

        QList<QGraphicsItem*> allItems = scene_->items();
        for (QGraphicsItem* item : allItems)
        {
            if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
            {
                scene_->removeItem(edge);
                delete edge;
            }
        }

        for (QGraphicsItem* item : scene_->items())
        {
            if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            {
                node->setParentItem(nullptr);
            }
        }

        scene_->clear();
        scene_->blockSignals(false);
    }
    primaryNode_ = nullptr;

    delete model_;
    model_ = nullptr;

    if (db_)
    {
        db_->close();
        db_.reset();
    }

    QString qPath = QString::fromStdString(db_path);
    if (qPath.endsWith(".db", Qt::CaseInsensitive) || 
        qPath.endsWith(".sqlite", Qt::CaseInsensitive) || 
        qPath.endsWith(".sqlite3", Qt::CaseInsensitive))
    {
        db_ = std::make_unique<DbManagerSQLite>();
    }
    else
    {
        db_ = std::make_unique<DbManagerJson>();
    }

    auto r = db_->open(db_path);
    if (!r.ok)
    {
        QMessageBox::critical(this, "Error", QString::fromStdString(r.message));
        return;
    }

    model_ = new ArchitectureModel(*db_);
    populateNavigator();
    renderGraph(model_->extractGraph(std::nullopt));
}

void MainWindow::openDatabase()
{
    QString file = QFileDialog::getOpenFileName(
        this,
        "Open Architecture Database",
        "",
        "Supported Files (*.db *.sqlite *.sqlite3 *.json);;SQLite Databases (*.db *.sqlite *.sqlite3);;Architecture JSON (*.json);;All Files (*.*)");

    if (file.isEmpty())
        return;

    setDb(file.toStdString());
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
    if (!model_ || !graphView_)
        return;

    if (graphView_->mode() == GraphView::Mode::View)
        return;

    auto* item = navModel_->itemFromIndex(index);
    if (!item)
        return;

    ItemType type = static_cast<ItemType>(item->data(NavRole::Type).toInt());
    qulonglong id = item->data(NavRole::Id).toULongLong();

    switch (type) {
    case ItemType::Node: {
        NodeEditorDialog dlg(model_, id, this);
        if (dlg.exec() == QDialog::Accepted)
        {
            // Clear existing scene items so nodes recalculate their parent hierarchy completely
            if (scene_)
            {
                scene_->clear();
            }
            primaryNode_ = nullptr;
            populateNavigator();
            renderGraph(model_->extractGraph(std::nullopt));
        }
        break;
    }
    case ItemType::Layer: {
        LayerEditorDialog dlg(model_, id, this);
        if (dlg.exec() == QDialog::Accepted)
        {
            if (scene_)
            {
                scene_->clear();
            }
            primaryNode_ = nullptr;
            populateNavigator();
            renderGraph(model_->extractGraph(static_cast<LayerId>(id)));
        }
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

    ItemType type = static_cast<ItemType>(item->data(NavRole::Type).toInt());

    if (type == ItemType::Layer) {
        LayerId layerId = static_cast<LayerId>(item->data(NavRole::Id).toULongLong());
        renderGraph(model_->extractGraph(layerId));
    }
    else {
        renderGraph(model_->extractGraph(std::nullopt));
    }
}

void MainWindow::createNewNode()
{
    if (!model_)
        return;

    NodeEditorDialog dlg(model_, 0, this);
    dlg.exec();

    populateNavigator();
    renderGraph(model_->extractGraph(std::nullopt));
}

void MainWindow::createNewLayer()
{
    if (!model_)
        return;

    LayerEditorDialog dlg(model_, 0, this);
    dlg.exec();

    populateNavigator();
    renderGraph(model_->extractGraph(std::nullopt));
}

void MainWindow::renderGraph(const GraphSnapshot& snap)
{
    if (!model_ || !scene_ || !graphView_)
        return;

    int hVal = graphView_->horizontalScrollBar()->value();
    int vVal = graphView_->verticalScrollBar()->value();

    isRendering_ = true;
    scene_->blockSignals(true);
    graphView_->setUpdatesEnabled(false);

    // 1. Collect all existing scene items
    std::unordered_map<NodeId, GraphNodeItem*> existingNodes;
    std::unordered_map<EdgeId, GraphEdgeItem*> existingEdges;

    for (QGraphicsItem* item : scene_->items())
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            existingNodes[node->nodeId()] = node;
        else if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
            existingEdges[edge->edgeId()] = edge;
    }

    // 2. Remove and delete ALL edges first to prevent dangling callbacks
    for (auto& pair : existingEdges)
    {
        if (pair.second)
        {
            if (pair.second->scene() == scene_)
                scene_->removeItem(pair.second);
            delete pair.second;
        }
    }
    existingEdges.clear();

    // 3. CRITICAL: Unparent ALL existing nodes immediately.
    // This breaks Qt's automatic child-deletion cascade so deleting an unused container
    // does NOT free child nodes that are retained in currentNodes.
    for (auto& pair : existingNodes)
    {
        if (pair.second)
        {
            pair.second->setParentItem(nullptr);
        }
    }

    // 4. Stage 1: Partition retained nodes vs obsolete nodes
    std::unordered_map<NodeId, GraphNodeItem*> currentNodes;
    auto mode = graphView_->mode();
    bool selectable = (mode == GraphView::Mode::Edit || mode == GraphView::Mode::Arch || mode == GraphView::Mode::View);
    bool movable    = (mode == GraphView::Mode::Edit);

    for (const auto& n : snap.nodes)
    {
        GraphNodeItem* nodeItem = nullptr;
        auto it = existingNodes.find(n.id);
        if (it != existingNodes.end())
        {
            nodeItem = it->second;
            existingNodes.erase(it); // Retain this node
        }
        else
        {
            nodeItem = new GraphNodeItem(model_, n.id);
        }

        nodeItem->setFlag(QGraphicsItem::ItemIsSelectable, selectable);
        nodeItem->setFlag(QGraphicsItem::ItemIsMovable, movable);
        nodeItem->setAcceptHoverEvents(true);

        currentNodes[n.id] = nodeItem;
    }

    // 5. Delete obsolete nodes that are not in the new layer snapshot
    for (auto& pair : existingNodes)
    {
        if (pair.second)
        {
            if (primaryNode_ == pair.second)
                primaryNode_ = nullptr;

            if (pair.second->scene() == scene_)
                scene_->removeItem(pair.second);

            delete pair.second;
        }
    }
    existingNodes.clear();

    // 6. Stage 2: Re-establish parent-child links only if the parent is present in currentNodes
    for (const auto& n : snap.nodes)
    {
        GraphNodeItem* nodeItem = currentNodes[n.id];
        if (n.parentId.has_value())
        {
            auto pIt = currentNodes.find(*n.parentId);
            if (pIt != currentNodes.end() && pIt->second != nodeItem)
            {
                nodeItem->setParentItem(pIt->second);
                nodeItem->setZValue(pIt->second->zValue() + 1);
            }
            else
            {
                nodeItem->setParentItem(nullptr);
                nodeItem->setZValue(0);
            }
        }
        else
        {
            nodeItem->setParentItem(nullptr);
            nodeItem->setZValue(0);
        }
    }

    // 7. Stage 3: Add only unparented root items directly to scene_
    for (const auto& n : snap.nodes)
    {
        GraphNodeItem* nodeItem = currentNodes[n.id];
        if (!nodeItem->parentItem())
        {
            if (nodeItem->scene() != scene_)
            {
                scene_->addItem(nodeItem);
            }
        }
    }

    // 8. Stage 4: Position items and refresh geometry
    int i = 0;
    for (const auto& n : snap.nodes)
    {
        GraphNodeItem* nodeItem = currentNodes[n.id];
        bool restored = false;

        auto nodeOpt = model_->getNodeById(n.id);
        if (nodeOpt && !nodeOpt->metadata.empty())
        {
            QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(nodeOpt->metadata).toUtf8());
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

        if (!restored && !nodeItem->parentItem())
        {
            nodeItem->setPos((i % 5) * 200, (i / 5) * 140);
        }

        nodeItem->refreshGeometry();
        ++i;
    }

    // 9. Stage 5: Recreate edges ONLY after all nodes are positioned and attached to scene_
    for (const auto& e : snap.edges)
    {
        auto srcIt = currentNodes.find(e.srcNode);
        auto dstIt = currentNodes.find(e.dstNode);

        if (srcIt != currentNodes.end() && dstIt != currentNodes.end())
        {
            GraphNodeItem* srcNode = srcIt->second;
            GraphNodeItem* dstNode = dstIt->second;

            if (srcNode && dstNode && srcNode->scene() == scene_ && dstNode->scene() == scene_)
            {
                auto* edgeItem = new GraphEdgeItem(model_, e.id, srcNode, dstNode);
                edgeItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
                scene_->addItem(edgeItem);
                edgeItem->updateEndpoints();
            }
        }
    }

    scene_->blockSignals(false);
    graphView_->setUpdatesEnabled(true);
    isRendering_ = false;

    graphView_->horizontalScrollBar()->setValue(hVal);
    graphView_->verticalScrollBar()->setValue(vVal);
}

void MainWindow::saveLayout()
{
    if (!model_ || !scene_) return;

    for (QGraphicsItem* item : scene_->items())
    {
        auto* nodeItem = dynamic_cast<GraphNodeItem*>(item);
        if (!nodeItem)
            continue;

        NodeId id = nodeItem->nodeId();
        QPointF p = nodeItem->pos();

        QJsonObject obj;
        obj["x"] = p.x();
        obj["y"] = p.y();

        QJsonDocument doc(obj);
        model_->setNodeMetadata(id, doc.toJson(QJsonDocument::Compact).toStdString());
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

    QJsonObject obj;
    obj["x"] = pos.x();
    obj["y"] = pos.y();

    QJsonDocument doc(obj);
    model_->setNodeMetadata(newId, doc.toJson(QJsonDocument::Compact).toStdString());

    QModelIndex index = navigator_->currentIndex();
    if (index.isValid()) {
        ItemType type = static_cast<ItemType>(index.data(NavRole::Type).toInt());
        if (type == ItemType::Layer) {
            LayerId layerId = static_cast<LayerId>(index.data(NavRole::Id).toULongLong());
            model_->addNodeToLayer(newId, layerId);
            populateNavigator();
            renderGraph(model_->extractGraph(layerId));
            return;
        }
    }

    populateNavigator();
    renderGraph(model_->extractGraph(std::nullopt));
}

void MainWindow::handleConnectNodes(qulonglong srcId, qulonglong dstId)
{
    if (!model_ || srcId == dstId)
        return;

    QModelIndex index = navigator_->currentIndex();
    if (!index.isValid())
        return;

    ItemType type = static_cast<ItemType>(index.data(NavRole::Type).toInt());
    if (type != ItemType::Layer) {
        statusBar()->showMessage("Select a layer first", 2000);
        return;
    }

    LayerId layerId = static_cast<LayerId>(index.data(NavRole::Id).toULongLong());

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

    auto reply = QMessageBox::question(
        this,
        "Confirm Delete",
        QString("Are you sure you want to delete %1 selected item(s)?").arg(selected.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    std::vector<GraphNodeItem*> nodesToDelete;
    std::vector<GraphEdgeItem*> edgesToDelete;

    for (QGraphicsItem* item : selected)
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            nodesToDelete.push_back(node);
        else if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
            edgesToDelete.push_back(edge);
    }

    for (auto* edge : edgesToDelete)
    {
        if (!edge) continue;
        model_->deleteEdge(edge->edgeId());
        scene_->removeItem(edge);
        delete edge;
    }

    for (auto* node : nodesToDelete)
    {
        if (!node) continue;
        NodeId nId = node->nodeId();

        const auto allItems = scene_->items();
        for (QGraphicsItem* item : allItems)
        {
            if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
            {
                auto edgeData = model_->getEdgeById(edge->edgeId());
                if (edgeData && (edgeData->srcNode == nId || edgeData->dstNode == nId))
                {
                    model_->deleteEdge(edge->edgeId());
                    scene_->removeItem(edge);
                    delete edge;
                }
            }
        }

        model_->deleteNode(nId);
        if (primaryNode_ == node)
            primaryNode_ = nullptr;

        scene_->removeItem(node);
        delete node;
    }

    onSelectionChanged();
}

void MainWindow::setGraphMode(GraphView::Mode mode)
{
    if (!graphView_)
        return;

    graphView_->setMode(mode);

    QString text;
    switch (mode) {
    case GraphView::Mode::View:    text = "Mode: View"; break;
    case GraphView::Mode::Edit:    text = "Mode: Layout"; break;
    case GraphView::Mode::Add:     text = "Mode: Add"; break;
    case GraphView::Mode::Arch:    text = "Mode: Arch"; break;
    case GraphView::Mode::Connect: text = "Mode: Connect"; break;
    }

    bool movable = (mode == GraphView::Mode::Edit);

    for (auto* item : scene_->items())
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
        {
            node->setFlag(QGraphicsItem::ItemIsSelectable, true);
            node->setFlag(QGraphicsItem::ItemIsMovable, movable);
        }
        else if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
        {
            edge->setFlag(QGraphicsItem::ItemIsSelectable, true);
        }
    }
    statusBar()->showMessage(text);
}

void MainWindow::onSelectionChanged()
{
    if (isRendering_ || !scene_)
        return;

    auto* bar = statusBar();
    if (!bar)
        return;

    const auto selected = scene_->selectedItems();
    
    if (selected.isEmpty())
    {
        primaryNode_ = nullptr;
        bar->showMessage("No selection");
        return;
    }

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

    for (QGraphicsItem* item : scene_->items())
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
        {
            node->setPrimary(primaryNode_ && (node == primaryNode_));
        }
    }

    if (primaryNode_)
    {
        QString text = primaryNode_->displayTitle();
        text.replace("\n", " | ");
        bar->showMessage(QString("Primary: %1 | Selected: %2").arg(text).arg(selected.size()));
    }
    else
    {
        bar->showMessage(QString("Selected items: %1").arg(selected.size()));
    }
}

void MainWindow::alignHorizontal()
{
    if (!primaryNode_ || !scene_)
        return;

    auto selected = scene_->selectedItems();
    if (selected.size() < 2)
        return;

    qreal targetSceneY = primaryNode_->mapToScene(primaryNode_->boundingRect().center()).y();

    for (auto* item : selected)
    {
        auto* node = dynamic_cast<GraphNodeItem*>(item);
        if (!node || node == primaryNode_)
            continue;

        qreal targetParentY = targetSceneY;
        if (node->parentItem())
        {
            QPointF parentTarget = node->parentItem()->mapFromScene(QPointF(0, targetSceneY));
            targetParentY = parentTarget.y();
        }

        qreal halfHeight = node->boundingRect().height() / 2.0;
        qreal newY = targetParentY - halfHeight - node->boundingRect().top();

        node->setPos(node->pos().x(), newY);

        for (auto* edge : node->edges())
        {
            if (edge && edge->scene())
                edge->updateEndpoints();
        }

        if (auto* parentContainer = dynamic_cast<GraphNodeItem*>(node->parentItem()))
        {
            parentContainer->refreshGeometry();
        }
    }

    statusBar()->showMessage("Aligned horizontally (center)", 2000);
}

void MainWindow::alignVertical()
{
    if (!primaryNode_ || !scene_)
        return;

    auto selected = scene_->selectedItems();
    if (selected.size() < 2)
        return;

    qreal targetSceneX = primaryNode_->mapToScene(primaryNode_->boundingRect().center()).x();

    for (auto* item : selected)
    {
        auto* node = dynamic_cast<GraphNodeItem*>(item);
        if (!node || node == primaryNode_)
            continue;

        qreal targetParentX = targetSceneX;
        if (node->parentItem())
        {
            QPointF parentTarget = node->parentItem()->mapFromScene(QPointF(targetSceneX, 0));
            targetParentX = parentTarget.x();
        }

        qreal halfWidth = node->boundingRect().width() / 2.0;
        qreal newX = targetParentX - halfWidth - node->boundingRect().left();

        node->setPos(newX, node->pos().y());

        for (auto* edge : node->edges())
        {
            if (edge && edge->scene())
                edge->updateEndpoints();
        }

        if (auto* parentContainer = dynamic_cast<GraphNodeItem*>(node->parentItem()))
        {
            parentContainer->refreshGeometry();
        }
    }

    statusBar()->showMessage("Aligned vertically (center)", 2000);
}

void MainWindow::loadThemeFromFile()
{
    QString file = QFileDialog::getOpenFileName(
        this, "Select Theme File", "", "Theme JSON (*.json);;All Files (*.*)");

    if (file.isEmpty())
        return;

    if (!GraphThemeManager::instance()->load(file)) {
        QMessageBox::critical(this, "Error", "Failed to load theme configuration.");
    }
}

void MainWindow::switchThemePreset(const QString& path)
{
    if (!GraphThemeManager::instance()->load(path)) {
        QMessageBox::critical(this, "Error", "Failed to load theme configuration: " + path);
    }
}