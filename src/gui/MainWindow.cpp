#include "MainWindow.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsTextItem>

#include "NodeEditorDialog.h"
#include "LayerEditorDialog.h"
#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"
#include <QDebug>
#include "GraphView.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenu();
    setupConnections();
}

MainWindow::~MainWindow()
{
    delete model_;
}

void MainWindow::setupUi()
{
    auto* splitter = new QSplitter(this);

    // -------- Navigator --------
    navigator_ = new QTreeView(splitter);
    navModel_ = new QStandardItemModel(this);
    navModel_->setHorizontalHeaderLabels({"Architecture"});
    navigator_->setModel(navModel_);

    // -------- Graph --------
    scene_ = new QGraphicsScene(this);

    // graphView_ = new QGraphicsView(splitter);
    graphView_ = new GraphView(splitter);

    graphView_->setScene(scene_);
    graphView_->setRenderHint(QPainter::Antialiasing);
    graphView_->setInteractive(true);

    graphView_->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);


    // -------- Layout --------
    splitter->addWidget(navigator_);
    splitter->addWidget(graphView_);

    setCentralWidget(splitter);
}


void MainWindow::setupMenu()
{
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* openAct = fileMenu->addAction("Open DB");
    connect(openAct, &QAction::triggered,
            this, &MainWindow::openDatabase);

    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu("&Edit");

    auto* addNodeAct  = editMenu->addAction("Add Node");
    auto* addLayerAct = editMenu->addAction("Add Layer");

    connect(addNodeAct,  &QAction::triggered,
            this, &MainWindow::createNewNode);

    connect(addLayerAct, &QAction::triggered,
            this, &MainWindow::createNewLayer);

    auto* saveLayoutAct = editMenu->addAction("Save Layout");
    
    connect(saveLayoutAct, &QAction::triggered,
        this, &MainWindow::saveLayout);

}


void MainWindow::setupConnections()
{
    connect(navigator_, &QTreeView::doubleClicked,
            this, &MainWindow::onTreeItemDoubleClicked);
    connect(navigator_, &QTreeView::clicked,
        this, &MainWindow::onTreeItemClicked);

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
