#include "MainWindow.h"

#include <QSplitter>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QCoreApplication>
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
#include <unordered_map>
#include <functional>

#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ArchitectureFilterProxyModel.h"
#include "NodeEditorDialog.h"
#include "LayerEditorDialog.h"
#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"
#include "GraphThemeManager.h"
#include "ThemeEditorDock.h"
#include "ShortcutManager.h"
#include "ShortcutConfigDialog.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenu();
    setupToolbar();
    setupShortcuts();
    setupConnections();

    showWelcome();
}

MainWindow::~MainWindow()
{
    if (autoSaveTimer_)
        autoSaveTimer_->stop();

    if (scene_)
        scene_->disconnect(this);

    delete model_;
}

void MainWindow::setupUi()
{
    navModel_ = new QStandardItemModel(this);
    navModel_->setHorizontalHeaderLabels({"Architecture"});

    navProxyModel_ = new ArchitectureFilterProxyModel(this);
    navProxyModel_->setSourceModel(navModel_);

    navigator_ = new QTreeView(this);
    navigator_->setModel(navProxyModel_);
    navigator_->setHeaderHidden(false);

    auto* navContainer = new QWidget(this);
    auto* navLayout = new QVBoxLayout(navContainer);
    navLayout->setContentsMargins(6, 6, 6, 6);
    navLayout->setSpacing(6);

    // Search bar
    treeSearchEdit_ = new QLineEdit(navContainer);
    treeSearchEdit_->setPlaceholderText("Search architecture...");
    treeSearchEdit_->setClearButtonEnabled(true);

    // Filter bar
    treeFilterCombo_ = new QComboBox(navContainer);
    treeFilterCombo_->addItem("All Categories", 0);
    treeFilterCombo_->addItem("Nodes Only", 1);
    treeFilterCombo_->addItem("Layers Only", 2);

    // Status label
    treeStatusLabel_ = new QLabel(navContainer);
    treeStatusLabel_->setStyleSheet("color: #888888; font-size: 11px; padding: 2px;");

    navLayout->addWidget(treeSearchEdit_);
    navLayout->addWidget(treeFilterCombo_);
    navLayout->addWidget(navigator_, 1);
    navLayout->addWidget(treeStatusLabel_);

    architectureDock_ = new QDockWidget("Architecture", this);
    architectureDock_->setObjectName("ArchitectureDock");
    architectureDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    architectureDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    architectureDock_->setWidget(navContainer);
    addDockWidget(Qt::LeftDockWidgetArea, architectureDock_);

    scene_ = new QGraphicsScene(this);

    graphView_ = new GraphView(this);
    graphView_->setScene(scene_);
    graphView_->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    graphView_->setRenderHint(QPainter::Antialiasing);
    graphView_->setInteractive(true);

    centralStack_ = new QStackedWidget(this);
    welcomeWidget_ = new WelcomeWidget(this);

    centralStack_->addWidget(welcomeWidget_);
    centralStack_->addWidget(graphView_);
    setCentralWidget(centralStack_);

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
                // Force full visual, layout, and geometric invalidation across all items
                for (QGraphicsItem* item : scene_->items())
                {
                    if (auto* node = dynamic_cast<GraphNodeItem*>(item))
                        node->onThemeChanged();
                    else if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
                        edge->onThemeChanged();
                }
                scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
            }

            if (graphView_)
                graphView_->viewport()->update();
        });

    setDockNestingEnabled(true);
}

void MainWindow::showWelcome()
{
    centralStack_->setCurrentWidget(welcomeWidget_);
    if (architectureDock_) architectureDock_->hide();
    if (graphToolBar_) graphToolBar_->setEnabled(false);
    if (layoutToolBar_) layoutToolBar_->setEnabled(false);
}

void MainWindow::showCanvas()
{
    centralStack_->setCurrentWidget(graphView_);
    if (architectureDock_) architectureDock_->show();
    if (graphToolBar_) graphToolBar_->setEnabled(true);
    if (layoutToolBar_) layoutToolBar_->setEnabled(true);
}

void MainWindow::setupMenu()
{
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("New Project...", this, &MainWindow::createNewDatabase);
    fileMenu->addAction("Open DB...", this, &MainWindow::openDatabase);
    fileMenu->addSeparator();

    actionExportCurrent_ = new QAction("Export Current View", this);
    connect(actionExportCurrent_, &QAction::triggered, this, [this]() {
        graphView_->exportToSvg(GraphView::ExportMode::CurrentView);
    });
    fileMenu->addAction(actionExportCurrent_);

    actionExportWhole_ = new QAction("Export Whole Diagram", this);
    connect(actionExportWhole_, &QAction::triggered, this, [this]() {
        graphView_->exportToSvg(GraphView::ExportMode::WholeScene);
    });
    fileMenu->addAction(actionExportWhole_);

    actionExportHtml_ = new QAction("Export Interactive HTML...", this);
    connect(actionExportHtml_, &QAction::triggered, this, [this]() {
        graphView_->exportToInteractiveHtml();
    });
    fileMenu->addAction(actionExportHtml_);

    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu("&Edit");
    actionAddNode_ = editMenu->addAction("Add Node", this, &MainWindow::createNewNode);
    actionAddLayer_ = editMenu->addAction("Add Layer", this, &MainWindow::createNewLayer);
    editMenu->addSeparator();

    actionCut_ = editMenu->addAction(QIcon(":/icons/cut.svg"), "Cut", this, &MainWindow::cutSelectedNodes);
    actionCut_->setShortcut(QKeySequence::Cut);

    actionCopy_ = editMenu->addAction(QIcon(":/icons/copy.svg"), "Copy", this, &MainWindow::copySelectedNodes);
    actionCopy_->setShortcut(QKeySequence::Copy);

    actionPaste_ = editMenu->addAction(QIcon(":/icons/paste.svg"), "Paste", this, [this]() { pasteNodes(); });
    actionPaste_->setShortcut(QKeySequence::Paste);

    actionDuplicate_ = editMenu->addAction(QIcon(":/icons/copy.svg"), "Duplicate Node", this, &MainWindow::copySelectedNode);

    editMenu->addSeparator();
    actionSaveLayout_ = editMenu->addAction("Save Layout", this, &MainWindow::saveLayout);

    editMenu->addSeparator();
    actionDistH_ = new QAction(QIcon(":/icons/dist-h.svg"), "Distribute Horizontally", this);
    connect(actionDistH_, &QAction::triggered, this, &MainWindow::distributeHorizontal);
    editMenu->addAction(actionDistH_);

    actionDistV_ = new QAction(QIcon(":/icons/dist-v.svg"), "Distribute Vertically", this);
    connect(actionDistV_, &QAction::triggered, this, &MainWindow::distributeVertical);
    editMenu->addAction(actionDistV_);

    auto* alignMenu = editMenu->addMenu("Align");

    auto* alignHMenu = alignMenu->addMenu("Horizontal");
    actionAlignLeft_ = new QAction(QIcon(":/icons/align-left.svg"), "Left", this);
    connect(actionAlignLeft_, &QAction::triggered, this, [this]() { alignNodes(AlignType::Left); });
    alignHMenu->addAction(actionAlignLeft_);

    actionAlignCenterH_ = new QAction(QIcon(":/icons/align-center-h.svg"), "Center", this);
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
    connect(actionAlignCenterV_, &QAction::triggered, this, [this]() { alignNodes(AlignType::CenterV); });
    alignVMenu->addAction(actionAlignCenterV_);

    actionAlignBottom_ = new QAction(QIcon(":/icons/align-bottom.svg"), "Bottom", this);
    connect(actionAlignBottom_, &QAction::triggered, this, [this]() { alignNodes(AlignType::Bottom); });
    alignVMenu->addAction(actionAlignBottom_);

    editMenu->addSeparator();
    actionConnect_ = new QAction(QIcon(":/icons/connect.svg"), "Connect Selected Nodes", this);
    connect(actionConnect_, &QAction::triggered, this, &MainWindow::connectSelectedNodes);
    editMenu->addAction(actionConnect_);

    editMenu->addSeparator();
    editMenu->addAction("Configure Shortcuts...", this, &MainWindow::openShortcutConfigDialog);

    auto* themeMenu = menuBar()->addMenu("&Theme");
    QDir themeDir("themes");
    if (!themeDir.exists()) {
        themeDir = QDir(QCoreApplication::applicationDirPath() + "/themes");
    }
    if (!themeDir.exists()) {
        themeDir = QDir(QCoreApplication::applicationDirPath() + "/../src/gui/theme/themes");
    }
    if (!themeDir.exists()) {
        themeDir = QDir("src/gui/theme/themes");
    }
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

    auto* settingsMenu = menuBar()->addMenu("&Settings");
    settingsMenu->addAction("Configure Shortcuts...", this, &MainWindow::openShortcutConfigDialog);
}

void MainWindow::setupToolbar()
{
    graphToolBar_ = addToolBar("Graph Modes");
    actionView_ = graphToolBar_->addAction("View");
    actionEdit_ = graphToolBar_->addAction("Edit");    

    actionView_->setCheckable(true);
    actionEdit_->setCheckable(true); 

    auto* group = new QActionGroup(this);
    group->addAction(actionView_);
    group->addAction(actionEdit_);
    actionView_->setChecked(true);

    connect(actionView_, &QAction::triggered, this, [this]() { setGraphMode(GraphView::Mode::View); });
    connect(actionEdit_, &QAction::triggered, this, [this]() { setGraphMode(GraphView::Mode::Edit); });

    layoutToolBar_ = addToolBar("Layout");
    layoutToolBar_->setIconSize(QSize(20, 20));

    auto* alignBtn = new QToolButton(this);
    alignBtn->setText("Align");
    alignBtn->setToolTip("Align selected objects relative to the primary node");
    alignBtn->setPopupMode(QToolButton::InstantPopup);
    alignBtn->setIcon(QIcon(":/icons/align-center-h.svg"));

    auto* alignPopup = new QMenu(alignBtn);
    alignPopup->addAction(actionAlignLeft_);
    alignPopup->addAction(actionAlignCenterH_);
    alignPopup->addAction(actionAlignRight_);
    alignPopup->addSeparator();
    alignPopup->addAction(actionAlignTop_);
    alignPopup->addAction(actionAlignCenterV_);
    alignPopup->addAction(actionAlignBottom_);
    alignBtn->setMenu(alignPopup);
    layoutToolBar_->addWidget(alignBtn);

    auto* distBtn = new QToolButton(this);
    distBtn->setText("Distribute");
    distBtn->setToolTip("Distribute selected objects evenly");
    distBtn->setPopupMode(QToolButton::InstantPopup);
    distBtn->setIcon(QIcon(":/icons/dist-h.svg"));

    auto* distPopup = new QMenu(distBtn);
    distPopup->addAction(actionDistH_);
    distPopup->addAction(actionDistV_);
    distBtn->setMenu(distPopup);
    layoutToolBar_->addWidget(distBtn);

    layoutToolBar_->addSeparator();
    layoutToolBar_->addAction(actionConnect_);

    duplicateBtn_ = layoutToolBar_->addAction(QIcon(":/icons/copy.svg"), "Duplicate");
    connect(duplicateBtn_, &QAction::triggered, this, &MainWindow::copySelectedNode);
}

void MainWindow::setupConnections()
{
    connect(welcomeWidget_, &WelcomeWidget::openFileClicked, this, &MainWindow::openDatabase);
    connect(welcomeWidget_, &WelcomeWidget::createNewClicked, this, &MainWindow::createNewDatabase);
    connect(welcomeWidget_, &WelcomeWidget::recentFileSelected, this, [this](const QString& path) {
        setDb(path.toStdString());
    });

    connect(navigator_, &QTreeView::doubleClicked, this, &MainWindow::onTreeItemDoubleClicked);
    connect(navigator_, &QTreeView::clicked, this, &MainWindow::onTreeItemClicked);

    connect(treeSearchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (navProxyModel_) {
            navProxyModel_->setSearchText(text);
            navigator_->expandAll();
            updateNavStatus();
        }
    });

    connect(treeFilterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (navProxyModel_) {
            navProxyModel_->setCategoryFilter(index);
            navigator_->expandAll();
            updateNavStatus();
        }
    });

    auto* findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        if (treeSearchEdit_) {
            treeSearchEdit_->setFocus();
            treeSearchEdit_->selectAll();
        }
    });

    connect(graphView_, &GraphView::requestAddNode, this, &MainWindow::handleAddNodeAtPosition);
    connect(graphView_, &GraphView::requestAddLayer, this, &MainWindow::createNewLayer);
    connect(graphView_, &GraphView::requestConnectNodes, this, &MainWindow::handleConnectNodes);
    connect(graphView_, &GraphView::deleteRequested, this, &MainWindow::deleteSelected);
    connect(scene_, &QGraphicsScene::selectionChanged, this, &MainWindow::onSelectionChanged);

    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::deleteSelected);

    auto* backspaceShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    connect(backspaceShortcut, &QShortcut::activated, this, &MainWindow::deleteSelected);

    auto* cutShortcut = new QShortcut(QKeySequence::Cut, this);
    connect(cutShortcut, &QShortcut::activated, this, &MainWindow::cutSelectedNodes);

    auto* copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, &MainWindow::copySelectedNodes);

    auto* pasteShortcut = new QShortcut(QKeySequence::Paste, this);
    connect(pasteShortcut, &QShortcut::activated, this, [this]() { pasteNodes(); });

    connect(ShortcutManager::instance(), &ShortcutManager::shortcutsChanged, this, &MainWindow::updateShortcutLabels);

    autoSaveTimer_ = new QTimer(this);
    autoSaveTimer_->setSingleShot(true);
    autoSaveTimer_->setInterval(500);
    connect(autoSaveTimer_, &QTimer::timeout, this, &MainWindow::saveLayout);
}

void MainWindow::setupShortcuts()
{
    auto* sm = ShortcutManager::instance();

    // Standard / Reserved Shortcuts (Immutable)
    sm->registerStandardShortcut("std.cut", "Cut", "Standard", QKeySequence::Cut, "Cut selected objects to clipboard");
    sm->registerStandardShortcut("std.copy", "Copy", "Standard", QKeySequence::Copy, "Copy selected objects to clipboard");
    sm->registerStandardShortcut("std.paste", "Paste", "Standard", QKeySequence::Paste, "Paste objects from clipboard");
    sm->registerStandardShortcut("std.delete", "Delete", "Standard", QKeySequence::Delete, "Delete selected objects");
    sm->registerStandardShortcut("std.backspace", "Delete (Backspace)", "Standard", QKeySequence(Qt::Key_Backspace), "Delete selected objects");
    sm->registerStandardShortcut("std.find", "Find in Architecture", "Standard", QKeySequence::Find, "Find and focus search in Architecture Navigator");

    // Configurable Shortcuts
    // Mode
    sm->registerAction("mode.view", "View Mode", "Mode", QKeySequence(Qt::Key_V), actionView_, "Switch canvas interaction to View mode");
    sm->registerAction("mode.edit", "Edit Mode", "Mode", QKeySequence(Qt::Key_L), actionEdit_, "Switch canvas interaction to Edit mode");

    // Edit
    sm->registerAction("edit.duplicate", "Duplicate Node", "Edit", QKeySequence(Qt::CTRL | Qt::Key_D), actionDuplicate_, "Duplicate the currently selected node");
    sm->registerAction("edit.connect", "Connect Selected Nodes", "Edit", QKeySequence(Qt::CTRL | Qt::Key_K), actionConnect_, "Connect the selected nodes with an edge");
    sm->registerAction("edit.add_node", "Add Node", "Edit", QKeySequence(), actionAddNode_, "Create a new node in the architecture");
    sm->registerAction("edit.add_layer", "Add Layer", "Edit", QKeySequence(), actionAddLayer_, "Create a new layer in the architecture");

    // Layout
    sm->registerAction("layout.dist_h", "Distribute Horizontally", "Layout", QKeySequence(Qt::ALT | Qt::Key_H), actionDistH_, "Distribute selected nodes evenly along horizontal axis");
    sm->registerAction("layout.dist_v", "Distribute Vertically", "Layout", QKeySequence(Qt::ALT | Qt::Key_V), actionDistV_, "Distribute selected nodes evenly along vertical axis");
    sm->registerAction("layout.save_layout", "Save Layout", "Layout", QKeySequence(), actionSaveLayout_, "Save node positions to the database");

    // Alignment
    sm->registerAction("align.left", "Align Left", "Alignment", QKeySequence(), actionAlignLeft_, "Align selected nodes to left edge of primary node");
    sm->registerAction("align.center_h", "Align Center (Horizontal)", "Alignment", QKeySequence(Qt::CTRL | Qt::Key_H), actionAlignCenterH_, "Align selected nodes to horizontal center of primary node");
    sm->registerAction("align.right", "Align Right", "Alignment", QKeySequence(), actionAlignRight_, "Align selected nodes to right edge of primary node");
    sm->registerAction("align.top", "Align Top", "Alignment", QKeySequence(), actionAlignTop_, "Align selected nodes to top edge of primary node");
    sm->registerAction("align.center_v", "Align Middle (Vertical)", "Alignment", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), actionAlignCenterV_, "Align selected nodes to vertical center of primary node");
    sm->registerAction("align.bottom", "Align Bottom", "Alignment", QKeySequence(), actionAlignBottom_, "Align selected nodes to bottom edge of primary node");

    // Export
    sm->registerAction("export.current_view", "Export Current View", "Export", QKeySequence(), actionExportCurrent_, "Export visible canvas area to SVG");
    sm->registerAction("export.whole_diagram", "Export Whole Diagram", "Export", QKeySequence(), actionExportWhole_, "Export entire architecture diagram to SVG");
    sm->registerAction("export.html", "Export Interactive HTML", "Export", QKeySequence(), actionExportHtml_, "Export architecture diagram to standalone interactive HTML");

    // Load saved shortcuts from QSettings and apply
    sm->loadSettings();

    // Update dynamic button texts and tooltips
    updateShortcutLabels();
}

void MainWindow::updateShortcutLabels()
{
    auto* sm = ShortcutManager::instance();
    if (!sm) return;

    // View mode
    if (actionView_) {
        QKeySequence viewKey = sm->getShortcut("mode.view");
        QString viewStr = viewKey.toString(QKeySequence::NativeText);
        actionView_->setText(viewStr.isEmpty() ? "View" : QString("View (%1)").arg(viewStr));
        actionView_->setToolTip(viewStr.isEmpty() ? "View Mode" : QString("View Mode (%1)").arg(viewStr));
    }

    // Edit mode
    if (actionEdit_) {
        QKeySequence editKey = sm->getShortcut("mode.edit");
        QString editStr = editKey.toString(QKeySequence::NativeText);
        actionEdit_->setText(editStr.isEmpty() ? "Edit" : QString("Edit (%1)").arg(editStr));
        actionEdit_->setToolTip(editStr.isEmpty() ? "Edit Mode" : QString("Edit Mode (%1)").arg(editStr));
    }

    // Duplicate toolbar button
    if (duplicateBtn_) {
        QKeySequence dupKey = sm->getShortcut("edit.duplicate");
        QString dupStr = dupKey.toString(QKeySequence::NativeText);
        duplicateBtn_->setToolTip(dupStr.isEmpty() ? "Duplicate selected node" : QString("Duplicate selected node (%1)").arg(dupStr));
    }
}

void MainWindow::openShortcutConfigDialog()
{
    ShortcutConfigDialog dialog(this);
    dialog.exec();
}

void MainWindow::scheduleAutoSave()
{
    if (autoSaveTimer_ && model_ && !isRendering_)
    {
        autoSaveTimer_->start();
    }
}

void MainWindow::setDb(const std::string& db_path)
{
    if (db_path.empty())
        return;

    if (autoSaveTimer_)
        autoSaveTimer_->stop();

    clipboardNodes_.clear();
    cutNodeIds_.clear();

    if (scene_)
    {
        scene_->blockSignals(true);

        for (QGraphicsItem* item : scene_->items())
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
        QMessageBox::critical(this, "Database Error", QString::fromStdString(r.message));
        return;
    }

    model_ = new ArchitectureModel(*db_);

    populateNavigator();
    renderGraph(model_->extractGraph(std::nullopt));
    showCanvas();
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

void MainWindow::createNewDatabase()
{
    QString file = QFileDialog::getSaveFileName(
        this,
        "Create New Architecture Project",
        "",
        "SQLite Databases (*.db *.sqlite *.sqlite3);;Architecture JSON (*.json);;All Files (*.*)");

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
        item->setData(QString::fromStdString(n.type), NavRole::Subtype);
        if (!n.type.empty()) {
            item->setToolTip(QString("%1 (%2)").arg(QString::fromStdString(n.name), QString::fromStdString(n.type)));
        }
        nodesRoot->appendRow(item);
    }

    auto* layersRoot = new QStandardItem("Layers");
    layersRoot->setData(static_cast<int>(ItemType::Category), NavRole::Type);

    for (const auto& l : model_->layers()) {
        auto* item = new QStandardItem(QString::fromStdString(l.name));
        item->setData(static_cast<qulonglong>(l.id), NavRole::Id);
        item->setData(static_cast<int>(ItemType::Layer), NavRole::Type);
        item->setData(QString::fromStdString(l.kind), NavRole::Subtype);
        if (!l.kind.empty()) {
            item->setToolTip(QString("%1 [%2]").arg(QString::fromStdString(l.name), QString::fromStdString(l.kind)));
        }
        layersRoot->appendRow(item);
    }

    root->appendRow(nodesRoot);
    root->appendRow(layersRoot);

    if (navProxyModel_) {
        navProxyModel_->invalidate();
    }
    navigator_->expandAll();
    updateNavStatus();
}

void MainWindow::onTreeItemDoubleClicked(const QModelIndex& index)
{
    if (!model_ || !index.isValid()) return;

    QModelIndex srcIndex = navProxyModel_ ? navProxyModel_->mapToSource(index) : index;
    auto* item = navModel_->itemFromIndex(srcIndex);
    if (!item) return;

    auto type = static_cast<ItemType>(item->data(NavRole::Type).toInt());
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
                auto layerId = static_cast<LayerId>(curIdx.data(NavRole::Id).toULongLong());
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
    if (!model_ || !index.isValid()) return;

    QModelIndex srcIndex = navProxyModel_ ? navProxyModel_->mapToSource(index) : index;
    auto* item = navModel_->itemFromIndex(srcIndex);
    if (!item) return;

    auto type = static_cast<ItemType>(item->data(NavRole::Type).toInt());

    if (type == ItemType::Layer) {
        auto layerId = static_cast<LayerId>(item->data(NavRole::Id).toULongLong());
        renderGraph(model_->extractGraph(layerId));
    }
    else {
        renderGraph(model_->extractGraph(std::nullopt));
    }
}

void MainWindow::updateNavStatus()
{
    if (!treeStatusLabel_) return;

    if (!model_) {
        treeStatusLabel_->clear();
        return;
    }

    int totalNodes = static_cast<int>(model_->nodes().size());
    int totalLayers = static_cast<int>(model_->layers().size());

    QString searchText = treeSearchEdit_ ? treeSearchEdit_->text().trimmed() : QString();
    int filterMode = treeFilterCombo_ ? treeFilterCombo_->currentData().toInt() : 0;

    if (searchText.isEmpty() && filterMode == 0) {
        treeStatusLabel_->setText(QString("%1 nodes, %2 layers").arg(totalNodes).arg(totalLayers));
        return;
    }

    int visibleItems = 0;
    if (navProxyModel_) {
        int rootRows = navProxyModel_->rowCount();
        for (int i = 0; i < rootRows; ++i) {
            QModelIndex catIdx = navProxyModel_->index(i, 0);
            visibleItems += navProxyModel_->rowCount(catIdx);
        }
    }

    if (!searchText.isEmpty()) {
        if (visibleItems == 0) {
            treeStatusLabel_->setText("No matching items found");
        } else {
            treeStatusLabel_->setText(QString("Found %1 relevant item%2")
                .arg(visibleItems)
                .arg(visibleItems == 1 ? "" : "s"));
        }
    } else {
        if (filterMode == 1) {
            treeStatusLabel_->setText(QString("%1 nodes").arg(totalNodes));
        } else if (filterMode == 2) {
            treeStatusLabel_->setText(QString("%1 layers").arg(totalLayers));
        }
    }
}

void MainWindow::createNewNode()
{
    if (!model_) return;

    NodeEditorDialog dlg(model_, 0, this);
    dlg.exec();

    populateNavigator();
    renderGraph(model_->extractGraph(std::nullopt));
}

void MainWindow::createNewLayer()
{
    if (!model_) return;

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

    std::unordered_map<NodeId, GraphNodeItem*> existingNodes;
    std::unordered_map<EdgeId, GraphEdgeItem*> existingEdges;

    for (QGraphicsItem* item : scene_->items())
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            existingNodes[node->nodeId()] = node;
        else if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
            existingEdges[edge->edgeId()] = edge;
    }

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

    for (auto& pair : existingNodes)
    {
        if (pair.second)
        {
            pair.second->setParentItem(nullptr);
        }
    }

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

        connect(nodeItem, &QGraphicsObject::xChanged, this, &MainWindow::scheduleAutoSave, Qt::UniqueConnection);
        connect(nodeItem, &QGraphicsObject::yChanged, this, &MainWindow::scheduleAutoSave, Qt::UniqueConnection);

        currentNodes[n.id] = nodeItem;
    }

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
    if (!model_ || !scene_ || isRendering_) return;

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

    statusBar()->showMessage("Layout saved", 1500);
}

void MainWindow::handleAddNodeAtPosition(QPointF pos)
{
    if (!model_) return;

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
        auto type = static_cast<ItemType>(index.data(NavRole::Type).toInt());
        if (type == ItemType::Layer) {
            auto layerId = static_cast<LayerId>(index.data(NavRole::Id).toULongLong());
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
        auto type = static_cast<ItemType>(currentIndex.data(NavRole::Type).toInt());
        if (type == ItemType::Layer)
        {
            auto activeLayerId = static_cast<LayerId>(currentIndex.data(NavRole::Id).toULongLong());
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
        auto activeLayerId = static_cast<LayerId>(currentIndex.data(NavRole::Id).toULongLong());
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
    if (!scene_ || !model_) return;

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
    if (!scene_ || !model_) return;

    const auto selected = scene_->selectedItems();
    if (selected.isEmpty()) return;

    std::unordered_set<NodeId> allNodeIdsToDelete;
    std::unordered_set<GraphNodeItem*> allNodeItemsToDelete;
    std::unordered_set<GraphEdgeItem*> edgesToDelete;

    std::function<void(NodeId)> collectDescendants;
    collectDescendants = [&](NodeId nId) {
        if (allNodeIdsToDelete.count(nId)) return;
        allNodeIdsToDelete.insert(nId);

        for (QGraphicsItem* item : scene_->items())
        {
            if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
            {
                if (nodeItem->nodeId() == nId)
                {
                    allNodeItemsToDelete.insert(nodeItem);
                    break;
                }
            }
        }

        for (const auto& candidate : model_->nodes())
        {
            if (candidate.parentId.has_value() && *candidate.parentId == nId)
            {
                collectDescendants(candidate.id);
            }
        }
    };

    for (QGraphicsItem* item : selected)
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            collectDescendants(node->nodeId());
        else if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
            edgesToDelete.insert(edge);
    }

    for (QGraphicsItem* item : scene_->items())
    {
        if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
        {
            auto edgeData = model_->getEdgeById(edge->edgeId());
            if (edgeData && (allNodeIdsToDelete.count(edgeData->srcNode) || 
                             allNodeIdsToDelete.count(edgeData->dstNode)))
            {
                edgesToDelete.insert(edge);
            }
        }
    }

    auto reply = QMessageBox::question(
        this,
        "Confirm Delete",
        QString("Are you sure you want to delete %1 node(s) (including children) and %2 edge(s)?")
            .arg(allNodeIdsToDelete.size())
            .arg(edgesToDelete.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    for (auto* node : allNodeItemsToDelete)
    {
        if (node)
        {
            node->setParentItem(nullptr);
        }
    }

    for (auto* edge : edgesToDelete)
    {
        if (!edge) continue;
        model_->deleteEdge(edge->edgeId());
        if (edge->scene() == scene_)
            scene_->removeItem(edge);
        delete edge;
    }

    for (NodeId nId : allNodeIdsToDelete)
    {
        model_->deleteNode(nId);
    }

    for (auto* node : allNodeItemsToDelete)
    {
        if (!node) continue;
        if (primaryNode_ == node)
            primaryNode_ = nullptr;

        if (node->scene() == scene_)
            scene_->removeItem(node);
        delete node;
    }

    populateNavigator();

    QModelIndex index = navigator_->currentIndex();
    if (index.isValid() && static_cast<ItemType>(index.data(NavRole::Type).toInt()) == ItemType::Layer)
    {
        auto layerId = static_cast<LayerId>(index.data(NavRole::Id).toULongLong());
        renderGraph(model_->extractGraph(layerId));
    }
    else
    {
        renderGraph(model_->extractGraph(std::nullopt));
    }

    onSelectionChanged();
}

NodeId MainWindow::cloneNodeRecursive(NodeId sourceId, std::optional<NodeId> newParentId, qreal offsetX, qreal offsetY)
{
    if (!model_) return 0;

    auto opt = model_->getNodeById(sourceId);
    if (!opt) return 0;

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
    if (!res.ok) return 0;

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
        statusBar()->showMessage("Select a node to duplicate", 2000);
        return;
    }

    NodeId srcId = primaryNode_->nodeId();
    auto srcOpt = model_->getNodeById(srcId);
    if (!srcOpt) return;

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
        auto layerId = static_cast<LayerId>(index.data(NavRole::Id).toULongLong());
        renderGraph(model_->extractGraph(layerId));
    }
    else
    {
        renderGraph(model_->extractGraph(std::nullopt));
    }

    statusBar()->showMessage(QString("Node duplicated (New ID: %1)").arg(copiedId), 2500);
}

void MainWindow::cutSelectedNodes()
{
    if (!scene_ || !model_) return;

    clipboardNodes_.clear();
    cutNodeIds_.clear();

    std::unordered_set<NodeId> visited;
    std::function<void(NodeId, bool)> collectCutHierarchy;

    collectCutHierarchy = [&](NodeId nId, bool isRoot) {
        if (visited.count(nId)) return;
        visited.insert(nId);

        QPointF scenePos(0, 0);
        for (QGraphicsItem* item : scene_->items())
        {
            if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
            {
                if (nodeItem->nodeId() == nId)
                {
                    scenePos = nodeItem->scenePos();
                    nodeItem->setVisible(false);
                    for (auto* edge : nodeItem->edges())
                    {
                        if (edge) edge->setVisible(false);
                    }
                    break;
                }
            }
        }

        cutNodeIds_.push_back({nId, scenePos, isRoot});

        for (const auto& candidate : model_->nodes())
        {
            if (candidate.parentId.has_value() && *candidate.parentId == nId)
            {
                collectCutHierarchy(candidate.id, false);
            }
        }
    };

    for (QGraphicsItem* item : scene_->selectedItems())
    {
        if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
        {
            collectCutHierarchy(nodeItem->nodeId(), true);
        }
    }

    if (!cutNodeIds_.empty())
    {
        scene_->clearSelection();
        statusBar()->showMessage(QString("Cut %1 node(s) to clipboard").arg(cutNodeIds_.size()), 2500);
    }
}

void MainWindow::copySelectedNodes()
{
    if (!scene_ || !model_) return;

    if (!cutNodeIds_.empty())
    {
        for (const auto& rec : cutNodeIds_)
        {
            for (QGraphicsItem* item : scene_->items())
            {
                if (auto* node = dynamic_cast<GraphNodeItem*>(item))
                {
                    if (node->nodeId() == rec.id)
                    {
                        node->setVisible(true);
                        for (auto* e : node->edges()) if (e) e->setVisible(true);
                    }
                }
            }
        }
        cutNodeIds_.clear();
    }

    clipboardNodes_.clear();
    pasteOffsetMultiplier_ = 1;

    std::function<void(NodeId, bool)> collectNodeAndChildren;
    std::unordered_set<NodeId> visitedIds;

    collectNodeAndChildren = [&](NodeId nId, bool isRoot) {
        if (visitedIds.count(nId)) return;
        visitedIds.insert(nId);

        auto opt = model_->getNodeById(nId);
        if (!opt) return;

        ClipboardNode cn;
        cn.data = *opt;
        cn.isRoot = isRoot;
        cn.localPos = QPointF(0, 0);

        for (QGraphicsItem* item : scene_->items())
        {
            if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
            {
                if (nodeItem->nodeId() == nId)
                {
                    cn.localPos = isRoot ? nodeItem->scenePos() : nodeItem->pos();
                    break;
                }
            }
        }

        for (const auto& layer : model_->layers())
        {
            for (const auto& nl : model_->nodesInLayer(layer.id))
            {
                if (nl.nodeId == nId)
                {
                    cn.layers.push_back(layer.id);
                    break;
                }
            }
        }

        clipboardNodes_.push_back(cn);

        for (const auto& candidate : model_->nodes())
        {
            if (candidate.parentId.has_value() && *candidate.parentId == nId)
            {
                collectNodeAndChildren(candidate.id, false);
            }
        }
    };

    for (QGraphicsItem* item : scene_->selectedItems())
    {
        auto* nodeItem = dynamic_cast<GraphNodeItem*>(item);
        if (!nodeItem) continue;

        collectNodeAndChildren(nodeItem->nodeId(), true);
    }

    if (!clipboardNodes_.empty())
    {
        statusBar()->showMessage(QString("Copied %1 node(s) with hierarchy").arg(clipboardNodes_.size()), 2000);
    }
}

void MainWindow::pasteNodes()
{
    pasteNodesAt(std::nullopt);
}

void MainWindow::pasteNodesAt(const std::optional<QPointF>& targetPos)
{
    if (!model_ || !scene_) return;

    auto findContainerAt = [this](const QPointF& scenePt, const std::unordered_set<NodeId>& excludeIds) -> GraphNodeItem* {
        for (QGraphicsItem* item : scene_->items(scenePt))
        {
            if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            {
                if (node->isContainer() && excludeIds.find(node->nodeId()) == excludeIds.end())
                {
                    return node;
                }
            }
        }
        return nullptr;
    };

    if (!cutNodeIds_.empty())
    {
        std::unordered_set<NodeId> cutIdSet;
        for (const auto& r : cutNodeIds_) cutIdSet.insert(r.id);

        QPointF anchorScenePos(0, 0);
        for (const auto& rec : cutNodeIds_)
        {
            if (rec.isRoot)
            {
                anchorScenePos = rec.originalScenePos;
                break;
            }
        }

        const bool hasExplicitTarget = targetPos.has_value();
        QPointF dropScenePos = hasExplicitTarget ? *targetPos : (anchorScenePos + QPointF(30, 30));

        GraphNodeItem* targetContainer = findContainerAt(dropScenePos, cutIdSet);

        for (const auto& rec : cutNodeIds_)
        {
            if (!rec.isRoot) continue;

            auto nodeOpt = model_->getNodeById(rec.id);
            if (!nodeOpt) continue;

            qreal deltaX = rec.originalScenePos.x() - anchorScenePos.x();
            qreal deltaY = rec.originalScenePos.y() - anchorScenePos.y();
            QPointF thisRootScenePos = dropScenePos + QPointF(deltaX, deltaY);

            qreal finalX = 0;
            qreal finalY = 0;

            if (targetContainer)
            {
                nodeOpt->parentId = targetContainer->nodeId();
                QPointF localPt = targetContainer->mapFromScene(thisRootScenePos);

                qreal minX = 10.0;
                qreal minY = targetContainer->rect().top() + 35.0;
                finalX = std::max(minX, localPt.x());
                finalY = std::max(minY, localPt.y());
            }
            else
            {
                nodeOpt->parentId = std::nullopt;
                finalX = thisRootScenePos.x();
                finalY = thisRootScenePos.y();
            }

            QJsonObject obj;
            if (!nodeOpt->metadata.empty())
            {
                QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(nodeOpt->metadata).toUtf8());
                if (doc.isObject()) obj = doc.object();
            }
            obj["x"] = finalX;
            obj["y"] = finalY;
            nodeOpt->metadata = QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();

            model_->updateNode(*nodeOpt);
        }

        std::vector<NodeId> movedIds;
        for (const auto& r : cutNodeIds_) movedIds.push_back(r.id);
        cutNodeIds_.clear();

        populateNavigator();

        QModelIndex index = navigator_->currentIndex();
        if (index.isValid() && static_cast<ItemType>(index.data(NavRole::Type).toInt()) == ItemType::Layer)
        {
            auto layerId = static_cast<LayerId>(index.data(NavRole::Id).toULongLong());
            renderGraph(model_->extractGraph(layerId));
        }
        else
        {
            renderGraph(model_->extractGraph(std::nullopt));
        }

        for (QGraphicsItem* item : scene_->items())
        {
            if (auto* node = dynamic_cast<GraphNodeItem*>(item))
            {
                node->setVisible(true);
                if (std::find(movedIds.begin(), movedIds.end(), node->nodeId()) != movedIds.end())
                {
                    node->setSelected(true);
                    node->refreshGeometry();
                }
            }
            else if (auto* edge = dynamic_cast<GraphEdgeItem*>(item))
            {
                edge->setVisible(true);
                edge->updateEndpoints();
                edge->refreshPath();
            }
        }

        if (targetContainer) targetContainer->refreshGeometry();
        scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
        graphView_->viewport()->update();

        statusBar()->showMessage(QString("Moved %1 node(s)").arg(movedIds.size()), 2000);
        return;
    }

    if (clipboardNodes_.empty()) return;

    scene_->clearSelection();

    QPointF anchorScenePos = clipboardNodes_.front().localPos;
    for (const auto& cn : clipboardNodes_)
    {
        if (cn.isRoot)
        {
            anchorScenePos = cn.localPos;
            break;
        }
    }

    const bool hasExplicitTarget = targetPos.has_value();
    const qreal defaultOffset = 30.0 * pasteOffsetMultiplier_;
    QPointF dropScenePos = hasExplicitTarget ? *targetPos : (anchorScenePos + QPointF(defaultOffset, defaultOffset));

    GraphNodeItem* targetContainer = findContainerAt(dropScenePos, {});

    std::unordered_map<NodeId, NodeId> oldToNewId;
    std::vector<NodeId> newlyCreatedIds;

    for (const auto& cn : clipboardNodes_)
    {
        NodeData copyData = cn.data;
        copyData.name = cn.data.name + " (Copy)";
        copyData.type = cn.data.type;
        copyData.attributes = cn.data.attributes;
        copyData.status = Status::New;
        copyData.reviewer = "";

        qreal finalX = 0;
        qreal finalY = 0;

        if (cn.isRoot)
        {
            qreal deltaX = cn.localPos.x() - anchorScenePos.x();
            qreal deltaY = cn.localPos.y() - anchorScenePos.y();
            QPointF thisRootScenePos = dropScenePos + QPointF(deltaX, deltaY);

            if (targetContainer)
            {
                copyData.parentId = targetContainer->nodeId();
                QPointF localPt = targetContainer->mapFromScene(thisRootScenePos);
                qreal minX = 10.0;
                qreal minY = targetContainer->rect().top() + 35.0;
                finalX = std::max(minX, localPt.x());
                finalY = std::max(minY, localPt.y());
            }
            else
            {
                copyData.parentId = std::nullopt;
                finalX = thisRootScenePos.x();
                finalY = thisRootScenePos.y();
            }
        }
        else
        {
            finalX = cn.localPos.x();
            finalY = cn.localPos.y();
        }

        QJsonObject obj;
        if (!cn.data.metadata.empty())
        {
            QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(cn.data.metadata).toUtf8());
            if (doc.isObject()) obj = doc.object();
        }
        obj["x"] = finalX;
        obj["y"] = finalY;
        copyData.metadata = QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();

        NodeId newId = 0;
        auto res = model_->addNode(copyData, newId);
        if (!res.ok) continue;

        oldToNewId[cn.data.id] = newId;
        newlyCreatedIds.push_back(newId);

        for (LayerId lid : cn.layers)
        {
            model_->addNodeToLayer(newId, lid);
        }
    }

    for (const auto& cn : clipboardNodes_)
    {
        if (cn.data.parentId.has_value())
        {
            auto pIt = oldToNewId.find(*cn.data.parentId);
            if (pIt != oldToNewId.end())
            {
                NodeId newChildId = oldToNewId[cn.data.id];
                auto nodeOpt = model_->getNodeById(newChildId);
                if (nodeOpt)
                {
                    nodeOpt->parentId = pIt->second;
                    model_->updateNode(*nodeOpt);
                }
            }
        }
    }

    if (!hasExplicitTarget)
    {
        pasteOffsetMultiplier_++;
    }

    populateNavigator();

    QModelIndex index = navigator_->currentIndex();
    if (index.isValid() && static_cast<ItemType>(index.data(NavRole::Type).toInt()) == ItemType::Layer)
    {
        auto layerId = static_cast<LayerId>(index.data(NavRole::Id).toULongLong());
        renderGraph(model_->extractGraph(layerId));
    }
    else
    {
        renderGraph(model_->extractGraph(std::nullopt));
    }

    for (QGraphicsItem* item : scene_->items())
    {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item))
        {
            if (std::find(newlyCreatedIds.begin(), newlyCreatedIds.end(), node->nodeId()) != newlyCreatedIds.end())
            {
                node->setSelected(true);
                node->refreshGeometry();
            }
        }
    }

    if (targetContainer) targetContainer->refreshGeometry();
    scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
    graphView_->viewport()->update();

    statusBar()->showMessage(QString("Pasted %1 node(s)").arg(newlyCreatedIds.size()), 2000);
}

void MainWindow::setGraphMode(GraphView::Mode mode)
{
    if (!graphView_) return;

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
    if (isRendering_ || !scene_) return;

    auto* bar = statusBar();
    if (!bar) return;

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
    if (!scene_) return;

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
    if (!scene_) return;

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