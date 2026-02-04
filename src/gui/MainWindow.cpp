
#include "MainWindow.h"

#include <QSplitter>
#include <QMenuBar>
#include <QFileDialog>
#include <QStatusBar>
#include <QMessageBox>

#include "MainWindow.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>   // ✅ ADD THIS
#include <QVBoxLayout>

#include <unordered_map>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    setupMenu();
    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow() {
    delete model_;
}

void MainWindow::setupUi() {
    auto* splitter = new QSplitter(this);

    navigator_ = new QTreeView(splitter);
    navModel_ = new QStandardItemModel(this);
    navModel_->setHorizontalHeaderLabels({"Architecture"});
    navigator_->setModel(navModel_);

    connect(navigator_->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            &MainWindow::onTreeSelectionChanged);

    scene_ = new QGraphicsScene(this);
    graphView_ = new QGraphicsView(scene_, splitter);
    graphView_->setRenderHint(QPainter::Antialiasing);

    splitter->addWidget(navigator_);
    splitter->addWidget(graphView_);
    splitter->setStretchFactor(1, 1);

    setCentralWidget(splitter);
    resize(1000, 600);
}

void MainWindow::setupMenu() {
    auto* fileMenu = menuBar()->addMenu("&File");
    auto* openAct = fileMenu->addAction("Open DB");
    connect(openAct, &QAction::triggered, this, &MainWindow::openDatabase);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);
}

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

void MainWindow::populateNavigator() {
    navModel_->clear();
    navModel_->setHorizontalHeaderLabels({"Architecture"});
    auto* root = navModel_->invisibleRootItem();

    auto* allItem = new QStandardItem("All Layers");
    allItem->setData(QVariant(), Qt::UserRole);
    root->appendRow(allItem);

    for (const auto& layer : model_->layers()) {
        auto* layerItem = new QStandardItem(
            QString("%1 (%2)").arg(layer.name.c_str()).arg(layer.id));
        layerItem->setData((qulonglong)layer.id, Qt::UserRole);

        auto snap = model_->extractGraph(layer.id);
        for (const auto& node : snap.nodes) {
            auto* nItem = new QStandardItem(
                QString("%1 (%2)").arg(node.name.c_str()).arg(node.id));
            layerItem->appendRow(nItem);
        }
        root->appendRow(layerItem);
    }
    navigator_->expandAll();
}

void MainWindow::onTreeSelectionChanged(const QModelIndex& index) {
    if (!index.isValid() || !model_)
        return;

    std::optional<LayerId> layerId;
    QVariant data = index.data(Qt::UserRole);
    if (data.isValid())
        layerId = (LayerId)data.toULongLong();

    auto snap = model_->extractGraph(layerId);
    statusBar()->showMessage(
        QString("Nodes: %1  Edges: %2")
            .arg(snap.nodes.size())
            .arg(snap.edges.size()));

    renderGraph(snap);
}

void MainWindow::renderGraph(const GraphSnapshot& snap) {
    scene_->clear();
    const int r = 20, spacing = 80;

    std::unordered_map<NodeId, QPointF> pos;
    int i = 0;

    for (const auto& n : snap.nodes) {
        QPointF p((i % 6) * spacing, (i / 6) * spacing);
        pos[n.id] = p;

        scene_->addEllipse(p.x(), p.y(), r*2, r*2,
                           QPen(Qt::black), QBrush(Qt::lightGray));
        scene_->addText(QString::fromStdString(n.name))
            ->setPos(p.x(), p.y() + r*2);
        ++i;
    }

    for (const auto& e : snap.edges) {
        if (!pos.count(e.srcNode) || !pos.count(e.dstNode))
            continue;
        scene_->addLine(QLineF(pos[e.srcNode] + QPointF(r, r),
                               pos[e.dstNode] + QPointF(r, r)),
                         QPen(Qt::black));
    }

    graphView_->fitInView(scene_->itemsBoundingRect(),
                          Qt::KeepAspectRatio);
}
