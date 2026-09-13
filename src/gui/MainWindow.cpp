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
#include <QToolButton>
#include <QMenu>
#include <QStyle>
#include <QInputDialog>
#include <QKeySequence>
#include <QScrollBar>
#include <algorithm>
#include <unordered_set>

#include "NodeEditorDialog.h"
#include "LayerEditorDialog.h"
#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"
#include "GraphThemeManager.h"
#include "ThemeEditorDock.h"

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

void MainWindow::setupMenu()
{
    auto* fileMenu = menuBar()->addMenu("&File");

    fileMenu->addAction("Open DB", this, &MainWindow::openDatabase);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction("Add Node", this, &MainWindow::createNewNode);
    editMenu->addAction("Add Layer", this, &MainWindow::createNewLayer);
    
    QAction* copyNodeAction = editMenu->addAction("Duplicate Node", this, &MainWindow::copySelectedNode);
    copyNodeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));

    editMenu->addAction("Save Layout", this, &MainWindow::saveLayout);

    // --- Distribution Actions ---
    editMenu->addSeparator();
    actionDistH_ = new QAction(QIcon(":/icons/dist-h.svg"), "Distribute Horizontally", this);
    actionDistH_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_H));
    connect(actionDistH_, &QAction::triggered, this, &MainWindow::distributeHorizontal);
    editMenu->addAction(actionDistH_);

    actionDistV_ = new QAction(QIcon(":/icons/dist-v.svg"), "Distribute Vertically", this);
    actionDistV_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_V));
    connect(actionDistV_, &QAction::triggered, this, &MainWindow::distributeVertical);
    editMenu->addAction(actionDistV_);

    // --- Align Submenus ---
    auto* alignMenu = editMenu->addMenu("Align");

    auto* alignHMenu = alignMenu->addMenu("Horizontal");
    actionAlignLeft_ = new QAction(QIcon(":/icons/align-left.svg"), "Left", this);
    connect(actionAlignLeft_, &QAction::triggered, this, [this]() { alignNodes(AlignType::Left); });
    alignHMenu->addAction(actionAlignLeft_);

    actionAlignCenterH_ = new QAction(QIcon(":/icons/align-center-h.svg"), "Center", this);
    actionAlignCenterH_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
    connect(actionAlignCenterH_, &QAction::triggered, this, [this]() { alignNodes(AlignType::CenterH); });
    alignHMenu->addAction(actionAlignCenterH_);

    actionAlignRight_ = new QAction(QIcon(":/icons/align-right.svg"), "Right", this);
    connect(actionAlignRight_, &QAction::triggered, this, [this]() { alignNodes(AlignType::Right); });
    alignHMenu->addAction(actionAlignRight_);

    auto* alignVMenu = alignMenu->addMenu("Vertical");
    actionAlignTop_ = new QAction(QIcon(":/icons/align-top.svg"), "Top", this);
    connect(actionAlignTop_, &QAction::triggered, this, [this]() { alignNodes(AlignType::Top); });
    alignVMenu->addAction(actionAlignTop_);

    actionAlignCenterV_ = new QAction(QIcon(":/icons/align-center-v.svg"), "Middle", this);
    actionAlignCenterV_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
    connect(actionAlignCenterV_, &QAction::triggered, this, [this]() { alignNodes(AlignType::CenterV); });
    alignVMenu->addAction(actionAlignCenterV_);

    actionAlignBottom_ = new QAction(QIcon(":/icons/align-bottom.svg"), "Bottom", this);
    connect(actionAlignBottom_, &QAction::triggered, this, [this]() { alignNodes(AlignType::Bottom); });
    alignVMenu->addAction(actionAlignBottom_);

    // --- Connect Action ---
    editMenu->addSeparator();
    actionConnect_ = new QAction(QIcon(":/icons/connect.svg"), "Connect Selected Nodes", this);
    actionConnect_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connect(actionConnect_, &QAction::triggered, this, &MainWindow::connectSelectedNodes);
    editMenu->addAction(actionConnect_);

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

void MainWindow::setupToolbar()
{
    // ========================================================
    // Graph Modes Toolbar
    // ========================================================
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

    // ========================================================
    // Layout Toolbar (Reuses Actions from setupMenu)
    // ========================================================
    layoutToolBar_ = addToolBar("Layout");
    layoutToolBar_->setIconSize(QSize(20, 20));

    // 1. Align Dropdown Button
    QToolButton* alignBtn = new QToolButton(this);
    alignBtn->setText("Align");
    alignBtn->setToolTip("Align selected objects relative to the primary node");
    alignBtn->setPopupMode(QToolButton::InstantPopup);
    alignBtn->setIcon(QIcon(":/icons/align-center-h.svg"));

    QMenu* alignPopup = new QMenu(alignBtn);
    alignPopup->addAction(actionAlignLeft_);
    alignPopup->addAction(actionAlignCenterH_);
    alignPopup->addAction(actionAlignRight_);
    alignPopup->addSeparator();
    alignPopup->addAction(actionAlignTop_);
    alignPopup->addAction(actionAlignCenterV_);
    alignPopup->addAction(actionAlignBottom_);

    alignBtn->setMenu(alignPopup);
    layoutToolBar_->addWidget(alignBtn);

    // 2. Distribute Dropdown Button
    QToolButton* distBtn = new QToolButton(this);
    distBtn->setText("Distribute");
    distBtn->setToolTip("Distribute selected objects evenly");
    distBtn->setPopupMode(QToolButton::InstantPopup);
    distBtn->setIcon(QIcon(":/icons/dist-h.svg"));

    QMenu* distPopup = new QMenu(distBtn);
    distPopup->addAction(actionDistH_);
    distPopup->addAction(actionDistV_);

    distBtn->setMenu(distPopup);
    layoutToolBar_->addWidget(distBtn);

    layoutToolBar_->addSeparator();

    // 3. Connect and Duplicate Buttons
    layoutToolBar_->addAction(actionConnect_);

    QAction* copyBtn = layoutToolBar_->addAction(QIcon(":/icons/copy.svg"), "Duplicate");
    copyBtn->setToolTip("Duplicate selected node (Ctrl+D)");
    connect(copyBtn, &QAction::triggered, this, &MainWindow::copySelectedNode);
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

    auto* duplicateShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this);
    connect(duplicateShortcut, &QShortcut::activated, this, &MainWindow::copySelectedNode);
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
    if (!model_)
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
            populateNavigator();
            QModelIndex curIdx = navigator_->currentIndex();
            if (curIdx.isValid() && static_cast<ItemType>(curIdx.data(NavRole::Type).toInt()) == ItemType::Layer)
            {
                LayerId layerId = static_cast<LayerId>(curIdx.data(NavRole::Id).toULongLong());
                renderGraph(model_->extractGraph(layerId));
            }
            else
            {
                renderGraph(model_->extractGraph(std::nullopt));
            }
        }
        break;
    }
    case ItemType::Layer: {
        LayerEditorDialog dlg(model_, id, this);
        if (dlg.exec() == QDialog::Accepted)
        {
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

    // 3. Unparent all existing nodes to break Qt ownership cascades during layer transitions
    for (auto& pair : existingNodes)
    {
        if (pair.second)
        {
            pair.second->setParentItem(nullptr);
        }
    }

    // 4. Partition retained nodes vs obsolete nodes
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
            existingNodes.erase(it);
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

    // 5. Delete obsolete nodes
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

    // 6. Re-establish parent-child links safely
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

    // 7. Add only unparented root items directly to scene_
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

    // 8. Position items and refresh geometry
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

    // 9. Recreate edges
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
                // edgeItem->setZValue(10.0);
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

    auto srcOpt = model_->getNodeById(srcId);
    auto dstOpt = model_->getNodeById(dstId);

    QString srcName = srcOpt ? QString::fromStdString(srcOpt->name) : QString("Node %1").arg(srcId);
    QString dstName = dstOpt ? QString::fromStdString(dstOpt->name) : QString("Node %2").arg(dstId);

    std::unordered_set<LayerId> srcLayers;
    for (const auto& layer : model_->layers())
    {
        for (const auto& nl : model_->nodesInLayer(layer.id))
        {
            if (nl.nodeId == srcId)
            {
                srcLayers.insert(layer.id);
                break;
            }
        }
    }

    std::vector<LayerData> commonLayers;
    for (const auto& layer : model_->layers())
    {
        if (srcLayers.find(layer.id) == srcLayers.end())
            continue;

        for (const auto& nl : model_->nodesInLayer(layer.id))
        {
            if (nl.nodeId == dstId)
            {
                commonLayers.push_back(layer);
                break;
            }
        }
    }

    if (commonLayers.empty())
    {
        QMessageBox::warning(
            this,
            "No Common Layer",
            QString("Nodes '%1' and '%2' do not share any common layers.\n"
                    "Assign them to a common layer first to connect them.")
                .arg(srcName, dstName));
        return;
    }

    LayerId chosenLayerId = commonLayers[0].id;
    QModelIndex currentIndex = navigator_->currentIndex();
    bool activeLayerMatched = false;

    if (currentIndex.isValid())
    {
        ItemType type = static_cast<ItemType>(currentIndex.data(NavRole::Type).toInt());
        if (type == ItemType::Layer)
        {
            LayerId activeLayerId = static_cast<LayerId>(currentIndex.data(NavRole::Id).toULongLong());
            for (const auto& cl : commonLayers)
            {
                if (cl.id == activeLayerId)
                {
                    chosenLayerId = activeLayerId;
                    activeLayerMatched = true;
                    break;
                }
            }
        }
    }

    if (!activeLayerMatched && commonLayers.size() > 1)
    {
        QStringList layerNames;
        for (const auto& l : commonLayers)
            layerNames << QString("%1 (ID: %2)").arg(QString::fromStdString(l.name)).arg(l.id);

        bool ok = false;
        QString chosen = QInputDialog::getItem(
            this,
            "Select Layer",
            "Multiple shared layers found. Choose edge layer:",
            layerNames,
            0,
            false,
            &ok);

        if (!ok)
            return;

        int idx = layerNames.indexOf(chosen);
        chosenLayerId = commonLayers[idx].id;
    }

    bool ok = false;
    QString edgeType = QInputDialog::getText(
        this,
        "Edge Type",
        "Enter connection type / protocol:",
        QLineEdit::Normal,
        "default",
        &ok);

    if (!ok)
        return;

    EdgeData e;
    e.srcNode = srcId;
    e.dstNode = dstId;
    e.srcLayer = chosenLayerId;
    e.dstLayer = chosenLayerId;
    e.edgeType = edgeType.trimmed().isEmpty() ? "default" : edgeType.trimmed().toStdString();

    EdgeId newId = 0;
    auto r = model_->addEdge(e, newId);
    if (!r.ok)
    {
        QMessageBox::critical(this, "Error", QString::fromStdString(r.message));
        return;
    }

    if (currentIndex.isValid() &&
        static_cast<ItemType>(currentIndex.data(NavRole::Type).toInt()) == ItemType::Layer)
    {
        LayerId activeLayerId = static_cast<LayerId>(currentIndex.data(NavRole::Id).toULongLong());
        renderGraph(model_->extractGraph(activeLayerId));
    }
    else
    {
        renderGraph(model_->extractGraph(std::nullopt));
    }

    statusBar()->showMessage(QString("Connected %1 -> %2").arg(srcName, dstName), 2500);
}

void MainWindow::connectSelectedNodes()
{
    if (!scene_ || !model_)
        return;

    std::vector<GraphNodeItem*> selectedNodes;
    for (QGraphicsItem* item : scene_->selectedItems())
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            selectedNodes.push_back(node);
    }

    if (selectedNodes.size() < 2)
    {
        QMessageBox::information(this, "Connect Nodes", "Please select at least two nodes on the canvas to connect.");
        return;
    }

    GraphNodeItem* srcNode = nullptr;
    GraphNodeItem* dstNode = nullptr;

    if (selectedNodes.size() == 2)
    {
        if (primaryNode_ && (selectedNodes[0] == primaryNode_ || selectedNodes[1] == primaryNode_))
        {
            srcNode = primaryNode_;
            dstNode = (selectedNodes[0] == primaryNode_) ? selectedNodes[1] : selectedNodes[0];
        }
        else
        {
            srcNode = selectedNodes[0];
            dstNode = selectedNodes[1];
        }
    }
    else
    {
        QStringList titles;
        for (auto* n : selectedNodes)
            titles << QString("%1 (ID: %2)").arg(n->displayTitle()).arg(n->nodeId());

        bool ok = false;
        QString srcChoice = QInputDialog::getItem(this, "Select Source Node", "Source:", titles, 0, false, &ok);
        if (!ok) return;

        int srcIndex = titles.indexOf(srcChoice);
        srcNode = selectedNodes[srcIndex];

        QStringList dstTitles = titles;
        dstTitles.removeAt(srcIndex);

        QString dstChoice = QInputDialog::getItem(this, "Select Target Node", "Target:", dstTitles, 0, false, &ok);
        if (!ok) return;

        for (auto* n : selectedNodes)
        {
            if (QString("%1 (ID: %2)").arg(n->displayTitle()).arg(n->nodeId()) == dstChoice)
            {
                dstNode = n;
                break;
            }
        }
    }

    if (srcNode && dstNode)
    {
        handleConnectNodes(srcNode->nodeId(), dstNode->nodeId());
    }
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

NodeId MainWindow::cloneNodeRecursive(NodeId sourceId, std::optional<NodeId> newParentId, qreal offsetX, qreal offsetY)
{
    if (!model_)
        return 0;

    auto opt = model_->getNodeById(sourceId);
    if (!opt)
        return 0;

    NodeData source = *opt;

    NodeData copyNode;
    copyNode.name = source.name + " (Copy)";
    copyNode.type = source.type;
    copyNode.parentId = newParentId;
    copyNode.attributes = source.attributes;
    copyNode.status = Status::New;
    copyNode.reviewer = "";

    if (!source.metadata.empty())
    {
        QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(source.metadata).toUtf8());
        if (doc.isObject())
        {
            QJsonObject obj = doc.object();
            if (obj.contains("x") && obj.contains("y"))
            {
                obj["x"] = obj["x"].toDouble() + offsetX;
                obj["y"] = obj["y"].toDouble() + offsetY;
                copyNode.metadata = QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
            }
        }
    }
    if (copyNode.metadata.empty())
    {
        QJsonObject obj;
        obj["x"] = offsetX;
        obj["y"] = offsetY;
        copyNode.metadata = QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
    }

    NodeId newId = 0;
    auto res = model_->addNode(copyNode, newId);
    if (!res.ok)
        return 0;

    for (const auto& layer : model_->layers())
    {
        auto nodesInLay = model_->nodesInLayer(layer.id);
        for (const auto& nl : nodesInLay)
        {
            if (nl.nodeId == sourceId)
            {
                model_->addNodeToLayer(newId, layer.id);
                break;
            }
        }
    }

    for (const auto& candidate : model_->nodes())
    {
        if (candidate.parentId.has_value() && *candidate.parentId == sourceId)
        {
            cloneNodeRecursive(candidate.id, newId, 0, 0);
        }
    }

    return newId;
}

void MainWindow::copySelectedNode()
{
    if (!model_ || !scene_ || !primaryNode_)
    {
        statusBar()->showMessage("Select a node to copy", 2000);
        return;
    }

    NodeId srcId = primaryNode_->nodeId();
    auto srcOpt = model_->getNodeById(srcId);
    if (!srcOpt)
        return;

    std::optional<NodeId> parentId = srcOpt->parentId;

    NodeId copiedId = cloneNodeRecursive(srcId, parentId, 40.0, 40.0);
    if (copiedId == 0)
    {
        QMessageBox::critical(this, "Error", "Failed to duplicate selected node.");
        return;
    }

    populateNavigator();

    QModelIndex index = navigator_->currentIndex();
    if (index.isValid() && static_cast<ItemType>(index.data(NavRole::Type).toInt()) == ItemType::Layer)
    {
        LayerId layerId = static_cast<LayerId>(index.data(NavRole::Id).toULongLong());
        renderGraph(model_->extractGraph(layerId));
    }
    else
    {
        renderGraph(model_->extractGraph(std::nullopt));
    }

    statusBar()->showMessage(QString("Node duplicated (New ID: %1)").arg(copiedId), 2500);
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
    alignNodes(AlignType::CenterH);
}

void MainWindow::alignVertical()
{
    alignNodes(AlignType::CenterV);
}

void MainWindow::distributeHorizontal()
{
    if (!scene_)
        return;

    std::vector<GraphNodeItem*> nodes;
    for (QGraphicsItem* item : scene_->selectedItems()) {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            nodes.push_back(node);
    }

    if (nodes.size() < 3) {
        statusBar()->showMessage("Select at least 3 nodes to distribute horizontally", 2500);
        return;
    }

    std::sort(nodes.begin(), nodes.end(), [](GraphNodeItem* a, GraphNodeItem* b) {
        return a->mapToScene(a->boundingRect().center()).x() <
               b->mapToScene(b->boundingRect().center()).x();
    });

    qreal minX = nodes.front()->mapToScene(nodes.front()->boundingRect().center()).x();
    qreal maxX = nodes.back()->mapToScene(nodes.back()->boundingRect().center()).x();
    qreal step = (maxX - minX) / static_cast<qreal>(nodes.size() - 1);

    for (size_t i = 1; i + 1 < nodes.size(); ++i) {
        GraphNodeItem* node = nodes[i];
        qreal targetSceneX = minX + i * step;

        qreal targetParentX = targetSceneX;
        if (node->parentItem()) {
            QPointF p = node->parentItem()->mapFromScene(QPointF(targetSceneX, 0));
            targetParentX = p.x();
        }

        qreal halfW = node->boundingRect().width() / 2.0;
        qreal newX = targetParentX - halfW - node->boundingRect().left();
        node->setPos(newX, node->pos().y());

        for (auto* edge : node->edges()) {
            if (edge && edge->scene())
                edge->updateEndpoints();
        }

        if (auto* parentContainer = dynamic_cast<GraphNodeItem*>(node->parentItem()))
            parentContainer->refreshGeometry();
    }

    statusBar()->showMessage("Distributed horizontally", 2000);
}

void MainWindow::distributeVertical()
{
    if (!scene_)
        return;

    std::vector<GraphNodeItem*> nodes;
    for (QGraphicsItem* item : scene_->selectedItems()) {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            nodes.push_back(node);
    }

    if (nodes.size() < 3) {
        statusBar()->showMessage("Select at least 3 nodes to distribute vertically", 2500);
        return;
    }

    std::sort(nodes.begin(), nodes.end(), [](GraphNodeItem* a, GraphNodeItem* b) {
        return a->mapToScene(a->boundingRect().center()).y() <
               b->mapToScene(b->boundingRect().center()).y();
    });

    qreal minY = nodes.front()->mapToScene(nodes.front()->boundingRect().center()).y();
    qreal maxY = nodes.back()->mapToScene(nodes.back()->boundingRect().center()).y();
    qreal step = (maxY - minY) / static_cast<qreal>(nodes.size() - 1);

    for (size_t i = 1; i + 1 < nodes.size(); ++i) {
        GraphNodeItem* node = nodes[i];
        qreal targetSceneY = minY + i * step;

        qreal targetParentY = targetSceneY;
        if (node->parentItem()) {
            QPointF p = node->parentItem()->mapFromScene(QPointF(0, targetSceneY));
            targetParentY = p.y();
        }

        qreal halfH = node->boundingRect().height() / 2.0;
        qreal newY = targetParentY - halfH - node->boundingRect().top();
        node->setPos(node->pos().x(), newY);

        for (auto* edge : node->edges()) {
            if (edge && edge->scene())
                edge->updateEndpoints();
        }

        if (auto* parentContainer = dynamic_cast<GraphNodeItem*>(node->parentItem()))
            parentContainer->refreshGeometry();
    }

    statusBar()->showMessage("Distributed vertically", 2000);
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

void MainWindow::alignNodes(AlignType type)
{
    if (!primaryNode_ || !scene_) {
        statusBar()->showMessage("Select a primary node first", 2000);
        return;
    }

    auto selected = scene_->selectedItems();
    if (selected.size() < 2) {
        statusBar()->showMessage("Select at least 2 nodes to align", 2000);
        return;
    }

    QRectF primarySceneRect = primaryNode_->mapToScene(primaryNode_->boundingRect()).boundingRect();

    qreal targetSceneX = 0;
    qreal targetSceneY = 0;

    switch (type) {
    case AlignType::Left:
        targetSceneX = primarySceneRect.left();
        break;
    case AlignType::Right:
        targetSceneX = primarySceneRect.right();
        break;
    case AlignType::CenterH:
        targetSceneX = primarySceneRect.center().x();
        break;
    case AlignType::Top:
        targetSceneY = primarySceneRect.top();
        break;
    case AlignType::Bottom:
        targetSceneY = primarySceneRect.bottom();
        break;
    case AlignType::CenterV:
        targetSceneY = primarySceneRect.center().y();
        break;
    }

    for (auto* item : selected) {
        auto* node = dynamic_cast<GraphNodeItem*>(item);
        if (!node || node == primaryNode_)
            continue;

        QRectF localBounds = node->boundingRect();
        QPointF curPos = node->pos();

        switch (type) {
        case AlignType::Left: {
            qreal targetParentX = targetSceneX;
            if (node->parentItem())
                targetParentX = node->parentItem()->mapFromScene(QPointF(targetSceneX, 0)).x();
            node->setPos(targetParentX - localBounds.left(), curPos.y());
            break;
        }
        case AlignType::Right: {
            qreal targetParentX = targetSceneX;
            if (node->parentItem())
                targetParentX = node->parentItem()->mapFromScene(QPointF(targetSceneX, 0)).x();
            node->setPos(targetParentX - localBounds.right(), curPos.y());
            break;
        }
        case AlignType::CenterH: {
            qreal targetParentX = targetSceneX;
            if (node->parentItem())
                targetParentX = node->parentItem()->mapFromScene(QPointF(targetSceneX, 0)).x();
            node->setPos(targetParentX - (localBounds.width() / 2.0) - localBounds.left(), curPos.y());
            break;
        }
        case AlignType::Top: {
            qreal targetParentY = targetSceneY;
            if (node->parentItem())
                targetParentY = node->parentItem()->mapFromScene(QPointF(0, targetSceneY)).y();
            node->setPos(curPos.x(), targetParentY - localBounds.top());
            break;
        }
        case AlignType::Bottom: {
            qreal targetParentY = targetSceneY;
            if (node->parentItem())
                targetParentY = node->parentItem()->mapFromScene(QPointF(0, targetSceneY)).y();
            node->setPos(curPos.x(), targetParentY - localBounds.bottom());
            break;
        }
        case AlignType::CenterV: {
            qreal targetParentY = targetSceneY;
            if (node->parentItem())
                targetParentY = node->parentItem()->mapFromScene(QPointF(0, targetSceneY)).y();
            node->setPos(curPos.x(), targetParentY - (localBounds.height() / 2.0) - localBounds.top());
            break;
        }
        }

        for (auto* edge : node->edges()) {
            if (edge && edge->scene())
                edge->updateEndpoints();
        }

        if (auto* parentContainer = dynamic_cast<GraphNodeItem*>(node->parentItem()))
            parentContainer->refreshGeometry();
    }

    statusBar()->showMessage("Nodes aligned", 2000);
}