#include "MainWindow.h"

#include <QMenuBar>
#include <QFileDialog>
#include <QStatusBar>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
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

    graphView_ = new QWidget(splitter);
    graphView_->setMinimumWidth(400);

    splitter->addWidget(navigator_);
    splitter->addWidget(graphView_);
    splitter->setStretchFactor(1, 1);

    setCentralWidget(splitter);
    resize(1000, 600);
}

void MainWindow::setupMenu() {
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* openAction = fileMenu->addAction("Open DB");
    connect(openAction, &QAction::triggered,
            this, &MainWindow::openDatabase);

    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);
}

void MainWindow::openDatabase() {
    QString file = QFileDialog::getOpenFileName(
        this,
        "Open Architecture Database",
        "",
        "SQLite DB (*.db);;All Files (*)"
    );

    if (file.isEmpty())
        return;

    if (model_) {
        delete model_;
        model_ = nullptr;
    }

    db_.close();
    auto r = db_.open(file.toStdString());
    if (!r.ok) {
        QMessageBox::critical(this, "Error",
                              QString::fromStdString(r.message));
        return;
    }

    model_ = new ArchitectureModel(db_);

    // Populate navigator (temporary)
    navModel_->clear();
    navModel_->setHorizontalHeaderLabels({"Architecture"});

    auto* root = navModel_->invisibleRootItem();
    auto* layersItem = new QStandardItem("Layers");
    root->appendRow(layersItem);

    for (const auto& l : model_->layers()) {
        auto* item = new QStandardItem(
            QString("%1 (%2)").arg(l.name.c_str()).arg(l.id)
        );
        layersItem->appendRow(item);
    }

    navigator_->expandAll();
    statusBar()->showMessage("DB opened: " + file);
}
