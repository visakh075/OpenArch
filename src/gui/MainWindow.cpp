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

void MainWindow::renderGraph(const GraphSnapshot& snap)
{
    scene_->clear();

    const int r = 20;
    const int spacing = 90;

    std::unordered_map<NodeId, QPointF> pos;
    int i = 0;

    // ---- Nodes ----
    for (const auto& n : snap.nodes) {
        QPointF p((i % 6) * spacing,
                  (i / 6) * spacing);

        pos[n.id] = p;

        scene_->addEllipse(
            p.x(), p.y(),
            r * 2, r * 2,
            QPen(Qt::black),
            QBrush(Qt::lightGray));

        auto* text = scene_->addText(
            QString::fromStdString(n.name));
        text->setPos(p.x(), p.y() + r * 2);

        ++i;
    }

    // ---- Edges ----
    for (const auto& e : snap.edges) {
        if (!pos.count(e.srcNode) || !pos.count(e.dstNode))
            continue;

        scene_->addLine(
            QLineF(pos[e.srcNode] + QPointF(r, r),
                   pos[e.dstNode] + QPointF(r, r)),
            QPen(Qt::black));
    }

    graphView_->fitInView(
        scene_->itemsBoundingRect(),
        Qt::KeepAspectRatio);
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
