#include "MainWindow.h"

#include <QSplitter>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QJsonDocument>
#include <QJsonObject>

#include "NodeEditorDialog.h"
#include "LayerEditorDialog.h"
#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"

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
    delete model_;
}

void MainWindow::setupUi()
{
    auto* splitter = new QSplitter(this);

    navigator_ = new QTreeView(splitter);
    navModel_ = new QStandardItemModel(this);
    navModel_->setHorizontalHeaderLabels({"Architecture"});
    navigator_->setModel(navModel_);

    scene_ = new QGraphicsScene(this);

    graphView_ = new GraphView(splitter);
    graphView_->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    graphView_->setDragMode(QGraphicsView::RubberBandDrag);
    // graphView_->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);

    graphView_->setScene(scene_);
    graphView_->setRenderHint(QPainter::Antialiasing);
    graphView_->setInteractive(true);

    splitter->addWidget(navigator_);
    splitter->addWidget(graphView_);

    setCentralWidget(splitter);
}

void MainWindow::setupToolbar()
{
    graphToolBar_ = addToolBar("Graph Modes");

    actionView_ = graphToolBar_->addAction("View (V)");
    actionAdd_  = graphToolBar_->addAction("Add (A)");
    actionArch_ = graphToolBar_->addAction("Arch (R)");
    actionConn_ = graphToolBar_->addAction("Connect (C)");

    actionView_->setCheckable(true);
    actionAdd_->setCheckable(true);
    actionArch_->setCheckable(true);
    actionConn_->setCheckable(true);

    QActionGroup* group = new QActionGroup(this);
    group->addAction(actionView_);
    group->addAction(actionAdd_);
    group->addAction(actionArch_);
    group->addAction(actionConn_);

    actionView_->setChecked(true);

    connect(actionView_, &QAction::triggered,
            this, [this]() { setGraphMode(GraphView::Mode::View); });

    connect(actionAdd_, &QAction::triggered,
            this, [this]() { setGraphMode(GraphView::Mode::Add); });

    connect(actionArch_, &QAction::triggered,
            this, [this]() { setGraphMode(GraphView::Mode::Arch); });

    connect(actionConn_, &QAction::triggered,
            this, [this]() { setGraphMode(GraphView::Mode::Connect); });

    actionView_->setShortcut(Qt::Key_V);
    actionAdd_->setShortcut(Qt::Key_A);
    actionArch_->setShortcut(Qt::Key_R);
    actionConn_->setShortcut(Qt::Key_C);
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
}

void MainWindow::setupConnections()
{
    connect(navigator_, &QTreeView::doubleClicked,
            this, &MainWindow::onTreeItemDoubleClicked);

    connect(navigator_, &QTreeView::clicked,
            this, &MainWindow::onTreeItemClicked);

    connect(graphView_, &GraphView::requestAddNode,
            this, &MainWindow::handleAddNodeAtPosition);

    connect(graphView_, &GraphView::requestConnectNodes,
            this, &MainWindow::handleConnectNodes);
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
        if (!model_) {
        qWarning() << "renderGraph called with null model";
        return;
    }
    scene_->clear();

    std::unordered_map<NodeId, GraphNodeItem*> nodeItems;

    // 1️⃣ Create nodes
    int i = 0;
    for (const auto& n : snap.nodes) {
        auto* nodeItem = new GraphNodeItem(model_, n.id);
        //nodeItem->setPos((i % 5) * 180, (i / 5) * 120);
        
        auto nodeOpt = model_->getNodeById(n.id);
        bool restored = false;

        if (nodeOpt && !nodeOpt->metadata.empty()) {
            QJsonDocument doc =
                QJsonDocument::fromJson(
                    QString::fromStdString(nodeOpt->metadata).toUtf8());

            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("x") && obj.contains("y")) {
                    nodeItem->setPos(obj["x"].toDouble(),
                                    obj["y"].toDouble());
                    restored = true;
                }
            }
        }

        if (!restored)
            nodeItem->setPos((i % 5) * 180,
                            (i / 5) * 120);

        scene_->addItem(nodeItem);

        nodeItems[n.id] = nodeItem;
        ++i;
    }

    // 2️⃣ Create edges (CONNECTED to nodes)
    for (const auto& e : snap.edges) {
        auto srcIt = nodeItems.find(e.srcNode);
        auto dstIt = nodeItems.find(e.dstNode);

        if (srcIt == nodeItems.end() || dstIt == nodeItems.end())
            continue;

        auto* edgeItem = new GraphEdgeItem(
            model_,
            e,
            srcIt->second,
            dstIt->second
        );

        scene_->addItem(edgeItem);
    }

    graphView_->fitInView(
        scene_->itemsBoundingRect(),
        Qt::KeepAspectRatio
    );
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

            populateNavigator();                    // 🔥 FIX
            renderGraph(model_->extractGraph(layerId));
            return;
        }
    }

    populateNavigator();                            // 🔥 FIX
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

    statusBar()->showMessage(text);
}
