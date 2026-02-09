#include "MainWindow.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsTextItem>

#include "NodeEditorDialog.h"
#include "LayerEditorDialog.h"

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

    navigator_ = new QTreeView(splitter);
    navModel_ = new QStandardItemModel(this);
    navModel_->setHorizontalHeaderLabels({"Architecture"});
    navigator_->setModel(navModel_);

    scene_ = new QGraphicsScene(this);
    graphView_ = new QGraphicsView(scene_, splitter);

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

#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"

void MainWindow::renderGraph(const GraphSnapshot& snap)
{
    scene_->clear();

    std::unordered_map<NodeId, GraphNodeItem*> nodeItems;

    int i = 0;
    const int spacingX = 180;
    const int spacingY = 100;

    // ---- Nodes ----
    for (const auto& n : snap.nodes) {
        auto* item = new GraphNodeItem(model_, n.id);
        item->setPos(
            (i % 5) * spacingX,
            (i / 5) * spacingY);

        scene_->addItem(item);
        nodeItems[n.id] = item;
        ++i;
    }

    // ---- Edges ----
    for (const auto& e : snap.edges) {
        if (!nodeItems.count(e.srcNode) ||
            !nodeItems.count(e.dstNode))
            continue;

        auto* edge = new GraphEdgeItem(model_, e);

        QPointF src =
            nodeItems[e.srcNode]->sceneBoundingRect().center();
        QPointF dst =
            nodeItems[e.dstNode]->sceneBoundingRect().center();

        edge->setEndpoints(src, dst);
        scene_->addItem(edge);
    }

    graphView_->fitInView(
        scene_->itemsBoundingRect(),
        Qt::KeepAspectRatio);
}
