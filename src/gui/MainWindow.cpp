#include "MainWindow.h"

#include <QSplitter>
#include <QMenuBar>
#include <QFileDialog>
#include <QStatusBar>
#include <QMessageBox>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QVBoxLayout>

#include <QDebug>

#include <unordered_map>

#include "gui/LayerEditorWidget.h"
#include "gui/NodeEditorWidget.h"
#include "gui/RelationEditorWidget.h"

/* ============================================================
   Constructor / Destructor
   ============================================================ */

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {

    setupUi();
    setupMenu();
    setupConnections();

    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow() {
    delete model_;
}

/* ============================================================
   UI setup
   ============================================================ */

void MainWindow::setupUi() {
    auto* splitter = new QSplitter(this);

    /* -------- Navigator (left) -------- */
    navigator_ = new QTreeView(splitter);
    navModel_ = new QStandardItemModel(this);
    navModel_->setHorizontalHeaderLabels({"Architecture"});
    navigator_->setModel(navModel_);

    /* -------- Right pane (graph + editors) -------- */
    auto* rightPane = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    /* Graph */
    scene_ = new QGraphicsScene(this);
    graphView_ = new QGraphicsView(scene_, rightPane);
    graphView_->setRenderHint(QPainter::Antialiasing);
    rightLayout->addWidget(graphView_, 1);

    /* Editors (already implemented by you) */
    nodeEditor_ = new NodeEditorWidget(rightPane);
    layerEditor_ = new LayerEditorWidget(rightPane);
    relationEditor_ = new RelationEditorWidget(rightPane);

    /* Hide initially */
    // nodeEditor_->hide();
    // layerEditor_->hide();
    // relationEditor_->hide();

    rightLayout->addWidget(nodeEditor_);
    rightLayout->addWidget(layerEditor_);
    rightLayout->addWidget(relationEditor_);

    /* Splitter layout */
    splitter->addWidget(navigator_);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(1, 1);

    setCentralWidget(splitter);
    resize(1000, 600);
}

/* ============================================================
   Menu
   ============================================================ */

void MainWindow::setupMenu() {
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* openAct = fileMenu->addAction("Open DB");
    connect(openAct, &QAction::triggered,
            this, &MainWindow::openDatabase);

    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu("&Edit");

    auto* addNodeAct  = editMenu->addAction("Add Node");
    auto* addLayerAct = editMenu->addAction("Add Layer");

    connect(addNodeAct,  &QAction::triggered, this, &MainWindow::createNewNode);
    connect(addLayerAct, &QAction::triggered, this, &MainWindow::createNewLayer);

}

/* ============================================================
   Connections (IMPORTANT)
   ============================================================ */

void MainWindow::setupConnections() {
    /* Tree selection → graph refresh */
    connect(navigator_->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            &MainWindow::onTreeSelectionChanged);

    /* -------- Layer editor -------- */
    connect(layerEditor_, &LayerEditorWidget::saveLayerRequested,
            this, &MainWindow::onSaveLayer);

    connect(layerEditor_, &LayerEditorWidget::addNodeRequested,
            this, &MainWindow::onAddNodeToLayer);

    connect(layerEditor_, &LayerEditorWidget::removeNodeRequested,
            this, &MainWindow::onRemoveNodeFromLayer);

    /* -------- Node editor -------- */
    connect(nodeEditor_, &NodeEditorWidget::saveNodeRequested,
            this, &MainWindow::onSaveNode);

    connect(nodeEditor_, &NodeEditorWidget::addLayerRequested,
            this, &MainWindow::onAddLayerToNode);

    connect(nodeEditor_, &NodeEditorWidget::removeLayerRequested,
            this, &MainWindow::onRemoveLayerFromNode);

}

/* ============================================================
   Database handling
   ============================================================ */

void MainWindow::openDatabase() {
    QString file = QFileDialog::getOpenFileName(
        this, "Open DB", "", "SQLite DB (*.db);;All Files (*)");

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
}

/* ============================================================
   Navigator population
   ============================================================ */

void MainWindow::populateNavigator()
{
    navModel_->clear();
    navModel_->setHorizontalHeaderLabels({ "Architecture" });

    auto* root = navModel_->invisibleRootItem();

    /* ======================================================
       All Nodes
       ====================================================== */
    auto* allNodesItem = new QStandardItem("All Nodes");
    allNodesItem->setData(QVariant("all_nodes"), Qt::UserRole);

    for (const auto& node : model_->nodes()) {
        auto* nodeItem = new QStandardItem(
            QString("%1").arg(QString::fromStdString(node.name)));
        nodeItem->setData(QVariant::fromValue<qulonglong>(node.id),
                          Qt::UserRole);
        allNodesItem->appendRow(nodeItem);
    }

    root->appendRow(allNodesItem);

    /* ======================================================
       All Layers
       ====================================================== */
    auto* allLayersItem = new QStandardItem("All Layers");
    allLayersItem->setData(QVariant("all_layers"), Qt::UserRole);
    root->appendRow(allLayersItem);

    for (const auto& layer : model_->layers()) {
        auto* layerItem = new QStandardItem(
            QString("%1").arg(QString::fromStdString(layer.name)));
        layerItem->setData(QVariant::fromValue<qulonglong>(layer.id),
                           Qt::UserRole);

        // Nodes in this layer
        for (const auto& nl : model_->nodesInLayer(layer.id)) {
            auto nodeOpt = model_->getNodeById(nl.nodeId);
            if (!nodeOpt)
                continue;

            auto* nodeItem = new QStandardItem(
                QString::fromStdString(nodeOpt->name));
            nodeItem->setData(QVariant::fromValue<qulonglong>(nodeOpt->id),
                              Qt::UserRole);

            layerItem->appendRow(nodeItem);
        }

        allLayersItem->appendRow(layerItem);
    }

    navigator_->expandAll();
}

/* ============================================================
   Tree selection
   ============================================================ */

void MainWindow::hideAllEditors()
{
    nodeEditor_->hide();
    layerEditor_->hide();
    relationEditor_->hide();
}

void MainWindow::showLayerEditor(const LayerData& layer)
{
    hideAllEditors();

    // ---- collect ALL node names ----
    QStringList allNodes;
    for (const auto& n : model_->nodes()) {
        allNodes << QString::fromStdString(n.name);
    }

    // ---- collect nodes IN THIS layer ----
    QStringList currentNodes;
    for (const auto& nl : model_->nodesInLayer(layer.id)) {
        auto nodeOpt = model_->getNodeById(nl.nodeId);
        if (nodeOpt) {
            currentNodes << QString::fromStdString(nodeOpt->name);
        }
    }

    // ---- pass everything to editor ----
    layerEditor_->setLayer(layer, allNodes, currentNodes);
    layerEditor_->show();
}

void MainWindow::showNodeEditor(const NodeData& node)
{
    hideAllEditors();

    // ---- collect ALL layers ----
    QStringList allLayers;
    for (const auto& l : model_->layers()) {
        allLayers << QString::fromStdString(l.name);
    }

    // ---- collect layers THIS node belongs to ----
    QStringList currentLayers;
    for (const auto& nl : model_->layersForNode(node.id)) {
        auto layerOpt = model_->getLayerById(nl.layerId);
        if (layerOpt) {
            currentLayers << QString::fromStdString(layerOpt->name);
        }
    }

    nodeEditor_->setNode(node, allLayers, currentLayers);
    nodeEditor_->show();
}

/*
void MainWindow::onTreeSelectionChanged(const QModelIndex& index)
{
    qDebug() << "---- Tree selection changed ----";

    if (!index.isValid()) {
        qDebug() << "Invalid index";
    } else {
        qDebug() << "Row:" << index.row()
                 << "Col:" << index.column()
                 << "Text:" << index.data().toString();
    }

    // hideAllEditors();

    if (!index.isValid() || !model_) {
        qDebug() << "invalid index";
        renderGraph(model_->extractGraph(std::nullopt));
        return;
    }

    auto* item = navModel_->itemFromIndex(index);
    if (!item) {
        qDebug() << "invalid item";
        renderGraph(model_->extractGraph(std::nullopt));
        return;
    }

    QVariant data = item->data(Qt::UserRole);

    std::optional<LayerId> layerId;

    // ---- Root / category items ----
    if (!data.isValid()) {
        // All layers
        qDebug() << "Root";
        layerId.reset();
    }
    // ---- String markers ("all_nodes", etc.) ----
    else if (data.canConvert<QString>()) {
        qDebug() << "Convert";
        layerId.reset();
    }
    // ---- ID-based selection ----
    else {
        qulonglong id = data.toULongLong();

        if (auto nodeOpt = model_->getNodeById(id)) {
            showNodeEditor(*nodeOpt);
            // Node → show all layers it belongs to
            layerId.reset();
        }
        else if (auto layerOpt = model_->getLayerById(id)) {
            showLayerEditor(*layerOpt);
            layerId = layerOpt->id;
        }
        else {
            layerId.reset();
        }
    }

    // ✅ ALWAYS render graph
    auto snap = model_->extractGraph(layerId);

    statusBar()->showMessage(
        QString("Nodes: %1  Edges: %2")
            .arg(snap.nodes.size())
            .arg(snap.edges.size()));

    renderGraph(snap);
}

*/

void MainWindow::onTreeSelectionChanged(const QModelIndex& index)
{
    hideAllEditors();

    if (!index.isValid() || !model_) {
        renderGraph(model_->extractGraph(std::nullopt));
        return;
    }

    auto* item = navModel_->itemFromIndex(index);
    if (!item) {
        renderGraph(model_->extractGraph(std::nullopt));
        return;
    }

    QVariant data = item->data(Qt::UserRole);

    // ---------- CASE 1: No ID → category / root ----------
    if (!data.isValid()) {
        renderGraph(model_->extractGraph(std::nullopt));
        return;
    }

    // ---------- CASE 2: ID-based selection ----------
    LayerId layerId = 0;
    bool layerSelected = false;

    qulonglong id = data.toULongLong();

    if (auto layerOpt = model_->getLayerById(id)) {
        showLayerEditor(*layerOpt);
        layerId = layerOpt->id;
        layerSelected = true;
    }
    else if (auto nodeOpt = model_->getNodeById(id)) {
        showNodeEditor(*nodeOpt);
    }

    // ---------- Graph rendering ----------
    if (layerSelected) {
        renderGraph(model_->extractGraph(layerId));
    } else {
        renderGraph(model_->extractGraph(std::nullopt));
    }
}

void MainWindow::createNewLayer()
{
    hideAllEditors();

    LayerData layer;          // id == 0 → NEW
    layer.name = "";
    layer.kind = "";
    layer.metadata = "";
    layer.attributes = "";

    QStringList allNodes;
    for (const auto& n : model_->nodes())
        allNodes << QString::fromStdString(n.name);

    layerEditor_->setLayer(layer, allNodes, {});
    layerEditor_->show();
}

void MainWindow::createNewNode()
{
    hideAllEditors();

    NodeData node;            // id == 0 → NEW
    node.name = "";
    node.type = "";
    node.metadata = "";
    node.attributes = "";

    QStringList allLayers;
    for (const auto& l : model_->layers())
        allLayers << QString::fromStdString(l.name);

    nodeEditor_->setNode(node, allLayers, {});
    nodeEditor_->show();
}

std::optional<LayerData> MainWindow::currentLayer() const
{
    return currentLayer_;
}

// LayerConnections
void MainWindow::onSaveLayer(const LayerData& layer)
{
    // Make a mutable copy for the model
    LayerData mutableLayer = layer;

    Result r;
    if (mutableLayer.id == 0) {
        LayerId newId = 0;
        r = model_->addLayer(mutableLayer, newId);
    } else {
        r = model_->updateLayer(mutableLayer);
    }

    if (!r.ok) {
        QMessageBox::critical(
            this,
            "Layer Error",
            QString::fromStdString(r.message)
        );
        return;
    }

    populateNavigator();
    statusBar()->showMessage("Layer saved", 3000);
}

void MainWindow::onAddNodeToLayer(const QString& nodeName)
{
    if (!layerEditor_->isVisible())
        return;

    auto layerOpt = currentLayer();   // we’ll define this below
    if (!layerOpt)
        return;

    NodeId nodeId = 0;
    for (const auto& n : model_->nodes()) {
        if (n.name == nodeName.toStdString()) {
            nodeId = n.id;
            break;
        }
    }

    if (nodeId == 0) {
        QMessageBox::warning(this, "Error", "Node not found");
        return;
    }

    Result r = model_->addNodeToLayer(nodeId, layerOpt->id);
    if (!r.ok) {
        QMessageBox::critical(this, "Error",
                              QString::fromStdString(r.message));
        return;
    }

    showLayerEditor(*layerOpt);  // refresh editor state
}

void MainWindow::onRemoveNodeFromLayer(const QString& nodeName)
{
    auto layerOpt = currentLayer();
    if (!layerOpt)
        return;

    NodeId nodeId = 0;
    for (const auto& n : model_->nodes()) {
        if (n.name == nodeName.toStdString()) {
            nodeId = n.id;
            break;
        }
    }

    if (nodeId == 0)
        return;

    Result r = model_->removeNodeFromLayer(nodeId, layerOpt->id);
    if (!r.ok) {
        QMessageBox::critical(this, "Error",
                              QString::fromStdString(r.message));
        return;
    }

    showLayerEditor(*layerOpt);  // refresh
}
// Node COnnections
void MainWindow::onSaveNode(const NodeData& node)
{
    NodeData mutableNode = node;   // important (same pattern as Layer)

    Result r;
    if (mutableNode.id == 0) {
        NodeId newId = 0;
        r = model_->addNode(mutableNode, newId);
    } else {
        r = model_->updateNode(mutableNode);
    }

    if (!r.ok) {
        QMessageBox::critical(
            this,
            "Node Error",
            QString::fromStdString(r.message)
        );
        return;
    }

    populateNavigator();
    statusBar()->showMessage("Node saved", 3000);
}

void MainWindow::onAddLayerToNode(const QString& layerName)
{
    if (!currentNode_)
        return;

    LayerId layerId = 0;
    for (const auto& l : model_->layers()) {
        if (l.name == layerName.toStdString()) {
            layerId = l.id;
            break;
        }
    }

    if (layerId == 0) {
        QMessageBox::warning(this, "Error", "Layer not found");
        return;
    }

    Result r = model_->addNodeToLayer(currentNode_->id, layerId);
    if (!r.ok) {
        QMessageBox::critical(this, "Error",
                              QString::fromStdString(r.message));
        return;
    }

    showNodeEditor(*currentNode_);   // refresh editor
}

void MainWindow::onRemoveLayerFromNode(const QString& layerName)
{
    if (!currentNode_)
        return;

    LayerId layerId = 0;
    for (const auto& l : model_->layers()) {
        if (l.name == layerName.toStdString()) {
            layerId = l.id;
            break;
        }
    }

    if (layerId == 0)
        return;

    Result r = model_->removeNodeFromLayer(currentNode_->id, layerId);
    if (!r.ok) {
        QMessageBox::critical(this, "Error",
                              QString::fromStdString(r.message));
        return;
    }

    showNodeEditor(*currentNode_);   // refresh
}


/* ============================================================
   Graph rendering
   ============================================================ */

void MainWindow::renderGraph(const GraphSnapshot& snap) {
    scene_->clear();

    const int r = 20;
    const int spacing = 90;

    std::unordered_map<NodeId, QPointF> pos;
    int i = 0;

    /* Nodes */
    for (const auto& n : snap.nodes) {
        QPointF p((i % 6) * spacing, (i / 6) * spacing);
        pos[n.id] = p;

        scene_->addEllipse(p.x(), p.y(), r*2, r*2,
                           QPen(Qt::black),
                           QBrush(Qt::lightGray));

        scene_->addText(QString::fromStdString(n.name))
            ->setPos(p.x(), p.y() + r*2);

        ++i;
    }

    /* Edges */
    for (const auto& e : snap.edges) {
        if (!pos.count(e.srcNode) || !pos.count(e.dstNode))
            continue;

        scene_->addLine(
            QLineF(pos[e.srcNode] + QPointF(r, r),
                   pos[e.dstNode] + QPointF(r, r)),
            QPen(Qt::black));
    }

    graphView_->fitInView(scene_->itemsBoundingRect(),
                          Qt::KeepAspectRatio);
}
