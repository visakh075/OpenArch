#include "MainWindow.h"

#include <QApplication>
#include <QSplitter>
#include <QMenuBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStatusBar>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QShortcut>
#include <QToolButton>
#include <QMenu>
#include <QActionGroup>
#include <QStyle>
#include <QInputDialog>
#include <QKeySequence>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <functional>

#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QUrl>

#include "ArchitectureFilterProxyModel.h"
#include "NodeEditorDialog.h"
#include "LayerEditorDialog.h"
#include "GraphNodeItem.h"
#include "GraphEdgeItem.h"
#include "GraphThemeManager.h"
#include "theme/ApplicationThemeManager.h"
#include "ThemeEditorDock.h"
#include "ThemeEditorWindow.h"
#include "ShortcutManager.h"
#include "ShortcutConfigDialog.h"
#include "ConvertDialog.h"
#include "db/DbConverter.h"
#include "config/ApplicationConfig.h"
#include "PreferencesDialog.h"
#include "ContainerScopeBar.h"
#include "ContainerBoundaryItem.h"
#include "EditorDialogs/ContainerSizeDialog.h"
#include <QFontDialog>
#include <QTime>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    resize(1280, 800);
    setMinimumSize(900, 600);

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
    architectureDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
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

    canvasContainer_ = new QWidget(this);
    auto* canvasLayout = new QVBoxLayout(canvasContainer_);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    canvasLayout->setSpacing(0);

    scopeBar_ = new ContainerScopeBar(this);
    canvasLayout->addWidget(scopeBar_);
    canvasLayout->addWidget(graphView_);

    connect(scopeBar_, &ContainerScopeBar::backClicked, this, &MainWindow::stepUpContainerScope);
    connect(scopeBar_, &ContainerScopeBar::rootClicked, this, &MainWindow::exitContainerScope);
    connect(scopeBar_, &ContainerScopeBar::crumbClicked, this, &MainWindow::focusIntoContainer);
    connect(scopeBar_, &ContainerScopeBar::exitClicked, this, &MainWindow::exitContainerScope);
    connect(scopeBar_, &ContainerScopeBar::manageSizeClicked, this, &MainWindow::openActiveContainerSizeDialog);
    connect(scopeBar_, &ContainerScopeBar::addChildNodeClicked, this, [this]() {
        if (activeContainerScope_.has_value()) {
            QPointF targetPos(100, 100);
            if (scene_ && !scene_->items().isEmpty()) {
                QRectF b = scene_->itemsBoundingRect();
                if (!b.isEmpty()) {
                    targetPos = QPointF(b.right() + 40, b.top());
                }
            }
            handleAddNodeAtPosition(targetPos);
        }
    });
    connect(scopeBar_, &ContainerScopeBar::frameViewClicked, this, [this]() {
        if (scene_ && graphView_) {
            QRectF r = scene_->itemsBoundingRect();
            if (!r.isEmpty()) {
                graphView_->fitInView(r.adjusted(-80, -80, 80, 80), Qt::KeepAspectRatio);
            }
        }
    });

    centralStack_->addWidget(welcomeWidget_);
    centralStack_->addWidget(canvasContainer_);
    setCentralWidget(centralStack_);

    themeDock_ = new ThemeEditorDock(this);
    themeDock_->setObjectName("ThemeDock");
    themeDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    themeDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, themeDock_);

    tabifyDockWidget(architectureDock_, themeDock_);
    architectureDock_->raise();

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

    connect(
        GraphThemeManager::instance(),
        &GraphThemeManager::themeSaved,
        this,
        [this](const QString&)
        {
            populateThemeMenu();
        });

    connect(
        ApplicationThemeManager::instance(),
        &ApplicationThemeManager::themeChanged,
        this,
        [this](const QString&)
        {
            if (appThemeMenu_)
                setupApplicationThemeMenu(appThemeMenu_);
        });

    setDockNestingEnabled(true);
    setupSaveStatusWidget();
}

void MainWindow::showWelcome()
{
    centralStack_->setCurrentWidget(welcomeWidget_);
    if (architectureDock_) architectureDock_->hide();
    if (themeDock_) themeDock_->hide();
    if (graphToolBar_) graphToolBar_->setEnabled(false);
    if (layoutToolBar_) layoutToolBar_->setEnabled(false);
    currentDbPath_.clear();
    if (actionExportJson_) actionExportJson_->setEnabled(false);
    if (actionExportSqlite_) actionExportSqlite_->setEnabled(false);
    setSaveStatus(DbSaveState::NoDb);
}

void MainWindow::showCanvas()
{
    if (canvasContainer_)
    {
        centralStack_->setCurrentWidget(canvasContainer_);
    }
    else
    {
        centralStack_->setCurrentWidget(graphView_);
    }
    if (architectureDock_) architectureDock_->show();
    if (themeDock_) themeDock_->show();
    if (architectureDock_) architectureDock_->raise();
    if (graphToolBar_) graphToolBar_->setEnabled(true);
    if (layoutToolBar_) layoutToolBar_->setEnabled(true);
}

void MainWindow::setupMenu()
{
    // ------------------------------------------------------------
    // 1. File Menu
    // ------------------------------------------------------------
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("New Project...", this, &MainWindow::createNewDatabase);
    fileMenu->addAction("Open DB...", this, &MainWindow::openDatabase);
    actionSaveLayout_ = fileMenu->addAction("Save Layout", this, &MainWindow::saveLayout);
    actionSaveLayout_->setShortcut(QKeySequence::Save);
    fileMenu->addSeparator();

    actionConvert_ = fileMenu->addAction("Convert Database (JSON ⇄ SQLite)...", this, &MainWindow::openConvertDialog);

    auto* exportMenu = fileMenu->addMenu("&Export");
    actionExportJson_ = exportMenu->addAction("To JSON...", this, &MainWindow::exportCurrentAsJson);
    actionExportSqlite_ = exportMenu->addAction("To SQLite...", this, &MainWindow::exportCurrentAsSqlite);
    actionExportJson_->setEnabled(false);
    actionExportSqlite_->setEnabled(false);
    exportMenu->addSeparator();

    actionExportCurrent_ = new QAction("Current View (SVG)...", this);
    connect(actionExportCurrent_, &QAction::triggered, this, [this]() {
        graphView_->exportToSvg(GraphView::ExportMode::CurrentView);
    });
    exportMenu->addAction(actionExportCurrent_);

    actionExportWhole_ = new QAction("Whole Diagram (SVG)...", this);
    connect(actionExportWhole_, &QAction::triggered, this, [this]() {
        graphView_->exportToSvg(GraphView::ExportMode::WholeScene);
    });
    exportMenu->addAction(actionExportWhole_);

    actionExportHtml_ = new QAction("Interactive HTML Report...", this);
    connect(actionExportHtml_, &QAction::triggered, this, [this]() {
        exportToInteractiveHtml();
    });
    exportMenu->addAction(actionExportHtml_);

    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    // ------------------------------------------------------------
    // 2. Edit Menu
    // ------------------------------------------------------------
    auto* editMenu = menuBar()->addMenu("&Edit");
    actionCut_ = editMenu->addAction(QIcon(":/icons/cut.svg"), "Cut", this, &MainWindow::cutSelectedNodes);
    actionCut_->setShortcut(QKeySequence::Cut);

    actionCopy_ = editMenu->addAction(QIcon(":/icons/copy.svg"), "Copy", this, &MainWindow::copySelectedNodes);
    actionCopy_->setShortcut(QKeySequence::Copy);

    actionPaste_ = editMenu->addAction(QIcon(":/icons/paste.svg"), "Paste", this, [this]() { pasteNodes(); });
    actionPaste_->setShortcut(QKeySequence::Paste);

    actionDuplicate_ = editMenu->addAction(QIcon(":/icons/copy.svg"), "Duplicate Node", this, &MainWindow::copySelectedNode);

    editMenu->addSeparator();
    actionAddNode_ = editMenu->addAction("Add Node", this, &MainWindow::createNewNode);
    actionAddLayer_ = editMenu->addAction("Add Layer", this, &MainWindow::createNewLayer);
    actionConnect_ = new QAction(QIcon(":/icons/connect.svg"), "Connect Selected Nodes", this);
    connect(actionConnect_, &QAction::triggered, this, &MainWindow::connectSelectedNodes);
    editMenu->addAction(actionConnect_);

    editMenu->addSeparator();
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

    auto* distMenu = editMenu->addMenu("Distribute");
    actionDistH_ = new QAction(QIcon(":/icons/dist-h.svg"), "Horizontally", this);
    connect(actionDistH_, &QAction::triggered, this, &MainWindow::distributeHorizontal);
    distMenu->addAction(actionDistH_);

    actionDistV_ = new QAction(QIcon(":/icons/dist-v.svg"), "Vertically", this);
    connect(actionDistV_, &QAction::triggered, this, &MainWindow::distributeVertical);
    distMenu->addAction(actionDistV_);

    // ------------------------------------------------------------
    // 3. View Menu (New Professional View Management)
    // ------------------------------------------------------------
    viewMenu_ = menuBar()->addMenu("&View");

    actionView_ = new QAction("View", this);
    actionView_->setCheckable(true);
    actionView_->setChecked(true);
    connect(actionView_, &QAction::triggered, this, [this]() { setGraphMode(GraphView::Mode::View); });

    actionEdit_ = new QAction("Edit", this);
    actionEdit_->setCheckable(true);
    connect(actionEdit_, &QAction::triggered, this, [this]() { setGraphMode(GraphView::Mode::Edit); });

    auto* modeGroup = new QActionGroup(this);
    modeGroup->addAction(actionView_);
    modeGroup->addAction(actionEdit_);
    addAction(actionView_);
    addAction(actionEdit_);

    auto* modeSubMenu = viewMenu_->addMenu("&Mode");
    modeSubMenu->addAction(actionView_);
    modeSubMenu->addAction(actionEdit_);
    viewMenu_->addSeparator();

    auto* actZoomIn = viewMenu_->addAction("Zoom In", this, [this]() {
        if (graphView_) graphView_->scale(1.2, 1.2);
    });
    actZoomIn->setShortcut(QKeySequence::ZoomIn);

    auto* actZoomOut = viewMenu_->addAction("Zoom Out", this, [this]() {
        if (graphView_) graphView_->scale(1.0 / 1.2, 1.0 / 1.2);
    });
    actZoomOut->setShortcut(QKeySequence::ZoomOut);

    auto* actZoomFit = viewMenu_->addAction("Fit Scene to View", this, [this]() {
        if (graphView_ && scene_) graphView_->fitInView(scene_->itemsBoundingRect(), Qt::KeepAspectRatio);
    });
    actZoomFit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    viewMenu_->addSeparator();
    auto* scopeSubMenu = viewMenu_->addMenu("&Container Scope");
    actionFocusContainer_ = scopeSubMenu->addAction(QIcon(":/icons/layers.svg"), "Drill Down / Focus into Container", this, &MainWindow::focusSelectedContainer);
    actionFocusContainer_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Down));

    actionExitContainerScope_ = scopeSubMenu->addAction("Step Up / Exit Container Scope", this, &MainWindow::stepUpContainerScope);
    actionExitContainerScope_->setShortcut(QKeySequence(Qt::Key_Escape));

    addAction(actionFocusContainer_);
    addAction(actionExitContainerScope_);

    viewMenu_->addSeparator();
    auto* panelsMenu = viewMenu_->addMenu("&Panels");
    if (architectureDock_) {
        QAction* toggleArchAction = architectureDock_->toggleViewAction();
        toggleArchAction->setText("Architecture &Navigator");
        panelsMenu->addAction(toggleArchAction);
    }
    if (themeDock_) {
        QAction* toggleThemeAction = themeDock_->toggleViewAction();
        toggleThemeAction->setText("&Theme Editor");
        panelsMenu->addAction(toggleThemeAction);
    }

    toolbarsMenu_ = viewMenu_->addMenu("&Toolbars");

    // ------------------------------------------------------------
    // 4. Theme Menu (Dedicated strictly to Graph Canvas Themes)
    // ------------------------------------------------------------
    themeMenu_ = menuBar()->addMenu("&Theme");

    actionOpenTheme_ = new QAction("&Open Graph Theme File...", this);
    connect(actionOpenTheme_, &QAction::triggered, this, &MainWindow::loadThemeFromFile);

    actionSaveTheme_ = new QAction("&Save Graph Theme", this);
    connect(actionSaveTheme_, &QAction::triggered, this, &MainWindow::saveTheme);

    actionSaveThemeAs_ = new QAction("Save Graph Theme &As...", this);
    connect(actionSaveThemeAs_, &QAction::triggered, this, &MainWindow::saveThemeAs);

    actionResetTheme_ = new QAction("&Reset Graph Theme to Default", this);
    connect(actionResetTheme_, &QAction::triggered, this, [this]() {
        GraphThemeManager::instance()->resetDefaults();
        populateThemeMenu();
    });

    themeMenu_->addAction(actionOpenTheme_);
    themeMenu_->addAction(actionSaveTheme_);
    themeMenu_->addAction(actionSaveThemeAs_);
    themeMenu_->addSeparator();
    themeMenu_->addAction(actionResetTheme_);
    themeMenu_->addSeparator();

    themeMenu_->addAction("Launch Standalone &Theme Editor...", this, []() {
        auto* editor = new ThemeEditorWindow(nullptr);
        editor->setAttribute(Qt::WA_DeleteOnClose);
        editor->show();
    });

    populateThemeMenu();

    // ------------------------------------------------------------
    // 5. Governance Menu
    // ------------------------------------------------------------
    governanceMenu_ = menuBar()->addMenu("&Governance");
    actionReview_ = governanceMenu_->addAction("&Review Selected...", this, &MainWindow::reviewSelected);
    actionApprove_ = governanceMenu_->addAction("&Approve Selected...", this, &MainWindow::approveSelected);
    actionMarkChanged_ = governanceMenu_->addAction("Mark Selected as &Changed", this, &MainWindow::markSelectedChanged);
    actionMarkInvalid_ = governanceMenu_->addAction("Mark Selected as &Invalid", this, &MainWindow::markSelectedInvalid);
    governanceMenu_->addSeparator();
    actionVerifyIntegrity_ = governanceMenu_->addAction("&Verify Architecture Integrity...", this, &MainWindow::verifyModelIntegrity);
    governanceMenu_->addSeparator();
    actionShowBadges_ = governanceMenu_->addAction("Show Governance &Badges on Canvas", this, [this](bool checked) {
        GraphNodeItem::setShowGovernanceBadges(checked);
        if (scene_) scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
        if (graphView_) graphView_->viewport()->update();
    });
    actionShowBadges_->setCheckable(true);
    actionShowBadges_->setChecked(GraphNodeItem::showGovernanceBadges());

    // ------------------------------------------------------------
    // 6. Routing Menu
    // ------------------------------------------------------------
    routingMenu_ = menuBar()->addMenu("&Routing");
    setupRoutingMenuAndControls();

    // ------------------------------------------------------------
    // 7. Settings Menu
    // ------------------------------------------------------------
    auto* settingsMenu = menuBar()->addMenu("&Settings");
    actionPreferences_ = settingsMenu->addAction("&Preferences...", this, &MainWindow::openPreferencesDialog);
    actionPreferences_->setShortcut(QKeySequence::Preferences);
    settingsMenu->addSeparator();

    actionConfigureShortcuts_ = settingsMenu->addAction("Configure &Shortcuts...", this, &MainWindow::openShortcutConfigDialog);

    // Retain internal action pointers for test / shortcut compatibility
    if (!actionAutoSave_) {
        actionAutoSave_ = new QAction("Enable Auto-Save", this);
        actionAutoSave_->setCheckable(true);
        actionAutoSave_->setChecked(ApplicationConfig::instance().isAutoSaveEnabled());
        connect(actionAutoSave_, &QAction::toggled, this, &MainWindow::onAutoSaveToggled);
    }
    if (!actionApplicationFont_) {
        actionApplicationFont_ = new QAction("Application Font...", this);
        connect(actionApplicationFont_, &QAction::triggered, this, &MainWindow::openPreferencesDialog);
    }

    // ------------------------------------------------------------
    // 8. Help Menu
    // ------------------------------------------------------------
    helpMenu_ = menuBar()->addMenu("&Help");
    helpMenu_->addAction("Keyboard &Shortcuts Reference...", this, &MainWindow::showShortcutsReferenceDialog);
    helpMenu_->addSeparator();
    helpMenu_->addAction("&About OpenArch...", this, &MainWindow::showAboutDialog);
}

void MainWindow::setupApplicationThemeMenu(QMenu* parentMenu)
{
    if (!parentMenu)
        return;

    parentMenu->clear();

    auto* group = new QActionGroup(parentMenu);
    const QString currentId = ApplicationThemeManager::currentThemeId();

    const auto themes = ApplicationThemeManager::availableThemes();
    bool hasCustom = false;

    for (const auto& th : themes) {
        if (!th.isBuiltIn && !hasCustom) {
            parentMenu->addSeparator();
            hasCustom = true;
        }

        QAction* act = parentMenu->addAction(th.name);
        act->setCheckable(true);
        if (th.id.compare(currentId, Qt::CaseInsensitive) == 0) {
            act->setChecked(true);
        }

        const QString id = th.id;
        connect(act, &QAction::triggered, this, [this, id]() {
            ApplicationThemeManager::setThemeById(id);
            if (appThemeMenu_) setupApplicationThemeMenu(appThemeMenu_);
        });
        group->addAction(act);
    }

    parentMenu->addSeparator();
    parentMenu->addAction("&Load Custom UI Theme File...", this, &MainWindow::loadCustomApplicationTheme);
    parentMenu->addAction("&Open Custom Themes Folder...", this, &MainWindow::openCustomApplicationThemesFolder);
}

void MainWindow::loadCustomApplicationTheme()
{
    const QString customDir = ApplicationThemeManager::customThemesDirectory();
    QString file = QFileDialog::getOpenFileName(
        this,
        "Load Custom Application UI Theme",
        customDir,
        "UI Theme Files (*.json *.qss);;JSON Theme (*.json);;QSS Stylesheet (*.qss);;All Files (*.*)"
    );

    if (file.isEmpty())
        return;

    QString error;
    if (ApplicationThemeManager::loadCustomThemeFile(file, &error)) {
        statusBar()->showMessage(QString("Loaded application UI theme: %1").arg(QFileInfo(file).baseName()), 3000);
        if (appThemeMenu_) setupApplicationThemeMenu(appThemeMenu_);
    } else {
        QMessageBox::warning(this, "Theme Error", error.isEmpty() ? "Failed to load custom UI theme." : error);
    }
}

void MainWindow::openCustomApplicationThemesFolder()
{
    const QString dir = ApplicationThemeManager::customThemesDirectory();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MainWindow::populateThemeMenu()
{
    if (!themeMenu_ || !actionOpenTheme_)
        return;

    for (QAction* act : presetActions_) {
        themeMenu_->removeAction(act);
        delete act;
    }
    presetActions_.clear();

    if (presetSeparator_) {
        themeMenu_->removeAction(presetSeparator_);
        delete presetSeparator_;
        presetSeparator_ = nullptr;
    }

    delete presetActionGroup_;
    presetActionGroup_ = new QActionGroup(this);

    const auto presets = GraphThemeManager::availablePresets();
    const QString activePath = GraphThemeManager::instance()->currentThemePath();
    const QString activeThemeName = GraphThemeManager::instance()->theme().name;

    for (const auto& preset : presets) {
        QAction* act = new QAction(preset.name, this);
        act->setCheckable(true);
        const QString path = preset.path;
        connect(act, &QAction::triggered, this, [this, path]() {
            switchThemePreset(path);
        });

        if (activePath == path || (!preset.id.isEmpty() && activeThemeName.compare(preset.id, Qt::CaseInsensitive) == 0)) {
            act->setChecked(true);
        }

        presetActionGroup_->addAction(act);
        themeMenu_->insertAction(actionOpenTheme_, act);
        presetActions_.append(act);
    }

    if (!presetActions_.isEmpty()) {
        presetSeparator_ = themeMenu_->insertSeparator(actionOpenTheme_);
    }
}

void MainWindow::setupRoutingMenuAndControls()
{
    if (!routingMenu_) return;

    routingActionGroup_ = new QActionGroup(this);

    const auto& algos = GraphEdgeItem::availableRoutingAlgorithms();
    for (auto algo : algos)
    {
        QString name = GraphEdgeItem::routingAlgorithmName(algo);
        auto* act = routingMenu_->addAction(name);
        act->setCheckable(true);
        act->setData(static_cast<int>(algo));
        routingActionGroup_->addAction(act);

        connect(act, &QAction::triggered, this, [this, algo]() {
            setGlobalRoutingAlgorithm(algo);
        });
    }

    routingMenu_->addSeparator();

    actionLineJumps_ = routingMenu_->addAction("Bridge Hops on Crossings");
    actionLineJumps_->setCheckable(true);
    actionLineJumps_->setToolTip("Draw bridge hop arcs where edges cross so they never collide or overlap");

    QSettings settings;
    bool lineJumps = settings.value("Routing/LineJumps", true).toBool();
    GraphEdgeItem::setLineJumpsEnabled(lineJumps);
    actionLineJumps_->setChecked(lineJumps);

    connect(actionLineJumps_, &QAction::toggled, this, [this](bool checked) {
        GraphEdgeItem::setLineJumpsEnabled(checked);
        QSettings s;
        s.setValue("Routing/LineJumps", checked);
        if (scene_)
        {
            GraphEdgeItem::updateSceneEdges(scene_);
            scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
        }
        if (graphView_)
        {
            graphView_->viewport()->update();
        }
        statusBar()->showMessage(checked ? "Bridge hops enabled" : "Bridge hops disabled", 2000);
    });

    int saved = settings.value("Routing/Algorithm", static_cast<int>(EdgeRoutingAlgorithm::SmartOrthogonal)).toInt();
    auto currentAlgo = static_cast<EdgeRoutingAlgorithm>(saved);
    GraphEdgeItem::setGlobalRoutingAlgorithm(currentAlgo);

    for (auto* act : routingActionGroup_->actions()) {
        if (act->data().toInt() == saved) {
            act->setChecked(true);
            break;
        }
    }
}

void MainWindow::setGlobalRoutingAlgorithm(EdgeRoutingAlgorithm algo)
{
    GraphEdgeItem::setGlobalRoutingAlgorithm(algo);

    QSettings settings;
    settings.setValue("Routing/Algorithm", static_cast<int>(algo));

    if (routingActionGroup_) {
        for (auto* act : routingActionGroup_->actions()) {
            if (act->data().toInt() == static_cast<int>(algo)) {
                act->setChecked(true);
                break;
            }
        }
    }

    if (scene_)
    {
        GraphEdgeItem::updateSceneEdges(scene_);
        scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
    }
    if (graphView_)
    {
        graphView_->viewport()->update();
    }

    statusBar()->showMessage(QString("Routing algorithm: %1").arg(GraphEdgeItem::routingAlgorithmName(algo)), 2500);
}

void MainWindow::setupToolbar()
{
    // Mode toggle is displayed as a sleek floating segmented pill at the bottom-right corner of GraphView.
    // Window actions are preserved for keyboard shortcuts and ShortcutManager registration.
    if (!actionView_) {
        actionView_ = new QAction("View", this);
        actionView_->setCheckable(true);
        actionView_->setChecked(true);
        connect(actionView_, &QAction::triggered, this, [this]() { setGraphMode(GraphView::Mode::View); });
    }
    if (!actionEdit_) {
        actionEdit_ = new QAction("Edit", this);
        actionEdit_->setCheckable(true);
        connect(actionEdit_, &QAction::triggered, this, [this]() { setGraphMode(GraphView::Mode::Edit); });
    }

    layoutToolBar_ = addToolBar("Layout");
    layoutToolBar_->setIconSize(QSize(20, 20));
    if (toolbarsMenu_) {
        toolbarsMenu_->addAction(layoutToolBar_->toggleViewAction());
    }

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
    connect(welcomeWidget_, &WelcomeWidget::convertClicked, this, &MainWindow::openConvertDialog);
    connect(welcomeWidget_, &WelcomeWidget::recentFileSelected, this, [this](const QString& path) {
        setDb(path.toStdString());
    });

    connect(navigator_, &QTreeView::doubleClicked, this, &MainWindow::onTreeItemDoubleClicked);
    connect(navigator_, &QTreeView::clicked, this, &MainWindow::onTreeItemClicked);
    connect(navigator_, &QTreeView::activated, this, &MainWindow::showSelectedInGraph);
    navigator_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(navigator_, &QTreeView::customContextMenuRequested, this, &MainWindow::onNavigatorContextMenu);

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
    connect(graphView_, &GraphView::modeChanged, this, &MainWindow::setGraphMode);
    connect(scene_, &QGraphicsScene::selectionChanged, this, &MainWindow::onSelectionChanged);

    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::deleteSelected);

    auto* backspaceShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    connect(backspaceShortcut, &QShortcut::activated, this, &MainWindow::deleteSelected);

    connect(ShortcutManager::instance(), &ShortcutManager::shortcutsChanged, this, &MainWindow::updateShortcutLabels);

    autoSaveTimer_ = new QTimer(this);
    autoSaveTimer_->setSingleShot(true);
    autoSaveTimer_->setInterval(ApplicationConfig::instance().autoSaveDelayMs());
    connect(autoSaveTimer_, &QTimer::timeout, this, &MainWindow::saveLayout);

    connect(&ApplicationConfig::instance(), &ApplicationConfig::autoSaveChanged, this, &MainWindow::onAutoSaveConfigChanged);
    connect(&ApplicationConfig::instance(), &ApplicationConfig::autoSaveDelayChanged, this, &MainWindow::onAutoSaveDelayChanged);
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

    // Container Scope
    sm->registerAction("view.focus_container", "Focus into Container", "View", QKeySequence(Qt::CTRL | Qt::Key_Down), actionFocusContainer_, "Drill down and isolate the selected container");
    sm->registerAction("view.exit_container_scope", "Exit Container Scope", "View", QKeySequence(Qt::Key_Escape), actionExitContainerScope_, "Step up to parent or exit container scope");

    // Edit
    sm->registerAction("edit.duplicate", "Duplicate Node", "Edit", QKeySequence(Qt::CTRL | Qt::Key_D), actionDuplicate_, "Duplicate the currently selected node");
    sm->registerAction("edit.connect", "Connect Selected Nodes", "Edit", QKeySequence(Qt::CTRL | Qt::Key_K), actionConnect_, "Connect the selected nodes with an edge");
    sm->registerAction("edit.add_node", "Add Node", "Edit", QKeySequence(), actionAddNode_, "Create a new node in the architecture");
    sm->registerAction("edit.add_layer", "Add Layer", "Edit", QKeySequence(), actionAddLayer_, "Create a new layer in the architecture");

    // Layout
    sm->registerAction("layout.dist_h", "Distribute Horizontally", "Layout", QKeySequence(Qt::ALT | Qt::Key_H), actionDistH_, "Distribute selected nodes evenly along horizontal axis");
    sm->registerAction("layout.dist_v", "Distribute Vertically", "Layout", QKeySequence(Qt::ALT | Qt::Key_V), actionDistV_, "Distribute selected nodes evenly along vertical axis");
    sm->registerAction("layout.save_layout", "Save Layout", "Layout", QKeySequence(QKeySequence::Save), actionSaveLayout_, "Save node positions to the database");

    // Settings
    if (actionPreferences_)
    {
        sm->registerAction("settings.preferences", "Preferences", "Settings", QKeySequence::Preferences, actionPreferences_, "Open application preferences dialog");
    }

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

    // File
    sm->registerAction("file.convert", "Convert Database", "File", QKeySequence(), actionConvert_, "Convert between JSON and SQLite databases");

    // Theme
    sm->registerAction("theme.save", "Save Theme", "Theme", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S), actionSaveTheme_, "Save current theme settings to file");
    sm->registerAction("theme.save_as", "Save Theme As", "Theme", QKeySequence(), actionSaveThemeAs_, "Save current theme settings to a new file");

    // Governance
    sm->registerAction("gov.review", "Review Selected", "Governance", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R), actionReview_, "Mark selected architecture elements as formally reviewed");
    sm->registerAction("gov.approve", "Approve Selected", "Governance", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A), actionApprove_, "Commit formal governance approval on selected elements");
    sm->registerAction("gov.verify", "Verify Architecture Integrity", "Governance", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I), actionVerifyIntegrity_, "Run cryptographic checksum integrity audit across all architecture elements");

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

    if (graphView_) {
        graphView_->updateOverlayShortcutHints();
    }
}

void MainWindow::openShortcutConfigDialog()
{
    ShortcutConfigDialog dialog(this);
    dialog.exec();
}

void MainWindow::scheduleAutoSave()
{
    if (!model_ || isRendering_)
        return;

    if (ApplicationConfig::instance().isAutoSaveEnabled())
    {
        if (autoSaveTimer_)
        {
            autoSaveTimer_->start();
        }
    }
    else
    {
        hasUnsavedLayoutChanges_ = true;
        setSaveStatus(DbSaveState::UnsavedChanges, "Unsaved layout changes (press Ctrl+S to save)");
    }
}

void MainWindow::setDb(const std::string& db_path)
{
    if (db_path.empty())
        return;

    if (!maybePromptSaveUnsavedChanges())
        return;

    if (autoSaveTimer_)
        autoSaveTimer_->stop();

    hasUnsavedLayoutChanges_ = false;

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

    currentDbPath_ = db_path;
    bool isSqlite = qPath.endsWith(".db", Qt::CaseInsensitive) || 
                    qPath.endsWith(".sqlite", Qt::CaseInsensitive) || 
                    qPath.endsWith(".sqlite3", Qt::CaseInsensitive);
    if (actionExportJson_) actionExportJson_->setEnabled(isSqlite);
    if (actionExportSqlite_) actionExportSqlite_->setEnabled(!isSqlite);

    model_ = new ArchitectureModel(*db_);

    exitContainerScope();
    populateNavigator();
    renderGraph(model_->extractGraph(std::nullopt));
    showCanvas();
    hasUnsavedLayoutChanges_ = false;
    setSaveStatus(DbSaveState::Saved, QString("Loaded %1").arg(QFileInfo(qPath).fileName()));
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
    if (isRendering_ || !model_ || !navModel_)
        return;

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
    if (isRendering_ || !model_ || !index.isValid()) return;

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
            std::optional<LayerId> activeLayerId;
            QModelIndex curIdx = navigator_->currentIndex();
            if (curIdx.isValid() && static_cast<ItemType>(curIdx.data(NavRole::Type).toInt()) == ItemType::Layer)
            {
                activeLayerId = static_cast<LayerId>(curIdx.data(NavRole::Id).toULongLong());
            }
            populateNavigator();
            if (activeLayerId)
            {
                renderGraph(model_->extractGraph(*activeLayerId));
            }
            else
            {
                renderGraph(model_->extractGraph(std::nullopt));
            }
            setSaveStatus(DbSaveState::Saved, "Node updated in database");
        }
        break;
    }
    case ItemType::Layer: {
        LayerEditorDialog dlg(model_, id, this);
        if (dlg.exec() == QDialog::Accepted)
        {
            populateNavigator();
            renderGraph(model_->extractGraph(static_cast<LayerId>(id)));
            setSaveStatus(DbSaveState::Saved, "Layer updated in database");
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
    if (isRendering_ || !model_ || !index.isValid()) return;

    QModelIndex srcIndex = navProxyModel_ ? navProxyModel_->mapToSource(index) : index;
    auto* item = navModel_->itemFromIndex(srcIndex);
    if (!item) return;

    auto type = static_cast<ItemType>(item->data(NavRole::Type).toInt());

    if (type == ItemType::Layer) {
        auto layerId = static_cast<LayerId>(item->data(NavRole::Id).toULongLong());
        showLayerInGraph(layerId);
    }
    else if (type == ItemType::Node) {
        auto nodeId = static_cast<NodeId>(item->data(NavRole::Id).toULongLong());
        showNodeInGraph(nodeId);
    }
    else {
        renderGraph(model_->extractGraph(std::nullopt));
    }
}

void MainWindow::showSelectedInGraph()
{
    if (!navigator_ || !model_) return;

    QModelIndex index = navigator_->currentIndex();
    if (!index.isValid()) return;

    QModelIndex srcIndex = navProxyModel_ ? navProxyModel_->mapToSource(index) : index;
    auto* item = navModel_->itemFromIndex(srcIndex);
    if (!item) return;

    auto type = static_cast<ItemType>(item->data(NavRole::Type).toInt());
    qulonglong id = item->data(NavRole::Id).toULongLong();

    if (type == ItemType::Node)
    {
        showNodeInGraph(static_cast<NodeId>(id));
    }
    else if (type == ItemType::Layer)
    {
        showLayerInGraph(static_cast<LayerId>(id));
    }
    else
    {
        showCanvas();
        renderGraph(model_->extractGraph(std::nullopt));
        if (scene_ && graphView_)
        {
            QRectF b = scene_->itemsBoundingRect();
            if (!b.isEmpty()) graphView_->fitInView(b.adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
        }
    }
}

void MainWindow::showNodeInGraph(NodeId nodeId)
{
    if (!model_ || !scene_ || !graphView_) return;

    showCanvas();

    // 1. Locate node item in current scene
    GraphNodeItem* targetItem = nullptr;
    for (QGraphicsItem* item : scene_->items())
    {
        if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
        {
            if (nodeItem->nodeId() == nodeId)
            {
                targetItem = nodeItem;
                break;
            }
        }
    }

    // 2. If node is not on canvas (e.g. filtered by a layer or outside current container scope),
    // restore the graph so the node can be shown
    if (!targetItem)
    {
        if (activeContainerScope_.has_value())
        {
            exitContainerScope();
        }
        else
        {
            renderGraph(model_->extractGraph(std::nullopt));
        }
        for (QGraphicsItem* item : scene_->items())
        {
            if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
            {
                if (nodeItem->nodeId() == nodeId)
                {
                    targetItem = nodeItem;
                    break;
                }
            }
        }
    }

    if (!targetItem) return;

    // 3. Unfold any parent containers so the target node is visible
    QGraphicsItem* p = targetItem->parentItem();
    while (p)
    {
        if (auto* parentNode = dynamic_cast<GraphNodeItem*>(p))
        {
            if (parentNode->isFolded())
            {
                parentNode->setFolded(false);
            }
        }
        p = p->parentItem();
    }

    // 4. Select the target node
    scene_->clearSelection();
    targetItem->setSelected(true);
    primaryNode_ = targetItem;

    // 5. Center and focus view on target node
    graphView_->centerOn(targetItem);
    graphView_->ensureVisible(targetItem, 80, 80);

    // 6. Provide status bar feedback
    auto nodeOpt = model_->getNodeById(nodeId);
    QString name = nodeOpt ? QString::fromStdString(nodeOpt->name) : QString("Node #%1").arg(nodeId);
    statusBar()->showMessage(QString("Showing '%1' in graph").arg(name), 2500);
}

void MainWindow::showLayerInGraph(LayerId layerId)
{
    if (!model_ || !scene_ || !graphView_) return;

    showCanvas();

    if (activeContainerScope_.has_value())
    {
        activeContainerScope_ = std::nullopt;
        containerBreadcrumbTrail_.clear();
        if (scopeBar_)
        {
            scopeBar_->clearScope();
        }
    }

    if (layerId > 0)
    {
        renderGraph(model_->extractGraph(layerId));
    }
    else
    {
        renderGraph(model_->extractGraph(std::nullopt));
    }

    QRectF b = scene_->itemsBoundingRect();
    if (!b.isEmpty())
    {
        graphView_->fitInView(b.adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
    }

    auto layerOpt = model_->getLayerById(layerId);
    QString name = layerOpt ? QString::fromStdString(layerOpt->name) : QString("Layer #%1").arg(layerId);
    statusBar()->showMessage(QString("Showing layer '%1' in graph").arg(name), 2500);
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
    if (dlg.exec() == QDialog::Accepted)
    {
        if (activeContainerScope_.has_value())
        {
            auto nodes = model_->nodes();
            if (!nodes.empty())
            {
                NodeData& newNode = nodes.back();
                if (!newNode.parentId.has_value())
                {
                    newNode.parentId = *activeContainerScope_;
                    model_->updateNode(newNode);
                }
            }
        }
        populateNavigator();
        refreshActiveGraph();
    }
}

void MainWindow::createNewLayer()
{
    if (!model_) return;

    LayerEditorDialog dlg(model_, 0, this);
    dlg.exec();

    populateNavigator();
    refreshActiveGraph();
}

void MainWindow::renderGraph(const GraphSnapshot& snap)
{
    if (!model_ || !scene_ || !graphView_)
        return;

    int hVal = graphView_->horizontalScrollBar()->value();
    int vVal = graphView_->verticalScrollBar()->value();

    isRendering_ = true;
    scene_->clearSelection();
    primaryNode_ = nullptr;
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

        bool isProxy = false;
        bool isOut = true;
        if (!n.metadata.empty())
        {
            auto doc = QJsonDocument::fromJson(QString::fromStdString(n.metadata).toUtf8());
            if (doc.isObject())
            {
                isProxy = doc.object()["_is_external_proxy"].toBool(false);
                isOut = (doc.object()["_external_direction"].toString("outgoing") == "outgoing");
            }
        }
        nodeItem->setExternalProxy(isProxy, isOut);

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
                bool isCycle = false;
                QGraphicsItem* anc = pIt->second;
                while (anc)
                {
                    if (anc == nodeItem) { isCycle = true; break; }
                    anc = anc->parentItem();
                }
                if (!isCycle)
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
    for (const auto& n : snap.nodes)
    {
        GraphNodeItem* nodeItem = currentNodes[n.id];
        if (nodeItem->scene() != scene_)
        {
            nodeItem->setParentItem(nullptr);
            scene_->addItem(nodeItem);
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
                if (obj.contains("folded") && obj["folded"].toBool())
                {
                    nodeItem->setFolded(true);
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

    for (const auto& n : snap.nodes)
    {
        GraphNodeItem* nodeItem = currentNodes[n.id];
        if (nodeItem && (nodeItem->isContainer() || nodeItem->canAcceptChildren()))
        {
            nodeItem->refreshGeometry();
        }
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

                bool srcProxy = srcNode->isExternalProxy();
                bool dstProxy = dstNode->isExternalProxy();
                if (srcProxy || dstProxy)
                {
                    edgeItem->setIsExternalLink(true, dstProxy); // outgoing if destination is the external proxy
                }

                scene_->addItem(edgeItem);
                edgeItem->updateEndpoints();
            }
        }
    }

    GraphEdgeItem::updateSceneEdges(scene_);

    scene_->blockSignals(false);
    graphView_->setUpdatesEnabled(true);
    isRendering_ = false;

    graphView_->horizontalScrollBar()->setValue(hVal);
    graphView_->verticalScrollBar()->setValue(vVal);
}

void MainWindow::saveLayout()
{
    if (!model_ || !scene_ || isRendering_) return;

    setSaveStatus(DbSaveState::Saving, "Saving layout to database...");
    QApplication::processEvents();

    for (QGraphicsItem* item : scene_->items())
    {
        auto* nodeItem = dynamic_cast<GraphNodeItem*>(item);
        if (!nodeItem)
            continue;

        NodeId id = nodeItem->nodeId();
        QPointF p = nodeItem->pos();

        if (nodeItem->isExternalProxy())
        {
            auto nodeOpt = model_->getNodeById(id);
            if (nodeOpt)
            {
                QJsonObject obj;
                if (!nodeOpt->metadata.empty())
                {
                    QJsonDocument existingDoc = QJsonDocument::fromJson(QString::fromStdString(nodeOpt->metadata).toUtf8());
                    if (existingDoc.isObject()) obj = existingDoc.object();
                }
                obj["_scoped_proxy_x"] = p.x();
                obj["_scoped_proxy_y"] = p.y();
                model_->setNodeMetadata(id, QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString());
            }
            continue;
        }

        QJsonObject obj;
        auto nodeOpt = model_->getNodeById(id);
        if (nodeOpt && !nodeOpt->metadata.empty())
        {
            QJsonDocument existingDoc = QJsonDocument::fromJson(QString::fromStdString(nodeOpt->metadata).toUtf8());
            if (existingDoc.isObject())
            {
                obj = existingDoc.object();
            }
        }
        obj["x"] = p.x();
        obj["y"] = p.y();
        if (nodeItem->isFolded())
        {
            obj["folded"] = true;
        }
        else
        {
            obj.remove("folded");
        }

        QJsonDocument doc(obj);
        model_->setNodeMetadata(id, doc.toJson(QJsonDocument::Compact).toStdString());
    }

    hasUnsavedLayoutChanges_ = false;
    setSaveStatus(DbSaveState::Saved, "Layout saved");
}

void MainWindow::handleAddNodeAtPosition(QPointF pos)
{
    if (!model_) return;

    NodeEditorDialog dlg(model_, 0, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    setSaveStatus(DbSaveState::Saving, "Saving new node to database...");

    auto nodes = model_->nodes();
    if (nodes.empty())
        return;

    NodeData& newNode = nodes.back();
    NodeId newId = newNode.id;

    QJsonObject obj;
    if (!newNode.metadata.empty())
    {
        QJsonDocument existingDoc = QJsonDocument::fromJson(QString::fromStdString(newNode.metadata).toUtf8());
        if (existingDoc.isObject())
        {
            obj = existingDoc.object();
        }
    }
    if (activeContainerScope_.has_value())
    {
        newNode.parentId = *activeContainerScope_;
        // Inherit parent container's layers
        if (!navigator_ || !navigator_->currentIndex().isValid() ||
            static_cast<ItemType>(navigator_->currentIndex().data(NavRole::Type).toInt()) != ItemType::Layer)
        {
            for (const auto& layer : model_->layers())
            {
                for (const auto& nl : model_->nodesInLayer(layer.id))
                {
                    if (nl.nodeId == *activeContainerScope_)
                    {
                        model_->addNodeToLayer(newId, layer.id);
                        break;
                    }
                }
            }
        }
        obj["x"] = pos.x();
        obj["y"] = pos.y();
    }
    else
    {
        obj["x"] = pos.x();
        obj["y"] = pos.y();
    }

    QJsonDocument doc(obj);
    model_->setNodeMetadata(newId, doc.toJson(QJsonDocument::Compact).toStdString());

    QModelIndex index = navigator_->currentIndex();
    if (index.isValid()) {
        auto type = static_cast<ItemType>(index.data(NavRole::Type).toInt());
        if (type == ItemType::Layer) {
            auto layerId = static_cast<LayerId>(index.data(NavRole::Id).toULongLong());
            model_->addNodeToLayer(newId, layerId);
            populateNavigator();
            refreshActiveGraph();
            setSaveStatus(DbSaveState::Saved, QString("Added node '%1'").arg(QString::fromStdString(newNode.name)));
            return;
        }
    }

    populateNavigator();
    refreshActiveGraph();
    setSaveStatus(DbSaveState::Saved, QString("Added node '%1'").arg(QString::fromStdString(newNode.name)));
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

    QDialog connDlg(this);
    connDlg.setWindowTitle("New Connection");
    connDlg.resize(380, 160);
    auto* formLayout = new QFormLayout(&connDlg);
    auto* nameInput = new QLineEdit(&connDlg);
    nameInput->setPlaceholderText(QString("%1 -> %2").arg(srcName, dstName));
    auto* typeInput = new QLineEdit("default", &connDlg);
    typeInput->setPlaceholderText("Connection type / protocol (e.g. lan, pwr, https)");
    formLayout->addRow("Edge Name:", nameInput);
    formLayout->addRow("Type / Protocol:", typeInput);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &connDlg);
    connect(buttonBox, &QDialogButtonBox::accepted, &connDlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &connDlg, &QDialog::reject);
    formLayout->addWidget(buttonBox);

    if (connDlg.exec() != QDialog::Accepted)
        return;

    QString edgeName = nameInput->text().trimmed();
    QString edgeType = typeInput->text().trimmed();
    if (edgeType.isEmpty())
        edgeType = "default";

    EdgeData e;
    e.name = edgeName.toStdString();
    e.srcNode = srcId;
    e.dstNode = dstId;
    e.srcLayer = chosenLayerId;
    e.dstLayer = chosenLayerId;
    e.edgeType = edgeType.toStdString();

    EdgeId newId = 0;
    setSaveStatus(DbSaveState::Saving, "Saving connection to database...");
    auto r = model_->addEdge(e, newId);
    if (!r.ok)
    {
        setSaveStatus(DbSaveState::Error, QString::fromStdString(r.message));
        QMessageBox::critical(this, "Error", QString::fromStdString(r.message));
        return;
    }

    populateNavigator();
    refreshActiveGraph();

    setSaveStatus(DbSaveState::Saved, QString("Connected %1 -> %2").arg(srcName, dstName));
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

    setSaveStatus(DbSaveState::Saving, "Deleting selected elements from database...");

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

    if (activeContainerScope_.has_value())
    {
        if (allNodeIdsToDelete.count(*activeContainerScope_) > 0)
        {
            stepUpContainerScope();
        }
        else
        {
            refreshActiveGraph();
        }
    }
    else
    {
        refreshActiveGraph();
    }

    onSelectionChanged();
    setSaveStatus(DbSaveState::Saved, "Deleted selected items from database");
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
            obj["x"] = obj.value("x").toDouble(0.0) + offsetX;
            obj["y"] = obj.value("y").toDouble(0.0) + offsetY;
            copyNode.metadata = QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
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

    setSaveStatus(DbSaveState::Saving, "Duplicating node in database...");
    NodeId copiedId = cloneNodeRecursive(srcId, parentId, 40.0, 40.0);
    if (copiedId == 0)
    {
        setSaveStatus(DbSaveState::Error, "Failed to duplicate selected node");
        QMessageBox::critical(this, "Error", "Failed to duplicate selected node.");
        return;
    }

    populateNavigator();
    refreshActiveGraph();

    setSaveStatus(DbSaveState::Saved, QString("Node duplicated (New ID: %1)").arg(copiedId));
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
                if (node->canAcceptChildren() && excludeIds.find(node->nodeId()) == excludeIds.end())
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
        refreshActiveGraph();

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

    setSaveStatus(DbSaveState::Saving, "Saving pasted nodes to database...");

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
                copyData.parentId = activeContainerScope_;
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
    refreshActiveGraph();

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

    setSaveStatus(DbSaveState::Saved, QString("Pasted %1 node(s)").arg(newlyCreatedIds.size()));
}

void MainWindow::setGraphMode(GraphView::Mode mode)
{
    if (!graphView_) return;

    if (actionView_ && actionView_->isChecked() != (mode == GraphView::Mode::View)) {
        QSignalBlocker blocker(actionView_);
        actionView_->setChecked(mode == GraphView::Mode::View);
    }
    if (actionEdit_ && actionEdit_->isChecked() != (mode == GraphView::Mode::Edit)) {
        QSignalBlocker blocker(actionEdit_);
        actionEdit_->setChecked(mode == GraphView::Mode::Edit);
    }

    if (graphView_->mode() != mode) {
        graphView_->setMode(mode);
    }

    QString text;
    switch (mode) {
    case GraphView::Mode::View:    text = "Mode: View"; break;
    case GraphView::Mode::Edit:    text = "Mode: Layout"; break;
    case GraphView::Mode::Add:     text = "Mode: Add"; break;
    case GraphView::Mode::Arch:    text = "Mode: Arch"; break;
    case GraphView::Mode::Connect: text = "Mode: Connect"; break;
    }

    bool movable = (mode == GraphView::Mode::Edit);

    if (scene_)
    {
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
    } else {
        statusBar()->showMessage(QString("Loaded theme: %1").arg(QFileInfo(file).fileName()), 3000);
        populateThemeMenu();
    }
}

void MainWindow::switchThemePreset(const QString& path)
{
    if (!GraphThemeManager::instance()->load(path)) {
        QMessageBox::critical(this, "Error", "Failed to load theme configuration: " + path);
    } else {
        statusBar()->showMessage(QString("Theme: %1").arg(QFileInfo(path).baseName()), 3000);
        populateThemeMenu();
    }
}

void MainWindow::saveTheme()
{
    QString curPath = GraphThemeManager::instance()->currentThemePath();
    if (curPath.isEmpty() || curPath.startsWith(":/")) {
        saveThemeAs();
        return;
    }

    if (GraphThemeManager::instance()->save(curPath)) {
        statusBar()->showMessage(QString("Theme saved to %1").arg(curPath), 3000);
    } else {
        QMessageBox::critical(this, "Error", "Failed to save theme configuration to:\n" + curPath);
    }
}

void MainWindow::saveThemeAs()
{
    QString defaultDir;
    QString curPath = GraphThemeManager::instance()->currentThemePath();
    if (!curPath.isEmpty() && !curPath.startsWith(":/")) {
        defaultDir = curPath;
    } else {
        defaultDir = GraphThemeManager::instance()->theme().name + ".json";
    }

    QString file = QFileDialog::getSaveFileName(
        this, "Save Theme As", defaultDir, "Theme JSON (*.json);;All Files (*.*)");

    if (file.isEmpty())
        return;

    if (!file.endsWith(".json", Qt::CaseInsensitive)) {
        file += ".json";
    }

    GraphThemeManager::instance()->mutableTheme().name = QFileInfo(file).baseName();

    if (GraphThemeManager::instance()->save(file)) {
        statusBar()->showMessage(QString("Theme saved to %1").arg(file), 3000);
        populateThemeMenu();
    } else {
        QMessageBox::critical(this, "Error", "Failed to save theme configuration to:\n" + file);
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

void MainWindow::openConvertDialog()
{
    if (db_ && !currentDbPath_.empty()) {
        saveLayout();
    }

    ConvertDialog dlg(this, QString::fromStdString(currentDbPath_));
    connect(&dlg, &ConvertDialog::requestOpenDatabase, this, [this](const QString& path) {
        setDb(path.toStdString());
    });
    dlg.exec();
}

void MainWindow::exportCurrentAsJson()
{
    if (!db_ || currentDbPath_.empty()) {
        QMessageBox::warning(this, "Export Error", "No database is currently open.");
        return;
    }

    saveLayout();

    QFileInfo fi(QString::fromStdString(currentDbPath_));
    QString defaultTarget = fi.path() + "/" + fi.completeBaseName() + ".json";

    QString targetFile = QFileDialog::getSaveFileName(
        this,
        "Export Current Database to JSON",
        defaultTarget,
        "Architecture JSON (*.json);;All Files (*.*)");

    if (targetFile.isEmpty())
        return;

    Result r = DbConverter::convert(currentDbPath_, targetFile.toStdString(), true);
    if (!r.ok) {
        QMessageBox::critical(this, "Export Failed", QString::fromStdString(r.message));
    } else {
        auto reply = QMessageBox::information(
            this,
            "Export Successful",
            QString("Successfully exported to:\n%1\n\nWould you like to open this converted database now?").arg(targetFile),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            setDb(targetFile.toStdString());
        }
    }
}

void MainWindow::exportCurrentAsSqlite()
{
    if (!db_ || currentDbPath_.empty()) {
        QMessageBox::warning(this, "Export Error", "No database is currently open.");
        return;
    }

    saveLayout();

    QFileInfo fi(QString::fromStdString(currentDbPath_));
    QString defaultTarget = fi.path() + "/" + fi.completeBaseName() + ".db";

    QString targetFile = QFileDialog::getSaveFileName(
        this,
        "Export Current Database to SQLite",
        defaultTarget,
        "SQLite Databases (*.db *.sqlite *.sqlite3);;All Files (*.*)");

    if (targetFile.isEmpty())
        return;

    Result r = DbConverter::convert(currentDbPath_, targetFile.toStdString(), true);
    if (!r.ok) {
        QMessageBox::critical(this, "Export Failed", QString::fromStdString(r.message));
    } else {
        auto reply = QMessageBox::information(
            this,
            "Export Successful",
            QString("Successfully exported to:\n%1\n\nWould you like to open this converted database now?").arg(targetFile),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            setDb(targetFile.toStdString());
        }
    }
}

void MainWindow::exportToInteractiveHtml(const QString& filePath)
{
    if (graphView_)
        graphView_->exportToInteractiveHtml(filePath);
}

void MainWindow::reviewSelected()
{
    if (!model_) return;

    bool hasCanvasSelection = scene_ && !scene_->selectedItems().isEmpty();
    QModelIndex navIdx = navigator_ ? navigator_->currentIndex() : QModelIndex();

    if (!hasCanvasSelection && !navIdx.isValid())
    {
        statusBar()->showMessage("Select one or more elements to review", 2500);
        return;
    }

    QString defUser = QString::fromLocal8Bit(qgetenv("USER"));
    if (defUser.isEmpty()) defUser = QString::fromLocal8Bit(qgetenv("USERNAME"));
    if (defUser.isEmpty()) defUser = "architect";

    bool ok = false;
    QString reviewer = QInputDialog::getText(
        this, "Review Selected", "Enter Reviewer ID / Name:",
        QLineEdit::Normal, defUser, &ok);

    if (!ok || reviewer.trimmed().isEmpty()) return;
    std::string revStr = reviewer.trimmed().toStdString();

    int count = 0;
    if (hasCanvasSelection)
    {
        for (auto* item : scene_->selectedItems())
        {
            if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
            {
                auto s = model_->getContainerGovernanceSummary(nodeItem->nodeId());
                if (s.totalChildren > 0)
                {
                    auto nOpt = model_->getNodeById(nodeItem->nodeId());
                    QString name = nOpt ? QString::fromStdString(nOpt->name) : QString("Node #%1").arg(nodeItem->nodeId());
                    auto reply = QMessageBox::question(
                        this, "Cascade to Child Components?",
                        QString("Container '%1' has %2 child node(s) and %3 internal edge(s).\n\n"
                                "Do you want to cascade this review to all child components?")
                            .arg(name).arg(s.totalChildren).arg(s.totalInternalEdges),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

                    if (reply == QMessageBox::Yes)
                    {
                        model_->cascadeNodeGovernance(nodeItem->nodeId(), Status::Reviewed, revStr);
                        count += (1 + s.totalChildren + s.totalInternalEdges);
                        continue;
                    }
                }
                model_->setNodeGovernance(nodeItem->nodeId(), Status::Reviewed, revStr);
                count++;
            }
            else if (auto* edgeItem = dynamic_cast<GraphEdgeItem*>(item))
            {
                model_->setEdgeGovernance(edgeItem->edgeId(), Status::Reviewed, revStr);
                count++;
            }
        }

        for (auto* item : scene_->items())
        {
            if (auto* ni = dynamic_cast<GraphNodeItem*>(item))
            {
                ni->refreshGeometry();
                ni->update();
            }
            else if (auto* ei = dynamic_cast<GraphEdgeItem*>(item))
            {
                ei->refreshLayout();
                ei->update();
            }
        }
    }
    else if (navIdx.isValid())
    {
        QModelIndex srcIndex = navProxyModel_ ? navProxyModel_->mapToSource(navIdx) : navIdx;
        auto* item = navModel_->itemFromIndex(srcIndex);
        if (item)
        {
            auto type = static_cast<ItemType>(item->data(NavRole::Type).toInt());
            qulonglong id = item->data(NavRole::Id).toULongLong();
            if (type == ItemType::Node)
            {
                auto s = model_->getContainerGovernanceSummary(id);
                if (s.totalChildren > 0)
                {
                    auto nOpt = model_->getNodeById(id);
                    QString name = nOpt ? QString::fromStdString(nOpt->name) : QString("Node #%1").arg(id);
                    auto reply = QMessageBox::question(
                        this, "Cascade to Child Components?",
                        QString("Container '%1' has %2 child node(s) and %3 internal edge(s).\n\n"
                                "Do you want to cascade this review to all child components?")
                            .arg(name).arg(s.totalChildren).arg(s.totalInternalEdges),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

                    if (reply == QMessageBox::Yes)
                    {
                        model_->cascadeNodeGovernance(id, Status::Reviewed, revStr);
                        count += (1 + s.totalChildren + s.totalInternalEdges);
                    }
                    else
                    {
                        model_->setNodeGovernance(id, Status::Reviewed, revStr);
                        count++;
                    }
                }
                else
                {
                    model_->setNodeGovernance(id, Status::Reviewed, revStr);
                    count++;
                }
            }
            else if (type == ItemType::Layer)
            {
                model_->setLayerGovernance(id, Status::Reviewed, revStr);
                count++;
            }
        }
    }

    populateNavigator();
    if (scene_) scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
    if (graphView_) graphView_->viewport()->update();
    statusBar()->showMessage(QString("Marked %1 item(s) as Reviewed by %2").arg(count).arg(reviewer), 3000);
}

void MainWindow::approveSelected()
{
    if (!model_) return;

    bool hasCanvasSelection = scene_ && !scene_->selectedItems().isEmpty();
    QModelIndex navIdx = navigator_ ? navigator_->currentIndex() : QModelIndex();

    if (!hasCanvasSelection && !navIdx.isValid())
    {
        statusBar()->showMessage("Select one or more elements to approve", 2500);
        return;
    }

    QString defUser = QString::fromLocal8Bit(qgetenv("USER"));
    if (defUser.isEmpty()) defUser = QString::fromLocal8Bit(qgetenv("USERNAME"));
    if (defUser.isEmpty()) defUser = "architect";

    bool ok = false;
    QString reviewer = QInputDialog::getText(
        this, "Approve Selected", "Enter Reviewer ID / Name:",
        QLineEdit::Normal, defUser, &ok);

    if (!ok || reviewer.trimmed().isEmpty()) return;
    std::string revStr = reviewer.trimmed().toStdString();

    int count = 0;
    if (hasCanvasSelection)
    {
        for (auto* item : scene_->selectedItems())
        {
            if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
            {
                auto s = model_->getContainerGovernanceSummary(nodeItem->nodeId());
                if (s.totalChildren > 0)
                {
                    auto nOpt = model_->getNodeById(nodeItem->nodeId());
                    QString name = nOpt ? QString::fromStdString(nOpt->name) : QString("Node #%1").arg(nodeItem->nodeId());
                    auto reply = QMessageBox::question(
                        this, "Cascade to Child Components?",
                        QString("Container '%1' has %2 child node(s) and %3 internal edge(s).\n\n"
                                "Do you want to cascade this approval to all child components?")
                            .arg(name).arg(s.totalChildren).arg(s.totalInternalEdges),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

                    if (reply == QMessageBox::Yes)
                    {
                        model_->cascadeNodeGovernance(nodeItem->nodeId(), Status::Approved, revStr);
                        count += (1 + s.totalChildren + s.totalInternalEdges);
                        continue;
                    }
                }
                model_->setNodeGovernance(nodeItem->nodeId(), Status::Approved, revStr);
                count++;
            }
            else if (auto* edgeItem = dynamic_cast<GraphEdgeItem*>(item))
            {
                model_->setEdgeGovernance(edgeItem->edgeId(), Status::Approved, revStr);
                count++;
            }
        }

        for (auto* item : scene_->items())
        {
            if (auto* ni = dynamic_cast<GraphNodeItem*>(item))
            {
                ni->refreshGeometry();
                ni->update();
            }
            else if (auto* ei = dynamic_cast<GraphEdgeItem*>(item))
            {
                ei->refreshLayout();
                ei->update();
            }
        }
    }
    else if (navIdx.isValid())
    {
        QModelIndex srcIndex = navProxyModel_ ? navProxyModel_->mapToSource(navIdx) : navIdx;
        auto* item = navModel_->itemFromIndex(srcIndex);
        if (item)
        {
            auto type = static_cast<ItemType>(item->data(NavRole::Type).toInt());
            qulonglong id = item->data(NavRole::Id).toULongLong();
            if (type == ItemType::Node)
            {
                auto s = model_->getContainerGovernanceSummary(id);
                if (s.totalChildren > 0)
                {
                    auto nOpt = model_->getNodeById(id);
                    QString name = nOpt ? QString::fromStdString(nOpt->name) : QString("Node #%1").arg(id);
                    auto reply = QMessageBox::question(
                        this, "Cascade to Child Components?",
                        QString("Container '%1' has %2 child node(s) and %3 internal edge(s).\n\n"
                                "Do you want to cascade this approval to all child components?")
                            .arg(name).arg(s.totalChildren).arg(s.totalInternalEdges),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

                    if (reply == QMessageBox::Yes)
                    {
                        model_->cascadeNodeGovernance(id, Status::Approved, revStr);
                        count += (1 + s.totalChildren + s.totalInternalEdges);
                    }
                    else
                    {
                        model_->setNodeGovernance(id, Status::Approved, revStr);
                        count++;
                    }
                }
                else
                {
                    model_->setNodeGovernance(id, Status::Approved, revStr);
                    count++;
                }
            }
            else if (type == ItemType::Layer)
            {
                model_->setLayerGovernance(id, Status::Approved, revStr);
                count++;
            }
        }
    }

    populateNavigator();
    if (scene_) scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
    if (graphView_) graphView_->viewport()->update();
    statusBar()->showMessage(QString("Marked %1 item(s) as Approved by %2").arg(count).arg(reviewer), 3000);
}

void MainWindow::markSelectedChanged()
{
    if (!model_) return;

    int count = 0;
    if (scene_ && !scene_->selectedItems().isEmpty())
    {
        for (auto* item : scene_->selectedItems())
        {
            if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
            {
                model_->setNodeGovernance(nodeItem->nodeId(), Status::Changed, "");
                nodeItem->refreshGeometry();
                nodeItem->update();
                count++;
            }
            else if (auto* edgeItem = dynamic_cast<GraphEdgeItem*>(item))
            {
                model_->setEdgeGovernance(edgeItem->edgeId(), Status::Changed, "");
                edgeItem->refreshLayout();
                count++;
            }
        }
    }
    else if (navigator_ && navigator_->currentIndex().isValid())
    {
        QModelIndex srcIndex = navProxyModel_ ? navProxyModel_->mapToSource(navigator_->currentIndex()) : navigator_->currentIndex();
        auto* item = navModel_->itemFromIndex(srcIndex);
        if (item)
        {
            auto type = static_cast<ItemType>(item->data(NavRole::Type).toInt());
            qulonglong id = item->data(NavRole::Id).toULongLong();
            if (type == ItemType::Node)
            {
                model_->setNodeGovernance(id, Status::Changed, "");
                count++;
            }
            else if (type == ItemType::Layer)
            {
                model_->setLayerGovernance(id, Status::Changed, "");
                count++;
            }
        }
    }

    populateNavigator();
    if (scene_)
    {
        for (auto* it : scene_->items())
        {
            if (auto* ni = dynamic_cast<GraphNodeItem*>(it))
            {
                ni->refreshGeometry();
                ni->update();
            }
            else if (auto* ei = dynamic_cast<GraphEdgeItem*>(it))
            {
                ei->refreshLayout();
                ei->update();
            }
        }
        scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
    }
    if (graphView_) graphView_->viewport()->update();
    statusBar()->showMessage(QString("Reset %1 item(s) to Changed status").arg(count), 2500);
}

void MainWindow::markSelectedInvalid()
{
    if (!model_) return;

    int count = 0;
    if (scene_ && !scene_->selectedItems().isEmpty())
    {
        for (auto* item : scene_->selectedItems())
        {
            if (auto* nodeItem = dynamic_cast<GraphNodeItem*>(item))
            {
                model_->setNodeGovernance(nodeItem->nodeId(), Status::Invalid, "");
                count++;
            }
            else if (auto* edgeItem = dynamic_cast<GraphEdgeItem*>(item))
            {
                model_->setEdgeGovernance(edgeItem->edgeId(), Status::Invalid, "");
                count++;
            }
        }
    }
    else if (navigator_ && navigator_->currentIndex().isValid())
    {
        QModelIndex srcIndex = navProxyModel_ ? navProxyModel_->mapToSource(navigator_->currentIndex()) : navigator_->currentIndex();
        auto* item = navModel_->itemFromIndex(srcIndex);
        if (item)
        {
            auto type = static_cast<ItemType>(item->data(NavRole::Type).toInt());
            qulonglong id = item->data(NavRole::Id).toULongLong();
            if (type == ItemType::Node)
            {
                model_->setNodeGovernance(id, Status::Invalid, "");
                count++;
            }
            else if (type == ItemType::Layer)
            {
                model_->setLayerGovernance(id, Status::Invalid, "");
                count++;
            }
        }
    }

    populateNavigator();
    if (scene_)
    {
        for (auto* it : scene_->items())
        {
            if (auto* ni = dynamic_cast<GraphNodeItem*>(it))
            {
                ni->refreshGeometry();
                ni->update();
            }
            else if (auto* ei = dynamic_cast<GraphEdgeItem*>(it))
            {
                ei->refreshLayout();
                ei->update();
            }
        }
        scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
    }
    if (graphView_) graphView_->viewport()->update();
    statusBar()->showMessage(QString("Marked %1 item(s) as Invalid").arg(count), 2500);
}

void MainWindow::verifyModelIntegrity()
{
    if (!model_) return;

    auto report = model_->auditGovernance();

    QString statusSummary = QString(
        "<h3>Architecture Governance & Integrity Audit</h3>"
        "<b>Model Inventory:</b><br>"
        "&nbsp;&nbsp;• <b>Nodes:</b> %1<br>"
        "&nbsp;&nbsp;• <b>Layers:</b> %2<br>"
        "&nbsp;&nbsp;• <b>Edges:</b> %3<br><br>"
        "<b>Lifecycle Status Distribution:</b><br>"
        "&nbsp;&nbsp;• <span style='color:#a3be8c;font-weight:bold;'>Approved:</span> %4<br>"
        "&nbsp;&nbsp;• <span style='color:#b48ead;font-weight:bold;'>Reviewed:</span> %5<br>"
        "&nbsp;&nbsp;• <span style='color:#ebcb8b;font-weight:bold;'>Changed:</span> %6<br>"
        "&nbsp;&nbsp;• <span style='color:#88c0d0;font-weight:bold;'>New:</span> %7<br>"
        "&nbsp;&nbsp;• <span style='color:#bf616a;font-weight:bold;'>Invalid:</span> %8<br><br>")
        .arg(report.totalNodes)
        .arg(report.totalLayers)
        .arg(report.totalEdges)
        .arg(report.approvedCount)
        .arg(report.reviewedCount)
        .arg(report.changedCount)
        .arg(report.newCount)
        .arg(report.invalidCount);

    size_t totalTampered = report.tamperedNodes + report.tamperedLayers + report.tamperedEdges;
    if (totalTampered == 0)
    {
        statusSummary += "<b style='color:#a3be8c;'>✓ Tamper Verification Passed:</b> All cryptographic checksums match database records.";
        QMessageBox::information(this, "Architecture Integrity Audit", statusSummary);
    }
    else
    {
        statusSummary += QString("<b style='color:#bf616a;'>⚠ Tamper Verification Warning:</b> Detected %1 integrity violation(s)!<br><br>").arg(totalTampered);
        for (const auto& issue : report.issues)
        {
            statusSummary += QString("• %1<br>").arg(QString::fromStdString(issue).toHtmlEscaped());
        }
        QMessageBox::warning(this, "Architecture Integrity Warning", statusSummary);
    }
}

void MainWindow::onNavigatorContextMenu(const QPoint& pos)
{
    QModelIndex index = navigator_->indexAt(pos);
    if (!index.isValid() || !model_) return;

    QModelIndex srcIndex = navProxyModel_ ? navProxyModel_->mapToSource(index) : index;
    auto* item = navModel_->itemFromIndex(srcIndex);
    if (!item) return;

    auto type = static_cast<ItemType>(item->data(NavRole::Type).toInt());
    qulonglong id = item->data(NavRole::Id).toULongLong();

    QMenu menu(this);
    if (type == ItemType::Node)
    {
        QAction* showAct = menu.addAction(QIcon(":/icons/layers.svg"), "Show in Graph");

        auto nodeOpt = model_->getNodeById(id);
        auto summary = model_->getContainerGovernanceSummary(id);
        QAction* focusAct = nullptr;
        QAction* sizeAct = nullptr;
        if (nodeOpt && (nodeOpt->type == "container" || summary.totalChildren > 0))
        {
            focusAct = menu.addAction(QIcon(":/icons/layers.svg"), "Drill Down / Edit Container Separately (Ctrl+Down)");
            sizeAct  = menu.addAction("📐 Manage Container Size...");
        }

        menu.addSeparator();
        QAction* editAct = menu.addAction("Edit Node...");
        menu.addSeparator();
        auto* govMenu = menu.addMenu("Governance");
        QAction* reviewAct  = govMenu->addAction("Mark as Reviewed...");
        QAction* approveAct = govMenu->addAction("Mark as Approved...");
        QAction* changeAct  = govMenu->addAction("Mark as Changed");
        QAction* invalidAct = govMenu->addAction("Mark as Invalid");

        QAction* cascadeApproveAct = nullptr;
        QAction* cascadeReviewAct  = nullptr;
        if (summary.totalChildren > 0)
        {
            govMenu->addSeparator();
            cascadeApproveAct = govMenu->addAction(QString("Cascade Approve (%1 Children, %2 Edges)...").arg(summary.totalChildren).arg(summary.totalInternalEdges));
            cascadeReviewAct  = govMenu->addAction(QString("Cascade Review (%1 Children, %2 Edges)...").arg(summary.totalChildren).arg(summary.totalInternalEdges));
        }

        govMenu->addSeparator();
        QAction* verifyAct  = govMenu->addAction("Verify Checksum Integrity");
        menu.addSeparator();
        QAction* delAct = menu.addAction("Delete Node");

        QAction* chosen = menu.exec(navigator_->viewport()->mapToGlobal(pos));
        if (chosen == focusAct)
        {
            focusIntoContainer(id);
            return;
        }
        else if (chosen == sizeAct)
        {
            openContainerSizeDialog(id);
            return;
        }
        else if (chosen == showAct)
        {
            showNodeInGraph(id);
        }
        else if (chosen == editAct)
        {
            NodeEditorDialog dlg(model_, id, this);
            if (dlg.exec() == QDialog::Accepted)
            {
                populateNavigator();
                renderGraph(model_->extractGraph(std::nullopt));
                setSaveStatus(DbSaveState::Saved, "Node updated in database");
            }
        }
        else if (chosen == delAct)
        {
            deleteSelected();
        }
        else if (chosen == reviewAct || chosen == approveAct)
        {
            bool isApprove = (chosen == approveAct);
            QString defUser = QString::fromLocal8Bit(qgetenv("USER"));
            if (defUser.isEmpty()) defUser = "architect";

            bool ok = false;
            QString reviewer = QInputDialog::getText(
                this, isApprove ? "Approve Node" : "Review Node",
                "Enter Reviewer ID / Name:", QLineEdit::Normal, defUser, &ok);

            if (ok && !reviewer.trimmed().isEmpty())
            {
                Status newStatus = isApprove ? Status::Approved : Status::Reviewed;
                std::string revStr = reviewer.trimmed().toStdString();

                bool didCascade = false;
                if (summary.totalChildren > 0)
                {
                    auto nOpt = model_->getNodeById(id);
                    QString name = nOpt ? QString::fromStdString(nOpt->name) : QString("Node #%1").arg(id);
                    auto reply = QMessageBox::question(
                        this,
                        "Cascade to Child Components?",
                        QString("Container '%1' has %2 child node(s) and %3 internal edge(s).\n\n"
                                "Do you want to cascade this %4 to all child components?")
                            .arg(name)
                            .arg(summary.totalChildren)
                            .arg(summary.totalInternalEdges)
                            .arg(isApprove ? "approval" : "review"),
                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                        QMessageBox::Yes);

                    if (reply == QMessageBox::Cancel)
                        return;

                    if (reply == QMessageBox::Yes)
                    {
                        model_->cascadeNodeGovernance(id, newStatus, revStr);
                        didCascade = true;
                    }
                }

                if (!didCascade)
                {
                    model_->setNodeGovernance(id, newStatus, revStr);
                }

                populateNavigator();
                if (scene_) scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
                if (graphView_) graphView_->viewport()->update();
            }
        }
        else if (chosen == cascadeApproveAct || chosen == cascadeReviewAct)
        {
            bool isApprove = (chosen == cascadeApproveAct);
            QString defUser = QString::fromLocal8Bit(qgetenv("USER"));
            if (defUser.isEmpty()) defUser = "architect";

            bool ok = false;
            QString reviewer = QInputDialog::getText(
                this, isApprove ? "Cascade Approve Container" : "Cascade Review Container",
                "Enter Reviewer ID / Name:", QLineEdit::Normal, defUser, &ok);

            if (ok && !reviewer.trimmed().isEmpty())
            {
                Status newStatus = isApprove ? Status::Approved : Status::Reviewed;
                model_->cascadeNodeGovernance(id, newStatus, reviewer.trimmed().toStdString());
                populateNavigator();
                if (scene_) scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
                if (graphView_) graphView_->viewport()->update();
            }
        }
        else if (chosen == changeAct)
        {
            model_->setNodeGovernance(id, Status::Changed, "");
            populateNavigator();
            if (scene_) scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
            if (graphView_) graphView_->viewport()->update();
        }
        else if (chosen == invalidAct)
        {
            model_->setNodeGovernance(id, Status::Invalid, "");
            populateNavigator();
            if (scene_) scene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
            if (graphView_) graphView_->viewport()->update();
        }
        else if (chosen == verifyAct)
        {
            auto nodeOpt = model_->getNodeById(id);
            if (nodeOpt)
            {
                uint32_t expected = model_->computeNodeChecksum(*nodeOpt);
                bool match = (expected == nodeOpt->checksum);
                QString msg = QString("<b>Node:</b> %1 (ID %2)<br><b>Status:</b> %3<br><b>Reviewer:</b> %4<br><br><b>Stored Checksum:</b> 0x%5<br><b>Calculated Checksum:</b> 0x%6<br><br><b>Integrity:</b> %7")
                    .arg(QString::fromStdString(nodeOpt->name).toHtmlEscaped())
                    .arg(id)
                    .arg(QString::fromStdString(to_string(nodeOpt->status)).toUpper())
                    .arg(nodeOpt->reviewer.empty() ? "<i>(None)</i>" : QString::fromStdString(nodeOpt->reviewer).toHtmlEscaped())
                    .arg(QString::number(nodeOpt->checksum, 16).toUpper())
                    .arg(QString::number(expected, 16).toUpper())
                    .arg(match ? "<span style='color:#a3be8c;font-weight:bold;'>✓ Valid (Tamper-Free)</span>" : "<span style='color:#bf616a;font-weight:bold;'>⚠ CHECKSUM MISMATCH (TAMPER DETECTED)</span>");

                if (summary.totalChildren > 0)
                {
                    msg += QString("<hr><b>Child Components Summary:</b><br>"
                                   "• Child Nodes (%1): %2 approved, %3 changed, %4 new, %5 invalid<br>"
                                   "• Internal Edges (%6): %7 approved, %8 changed, %9 new, %10 invalid<br>"
                                   "<b>Rollup Status:</b> %11")
                        .arg(summary.totalChildren).arg(summary.approvedChildren).arg(summary.changedChildren).arg(summary.newChildren).arg(summary.invalidChildren)
                        .arg(summary.totalInternalEdges).arg(summary.approvedEdges).arg(summary.changedEdges).arg(summary.newEdges).arg(summary.invalidEdges)
                        .arg(QString::fromStdString(to_string(summary.rollupStatus)).toUpper());
                }

                QMessageBox::information(this, "Governance Integrity Verification", msg);
            }
        }
    }
    else if (type == ItemType::Layer)
    {
        QAction* showAct = menu.addAction(QIcon(":/icons/layers.svg"), "Show in Graph");
        menu.addSeparator();
        QAction* editAct = menu.addAction("Edit Layer...");
        menu.addSeparator();
        auto* govMenu = menu.addMenu("Governance");
        QAction* reviewAct  = govMenu->addAction("Mark as Reviewed...");
        QAction* approveAct = govMenu->addAction("Mark as Approved...");
        QAction* changeAct  = govMenu->addAction("Mark as Changed");
        QAction* invalidAct = govMenu->addAction("Mark as Invalid");
        govMenu->addSeparator();
        QAction* verifyAct  = govMenu->addAction("Verify Checksum Integrity");
        menu.addSeparator();
        QAction* delAct = menu.addAction("Delete Layer");

        QAction* chosen = menu.exec(navigator_->viewport()->mapToGlobal(pos));
        if (chosen == showAct)
        {
            showLayerInGraph(id);
        }
        else if (chosen == editAct)
        {
            LayerEditorDialog dlg(model_, id, this);
            if (dlg.exec() == QDialog::Accepted)
            {
                populateNavigator();
                renderGraph(model_->extractGraph(id));
                setSaveStatus(DbSaveState::Saved, "Layer updated in database");
            }
        }
        else if (chosen == delAct)
        {
            auto reply = QMessageBox::question(this, "Delete Layer", "Are you sure you want to delete this layer?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (reply == QMessageBox::Yes)
            {
                model_->deleteLayer(id);
                populateNavigator();
                renderGraph(model_->extractGraph(std::nullopt));
            }
        }
        else if (chosen == reviewAct || chosen == approveAct)
        {
            bool isApprove = (chosen == approveAct);
            QString defUser = QString::fromLocal8Bit(qgetenv("USER"));
            if (defUser.isEmpty()) defUser = "architect";

            bool ok = false;
            QString reviewer = QInputDialog::getText(
                this, isApprove ? "Approve Layer" : "Review Layer",
                "Enter Reviewer ID / Name:", QLineEdit::Normal, defUser, &ok);

            if (ok && !reviewer.trimmed().isEmpty())
            {
                model_->setLayerGovernance(id, isApprove ? Status::Approved : Status::Reviewed, reviewer.trimmed().toStdString());
                populateNavigator();
            }
        }
        else if (chosen == changeAct)
        {
            model_->setLayerGovernance(id, Status::Changed, "");
            populateNavigator();
        }
        else if (chosen == invalidAct)
        {
            model_->setLayerGovernance(id, Status::Invalid, "");
            populateNavigator();
        }
        else if (chosen == verifyAct)
        {
            auto layerOpt = model_->getLayerById(id);
            if (layerOpt)
            {
                uint32_t expected = model_->computeLayerChecksum(*layerOpt);
                bool match = (expected == layerOpt->checksum);
                QString msg = QString("<b>Layer:</b> %1 (ID %2)<br><b>Status:</b> %3<br><b>Reviewer:</b> %4<br><br><b>Stored Checksum:</b> 0x%5<br><b>Calculated Checksum:</b> 0x%6<br><br><b>Integrity:</b> %7")
                    .arg(QString::fromStdString(layerOpt->name).toHtmlEscaped())
                    .arg(id)
                    .arg(QString::fromStdString(to_string(layerOpt->status)).toUpper())
                    .arg(layerOpt->reviewer.empty() ? "<i>(None)</i>" : QString::fromStdString(layerOpt->reviewer).toHtmlEscaped())
                    .arg(QString::number(layerOpt->checksum, 16).toUpper())
                    .arg(QString::number(expected, 16).toUpper())
                    .arg(match ? "<span style='color:#a3be8c;font-weight:bold;'>✓ Valid (Tamper-Free)</span>" : "<span style='color:#bf616a;font-weight:bold;'>⚠ CHECKSUM MISMATCH (TAMPER DETECTED)</span>");
                QMessageBox::information(this, "Governance Integrity Verification", msg);
            }
        }
    }
}

void MainWindow::setupSaveStatusWidget()
{
    saveStatusContainer_ = new QWidget(this);
    auto* layout = new QHBoxLayout(saveStatusContainer_);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(5);

    saveStatusIndicator_ = new QLabel(this);
    saveStatusLabel_ = new QLabel(this);

    layout->addWidget(saveStatusIndicator_);
    layout->addWidget(saveStatusLabel_);

    saveStatusContainer_->setCursor(Qt::PointingHandCursor);
    saveStatusContainer_->installEventFilter(this);

    statusBar()->addPermanentWidget(saveStatusContainer_);
    setSaveStatus(DbSaveState::NoDb);
}

void MainWindow::setSaveStatus(DbSaveState state, const QString& details)
{
    currentSaveState_ = state;

    if (!saveStatusIndicator_ || !saveStatusLabel_)
        return;

    QString indicatorText;
    QString labelText;
    QString tooltipText;

    QString dbFile;
    if (!currentDbPath_.empty())
    {
        dbFile = QFileInfo(QString::fromStdString(currentDbPath_)).fileName();
    }

    switch (state)
    {
    case DbSaveState::NoDb:
        indicatorText = "<span style='color: #7f8c8d;'>○</span>";
        labelText = "<span style='color: #7f8c8d;'>No database open</span>";
        tooltipText = "Open an SQLite database or Architecture JSON project to begin.";
        break;

    case DbSaveState::Saving:
        indicatorText = "<span style='color: #3498db; font-size: 13px;'>💾</span>";
        labelText = "<span style='color: #3498db; font-weight: bold;'>Saving...</span>";
        tooltipText = details.isEmpty() ? "Writing changes to database..." : details;
        statusBar()->showMessage(details.isEmpty() ? "Saving database changes..." : details, 1500);
        break;

    case DbSaveState::Saved:
    {
        QString timeStr = QTime::currentTime().toString("HH:mm:ss");
        indicatorText = "<span style='color: #2ecc71; font-weight: bold;'>✓</span>";
        labelText = QString("<span style='color: #2ecc71;'>Saved (%1)</span>").arg(timeStr);
        tooltipText = QString("All changes saved to '%1' at %2.").arg(dbFile.isEmpty() ? "database" : dbFile, timeStr);
        if (!details.isEmpty())
        {
            statusBar()->showMessage(details, 2500);
        }
        break;
    }

    case DbSaveState::UnsavedChanges:
        indicatorText = "<span style='color: #e67e22; font-size: 13px;'>●</span>";
        labelText = "<span style='color: #e67e22; font-weight: bold; text-decoration: underline;'>Unsaved changes</span>";
        tooltipText = "Layout modifications are in memory. Click here or press Ctrl+S to save to database.";
        statusBar()->showMessage("Unsaved layout changes (press Ctrl+S to save)", 3000);
        break;

    case DbSaveState::Error:
        indicatorText = "<span style='color: #e74c3c; font-weight: bold;'>⚠</span>";
        labelText = "<span style='color: #e74c3c; font-weight: bold;'>Save Failed</span>";
        tooltipText = details.isEmpty() ? "An error occurred while saving to database." : details;
        statusBar()->showMessage(QString("Save error: %1").arg(details), 4000);
        break;
    }

    saveStatusIndicator_->setText(indicatorText);
    saveStatusLabel_->setText(labelText);
    saveStatusContainer_->setToolTip(tooltipText);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == saveStatusContainer_ && event->type() == QEvent::MouseButtonRelease)
    {
        if (currentSaveState_ == DbSaveState::UnsavedChanges)
        {
            saveLayout();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (maybePromptSaveUnsavedChanges())
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

bool MainWindow::maybePromptSaveUnsavedChanges()
{
    if (!model_ || !hasUnsavedLayoutChanges_ || ApplicationConfig::instance().isAutoSaveEnabled())
    {
        return true;
    }

    QString dbName = QString::fromStdString(currentDbPath_);
    if (dbName.isEmpty())
        dbName = "current architecture";
    else
        dbName = QFileInfo(dbName).fileName();

    auto reply = QMessageBox::question(
        this,
        "Unsaved Layout Changes",
        QString("You have unsaved layout modifications in '%1'.\n\nWould you like to save them before proceeding?").arg(dbName),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save
    );

    if (reply == QMessageBox::Save)
    {
        saveLayout();
        return true;
    }
    else if (reply == QMessageBox::Discard)
    {
        hasUnsavedLayoutChanges_ = false;
        return true;
    }
    else
    {
        return false;
    }
}

void MainWindow::openPreferencesDialog()
{
    PreferencesDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted)
    {
        if (appThemeMenu_)
            setupApplicationThemeMenu(appThemeMenu_);
        if (actionAutoSave_)
            actionAutoSave_->setChecked(ApplicationConfig::instance().isAutoSaveEnabled());
    }
}

void MainWindow::openFontChooserDialog()
{
    bool ok = false;
    QFont currentFont = ApplicationConfig::instance().applicationFont();
    QFont font = QFontDialog::getFont(&ok, currentFont, this, "Select Application Font");
    if (ok)
    {
        ApplicationConfig::instance().setApplicationFont(font);
        statusBar()->showMessage(QString("Application font updated: %1, %2pt")
            .arg(font.family())
            .arg(font.pointSize() > 0 ? font.pointSize() : 10), 3000);
    }
}

void MainWindow::onAutoSaveToggled(bool checked)
{
    ApplicationConfig::instance().setAutoSaveEnabled(checked);
}

void MainWindow::onAutoSaveConfigChanged(bool enabled)
{
    if (actionAutoSave_)
    {
        actionAutoSave_->setChecked(enabled);
    }

    if (enabled)
    {
        if (hasUnsavedLayoutChanges_)
        {
            saveLayout();
        }
        statusBar()->showMessage("Auto-save enabled", 2000);
    }
    else
    {
        if (autoSaveTimer_)
        {
            autoSaveTimer_->stop();
        }
        statusBar()->showMessage("Auto-save disabled. Press Ctrl+S to save layout.", 3000);
    }
}

void MainWindow::onAutoSaveDelayChanged(int ms)
{
    if (autoSaveTimer_)
    {
        autoSaveTimer_->setInterval(ms);
    }
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(
        this,
        "About OpenArch",
        "<h3>OpenArch</h3>"
        "<p>Cross-platform software architecture modeling, visualization, and governance platform.</p>"
        "<p>Version 1.1.0 &bull; Qt " QT_VERSION_STR " &bull; MIT License</p>"
    );
}

void MainWindow::showShortcutsReferenceDialog()
{
    openShortcutConfigDialog();
}

void MainWindow::focusIntoContainer(NodeId containerId)
{
    if (!model_) return;
    auto opt = model_->getNodeById(containerId);
    if (!opt) return;

    saveLayout();

    // Build breadcrumb trail from root down to containerId
    std::vector<NodeId> trail;
    NodeId curr = containerId;
    std::unordered_set<NodeId> visited;
    while (true)
    {
        if (visited.count(curr) > 0) break;
        visited.insert(curr);
        trail.push_back(curr);

        auto n = model_->getNodeById(curr);
        if (n && n->parentId.has_value() && *n->parentId != 0)
        {
            curr = *n->parentId;
        }
        else
        {
            break;
        }
    }
    std::reverse(trail.begin(), trail.end());
    containerBreadcrumbTrail_ = trail;
    activeContainerScope_ = containerId;

    applyContainerScope(containerId);
}

void MainWindow::stepUpContainerScope()
{
    if (!activeContainerScope_.has_value() || containerBreadcrumbTrail_.empty())
    {
        exitContainerScope();
        return;
    }

    saveLayout();

    containerBreadcrumbTrail_.pop_back();
    if (containerBreadcrumbTrail_.empty())
    {
        exitContainerScope();
    }
    else
    {
        NodeId parentCid = containerBreadcrumbTrail_.back();
        activeContainerScope_ = parentCid;
        applyContainerScope(parentCid);
    }
}

void MainWindow::exitContainerScope()
{
    if (!activeContainerScope_.has_value()) return;

    saveLayout();

    activeContainerScope_ = std::nullopt;
    containerBreadcrumbTrail_.clear();
    applyContainerScope(std::nullopt);
}

void MainWindow::focusSelectedContainer()
{
    if (!scene_) return;
    for (QGraphicsItem* it : scene_->selectedItems())
    {
        if (auto* n = dynamic_cast<GraphNodeItem*>(it))
        {
            if (n->isContainer())
            {
                focusIntoContainer(n->nodeId());
                return;
            }
        }
    }
}

std::unordered_set<NodeId> MainWindow::collectScopedNodeIds(NodeId rootContainerId) const
{
    std::unordered_set<NodeId> result;
    if (!model_) return result;

    std::vector<NodeId> queue = {rootContainerId};
    auto allNodes = model_->nodes();

    while (!queue.empty())
    {
        NodeId parent = queue.back();
        queue.pop_back();

        for (const auto& n : allNodes)
        {
            if (n.parentId.has_value() && *n.parentId == parent)
            {
                if (result.insert(n.id).second)
                {
                    queue.push_back(n.id);
                }
            }
        }
    }

    return result;
}

GraphSnapshot MainWindow::extractScopedSnapshot(NodeId containerId) const
{
    GraphSnapshot snap;
    if (!model_) return snap;

    std::unordered_set<NodeId> scopedNodeIds = collectScopedNodeIds(containerId);

    qreal cWidth = 800.0;
    qreal cHeight = 600.0;
    auto targetOpt = model_->getNodeById(containerId);
    if (targetOpt && !targetOpt->metadata.empty())
    {
        auto doc = QJsonDocument::fromJson(QString::fromStdString(targetOpt->metadata).toUtf8());
        if (doc.isObject())
        {
            auto obj = doc.object();
            if (obj.contains("_width")) cWidth = obj["_width"].toDouble(cWidth);
            if (obj.contains("_height")) cHeight = obj["_height"].toDouble(cHeight);
        }
    }

    qreal maxInternalX = 200.0;
    qreal maxInternalY = 200.0;

    // containerId itself is NOT added as a node item: its interior IS the canvas!
    for (const auto& n : model_->nodes())
    {
        if (scopedNodeIds.count(n.id) > 0)
        {
            NodeData scopedNode = n;
            // Direct children of containerId become top-level canvas items on the scoped sub-diagram
            if (scopedNode.parentId.has_value() && *scopedNode.parentId == containerId)
            {
                scopedNode.parentId = std::nullopt;
            }
            if (!scopedNode.metadata.empty())
            {
                auto doc = QJsonDocument::fromJson(QString::fromStdString(scopedNode.metadata).toUtf8());
                if (doc.isObject())
                {
                    qreal x = doc.object()["x"].toDouble(0);
                    qreal y = doc.object()["y"].toDouble(0);
                    maxInternalX = std::max(maxInternalX, x + 160.0);
                    maxInternalY = std::max(maxInternalY, y + 80.0);
                }
            }
            snap.nodes.push_back(scopedNode);
        }
    }

    cWidth = std::max(cWidth, maxInternalX + 40.0);
    cHeight = std::max(cHeight, maxInternalY + 40.0);

    // Identify Internal and External (Cross-Boundary) Edges
    std::unordered_set<NodeId> externalProxyNodeIds;
    std::unordered_map<NodeId, bool> externalNodeIsOutgoing;
    std::unordered_map<NodeId, QPointF> internalAnchorPositions;

    for (const auto& e : model_->edges())
    {
        bool srcIn = scopedNodeIds.count(e.srcNode) > 0;
        bool dstIn = scopedNodeIds.count(e.dstNode) > 0;

        if (srcIn && dstIn)
        {
            snap.edges.push_back(e);
        }
        else if (srcIn && !dstIn)
        {
            // Outgoing external link: Internal -> Outside
            snap.edges.push_back(e);
            externalProxyNodeIds.insert(e.dstNode);
            externalNodeIsOutgoing[e.dstNode] = true;

            auto sOpt = model_->getNodeById(e.srcNode);
            if (sOpt && !sOpt->metadata.empty())
            {
                auto doc = QJsonDocument::fromJson(QString::fromStdString(sOpt->metadata).toUtf8());
                if (doc.isObject())
                {
                    internalAnchorPositions[e.dstNode] = QPointF(doc.object()["x"].toDouble(100),
                                                                 doc.object()["y"].toDouble(100));
                }
            }
        }
        else if (!srcIn && dstIn)
        {
            // Incoming external link: Outside -> Internal
            snap.edges.push_back(e);
            externalProxyNodeIds.insert(e.srcNode);
            if (externalNodeIsOutgoing.find(e.srcNode) == externalNodeIsOutgoing.end())
            {
                externalNodeIsOutgoing[e.srcNode] = false;
            }

            auto dOpt = model_->getNodeById(e.dstNode);
            if (dOpt && !dOpt->metadata.empty())
            {
                auto doc = QJsonDocument::fromJson(QString::fromStdString(dOpt->metadata).toUtf8());
                if (doc.isObject())
                {
                    internalAnchorPositions[e.srcNode] = QPointF(doc.object()["x"].toDouble(100),
                                                                 doc.object()["y"].toDouble(100));
                }
            }
        }
    }

    // Add External Proxy Nodes to the snapshot on the periphery
    int outIdx = 0;
    int inIdx = 0;
    for (NodeId extId : externalProxyNodeIds)
    {
        auto extOpt = model_->getNodeById(extId);
        if (!extOpt) continue;

        NodeData extProxy = *extOpt;
        extProxy.parentId = std::nullopt;

        QJsonObject obj;
        if (!extProxy.metadata.empty())
        {
            auto doc = QJsonDocument::fromJson(QString::fromStdString(extProxy.metadata).toUtf8());
            if (doc.isObject()) obj = doc.object();
        }

        bool isOut = externalNodeIsOutgoing[extId];
        obj["_is_external_proxy"] = true;
        obj["_external_direction"] = isOut ? "outgoing" : "incoming";

        if (obj.contains("_scoped_proxy_x") && obj.contains("_scoped_proxy_y"))
        {
            obj["x"] = obj["_scoped_proxy_x"];
            obj["y"] = obj["_scoped_proxy_y"];
        }
        else
        {
            QPointF anchor = internalAnchorPositions[extId];
            if (isOut)
            {
                obj["x"] = cWidth + 70.0;
                obj["y"] = anchor.y() + (outIdx * 100.0);
                outIdx++;
            }
            else
            {
                obj["x"] = -220.0;
                obj["y"] = anchor.y() + (inIdx * 100.0);
                inIdx++;
            }
        }

        extProxy.metadata = QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
        snap.nodes.push_back(extProxy);
    }

    snap.layers = model_->layers();
    return snap;
}

void MainWindow::refreshActiveGraph()
{
    if (!model_ || !scene_) return;

    if (activeContainerScope_.has_value())
    {
        GraphSnapshot scopedSnap = extractScopedSnapshot(*activeContainerScope_);
        renderGraph(scopedSnap);
        if (scopeBar_)
        {
            int internalNodeCount = 0;
            int internalEdgeCount = 0;
            int externalEdgeCount = 0;
            for (QGraphicsItem* it : scene_->items())
            {
                if (auto* n = dynamic_cast<GraphNodeItem*>(it))
                {
                    if (!n->isExternalProxy()) internalNodeCount++;
                }
                else if (auto* e = dynamic_cast<GraphEdgeItem*>(it))
                {
                    if (e->isExternalLink()) externalEdgeCount++;
                    else internalEdgeCount++;
                }
            }
            scopeBar_->setScopeInfo(internalNodeCount, internalEdgeCount, externalEdgeCount);
        }
    }
    else
    {
        std::optional<LayerId> activeLayerId;
        if (navigator_ && navigator_->currentIndex().isValid() &&
            static_cast<ItemType>(navigator_->currentIndex().data(NavRole::Type).toInt()) == ItemType::Layer)
        {
            activeLayerId = static_cast<LayerId>(navigator_->currentIndex().data(NavRole::Id).toULongLong());
        }

        if (activeLayerId)
        {
            renderGraph(model_->extractGraph(*activeLayerId));
        }
        else
        {
            renderGraph(model_->extractGraph(std::nullopt));
        }
    }
}

void MainWindow::applyContainerScope(std::optional<NodeId> containerId)
{
    if (!scene_ || !model_) return;

    if (!containerId.has_value())
    {
        if (boundaryItem_)
        {
            if (boundaryItem_->scene() == scene_)
            {
                scene_->removeItem(boundaryItem_);
            }
            delete boundaryItem_;
            boundaryItem_ = nullptr;
        }

        if (scopeBar_)
        {
            scopeBar_->clearScope();
        }

        refreshActiveGraph();

        QRectF fullRect = scene_->itemsBoundingRect();
        if (!fullRect.isEmpty() && graphView_)
        {
            graphView_->fitInView(fullRect.adjusted(-80, -80, 80, 80), Qt::KeepAspectRatio);
        }
        statusBar()->showMessage("Exited container isolation mode", 2000);
        return;
    }

    NodeId targetId = *containerId;

    // Read container dimensions & name
    auto targetOpt = model_->getNodeById(targetId);
    QString targetName = targetOpt ? QString::fromStdString(targetOpt->name) : QString("Container #%1").arg(targetId);
    qreal cWidth = 800.0;
    qreal cHeight = 600.0;
    bool isLocked = false;
    if (targetOpt && !targetOpt->metadata.empty())
    {
        auto doc = QJsonDocument::fromJson(QString::fromStdString(targetOpt->metadata).toUtf8());
        if (doc.isObject())
        {
            auto obj = doc.object();
            if (obj.contains("_width")) cWidth = obj["_width"].toDouble(cWidth);
            if (obj.contains("_height")) cHeight = obj["_height"].toDouble(cHeight);
            if (obj.contains("_dim_lock")) isLocked = obj["_dim_lock"].toBool(false);
        }
    }

    // Build breadcrumb trail details for the UI bar
    std::vector<std::pair<NodeId, QString>> trailDisplay;
    for (NodeId id : containerBreadcrumbTrail_)
    {
        auto n = model_->getNodeById(id);
        QString name = n ? QString::fromStdString(n->name) : QString("Container #%1").arg(id);
        trailDisplay.push_back({id, name});
    }

    if (scopeBar_)
    {
        scopeBar_->setBreadcrumbTrail(trailDisplay);
    }

    GraphSnapshot scopedSnap = extractScopedSnapshot(targetId);
    renderGraph(scopedSnap);

    // Compute internal vs external counts
    int internalNodeCount = 0;
    int internalEdgeCount = 0;
    int externalEdgeCount = 0;

    for (QGraphicsItem* it : scene_->items())
    {
        if (auto* n = dynamic_cast<GraphNodeItem*>(it))
        {
            if (!n->isExternalProxy()) internalNodeCount++;
        }
        else if (auto* e = dynamic_cast<GraphEdgeItem*>(it))
        {
            if (e->isExternalLink()) externalEdgeCount++;
            else internalEdgeCount++;
        }
    }

    if (scopeBar_)
    {
        scopeBar_->setScopeInfo(internalNodeCount, internalEdgeCount, externalEdgeCount);
    }

    // Update or create Container Boundary Guideline Item
    if (!boundaryItem_)
    {
        boundaryItem_ = new ContainerBoundaryItem(targetId, targetName, cWidth, cHeight, isLocked);
        scene_->addItem(boundaryItem_);
        connect(boundaryItem_, &ContainerBoundaryItem::dimensionsChanged, this, [this, targetId](qreal newW, qreal newH) {
            if (!model_) return;
            auto n = model_->getNodeById(targetId);
            if (!n) return;
            QJsonObject obj;
            if (!n->metadata.empty()) {
                auto doc = QJsonDocument::fromJson(QString::fromStdString(n->metadata).toUtf8());
                if (doc.isObject()) obj = doc.object();
            }
            obj["_width"] = newW;
            obj["_height"] = newH;
            model_->setNodeMetadata(targetId, QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString());
            setSaveStatus(DbSaveState::Saved, QString("Updated container bounds to %1 × %2 px").arg(static_cast<int>(newW)).arg(static_cast<int>(newH)));
        });
    }
    else
    {
        if (boundaryItem_->scene() != scene_)
        {
            scene_->addItem(boundaryItem_);
        }
        boundaryItem_->setDimensions(cWidth, cHeight, isLocked);
    }

    if (graphView_)
    {
        QRectF bounds = scene_->itemsBoundingRect();
        if (!bounds.isEmpty())
        {
            graphView_->fitInView(bounds.adjusted(-80, -80, 80, 80), Qt::KeepAspectRatio);
        }
    }

    statusBar()->showMessage(QString("Scoped into container: %1 (%2 internal nodes, %3 internal edges, %4 external links)")
        .arg(targetName).arg(internalNodeCount).arg(internalEdgeCount).arg(externalEdgeCount), 3000);
}

void MainWindow::openContainerSizeDialog(NodeId containerId)
{
    if (!model_) return;

    ContainerSizeDialog dlg(model_, containerId, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        setSaveStatus(DbSaveState::Saved, "Updated container dimensions & sizing mode");
        refreshActiveGraph();
        if (activeContainerScope_.has_value() && *activeContainerScope_ == containerId && boundaryItem_)
        {
            auto n = model_->getNodeById(containerId);
            if (n && !n->metadata.empty())
            {
                auto doc = QJsonDocument::fromJson(QString::fromStdString(n->metadata).toUtf8());
                if (doc.isObject())
                {
                    auto obj = doc.object();
                    qreal w = obj.contains("_width") ? obj["_width"].toDouble(800) : 800;
                    qreal h = obj.contains("_height") ? obj["_height"].toDouble(600) : 600;
                    bool lock = obj.contains("_dim_lock") ? obj["_dim_lock"].toBool(false) : false;
                    boundaryItem_->setDimensions(w, h, lock);
                }
            }
        }
    }
}

void MainWindow::openActiveContainerSizeDialog()
{
    if (activeContainerScope_.has_value())
    {
        openContainerSizeDialog(*activeContainerScope_);
    }
}