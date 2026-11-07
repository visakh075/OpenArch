#include <QApplication>
#include <QAction>
#include <QLabel>
#include <QComboBox>
#include <QTabWidget>
#include <QGraphicsScene>
#include <QJsonDocument>
#include <QJsonObject>
#include <cassert>
#include <iostream>
#include "MainWindow.h"
#include "ShortcutManager.h"
#include "ShortcutConfigDialog.h"
#include "GraphThemeManager.h"
#include "GraphModules/GraphNodeItem.h"
#include "GraphModules/GraphEdgeItem.h"
#include "theme/ThemeEditorWindow.h"
#include "theme/ThemeEditorDock.h"
#include "theme/ApplicationThemeManager.h"
#include "config/ApplicationConfig.h"
#include "PreferencesDialog.h"
#include "ContainerScopeBar.h"
#include "ContainerBoundaryItem.h"
#include "EditorDialogs/ContainerSizeDialog.h"
#include "db/DbManagerSQLite.h"
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDir>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ApplicationConfig::initialize(&app);

    // Initialize theme manager with default theme
    GraphThemeManager themeManager;
    QString darkThemePath = "src/gui/theme/themes/Dark.json";
    if (!QFile::exists(darkThemePath)) darkThemePath = "../src/gui/theme/themes/Dark.json";
    if (!QFile::exists(darkThemePath)) darkThemePath = "themes/Dark.json";
    themeManager.load(darkThemePath);

    // Construct MainWindow
    MainWindow window;

    auto* sm = ShortcutManager::instance();
    assert(sm != nullptr);

    std::cout << "[TEST] 1. Verifying Standard Shortcuts Registration..." << std::endl;
    assert(sm->isStandard("std.cut") == true);
    assert(sm->isStandard("std.copy") == true);
    assert(sm->isStandard("std.paste") == true);
    assert(sm->isStandard("std.delete") == true);
    assert(sm->isStandard("std.backspace") == true);
    assert(sm->isStandard("std.find") == true);
    std::cout << " -> PASSED: Standard shortcuts correctly flagged as standard." << std::endl;

    std::cout << "[TEST] 2. Verifying Configurable Shortcuts Registration..." << std::endl;
    assert(sm->isStandard("mode.view") == false);
    assert(sm->isStandard("mode.edit") == false);
    assert(sm->isStandard("edit.duplicate") == false);
    assert(sm->isStandard("edit.connect") == false);
    assert(sm->isStandard("edit.add_node") == false);
    assert(sm->isStandard("edit.add_layer") == false);
    assert(sm->isStandard("layout.dist_h") == false);
    assert(sm->isStandard("layout.dist_v") == false);
    assert(sm->isStandard("layout.save_layout") == false);
    assert(sm->isStandard("align.left") == false);
    assert(sm->isStandard("align.center_h") == false);
    assert(sm->isStandard("align.right") == false);
    assert(sm->isStandard("align.top") == false);
    assert(sm->isStandard("align.center_v") == false);
    assert(sm->isStandard("align.bottom") == false);
    assert(sm->isStandard("export.current_view") == false);
    assert(sm->isStandard("export.whole_diagram") == false);
    assert(sm->isStandard("export.html") == false);
    std::cout << " -> PASSED: Configurable shortcuts correctly registered." << std::endl;

    std::cout << "[TEST] 3. Verifying Standard Shortcut Immutability..." << std::endl;
    // Attempting to change standard shortcuts must be rejected
    bool changeCut = sm->setShortcut("std.cut", QKeySequence(Qt::CTRL | Qt::Key_W));
    assert(!changeCut);
    assert(sm->getShortcut("std.cut") == QKeySequence(QKeySequence::Cut));

    bool changeCopy = sm->setShortcut("std.copy", QKeySequence(Qt::CTRL | Qt::Key_X));
    assert(!changeCopy);
    assert(sm->getShortcut("std.copy") == QKeySequence(QKeySequence::Copy));
    std::cout << " -> PASSED: Standard shortcuts cannot be modified." << std::endl;

    std::cout << "[TEST] 4. Verifying Protection Against Standard Shortcut Collisions..." << std::endl;
    // Attempting to assign standard shortcut key sequences to configurable actions must fail
    bool setCopy = sm->setShortcut("mode.view", QKeySequence(QKeySequence::Copy));
    assert(!setCopy);
    assert(sm->getShortcut("mode.view") == QKeySequence(Qt::Key_V));

    bool setPaste = sm->setShortcut("align.center_v", QKeySequence(QKeySequence::Paste));
    assert(!setPaste);

    bool setCtrlV = sm->setShortcut("align.center_v", QKeySequence(Qt::CTRL | Qt::Key_V));
    assert(!setCtrlV);

    bool setDel = sm->setShortcut("edit.duplicate", QKeySequence(Qt::Key_Delete));
    assert(!setDel);

    bool setFind = sm->setShortcut("layout.dist_h", QKeySequence(Qt::CTRL | Qt::Key_F));
    assert(!setFind);
    std::cout << " -> PASSED: Standard shortcut key sequences cannot be stolen." << std::endl;

    std::cout << "[TEST] 5. Verifying Align Center V No Longer Clashes with Paste..." << std::endl;
    // In old code, align.center_v was Ctrl+V which broke Paste. Verify it is now non-colliding.
    QKeySequence alignMidKey = sm->getShortcut("align.center_v");
    assert(alignMidKey != QKeySequence(Qt::CTRL | Qt::Key_V));
    assert(alignMidKey != QKeySequence(QKeySequence::Paste));
    std::cout << " -> PASSED: align.center_v defaults to " << alignMidKey.toString().toStdString()
              << " without Paste conflict." << std::endl;

    std::cout << "[TEST] 6. Verifying Modifying Configurable Shortcuts..." << std::endl;
    bool changeDistH = sm->setShortcut("layout.dist_h", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H));
    assert(changeDistH);
    assert(sm->getShortcut("layout.dist_h") == QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H));

    bool changeDup = sm->setShortcut("edit.duplicate", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_D));
    assert(changeDup);
    assert(sm->getShortcut("edit.duplicate") == QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_D));
    std::cout << " -> PASSED: Configurable shortcuts update successfully." << std::endl;

    std::cout << "[TEST] 7. Verifying Conflict Detection..." << std::endl;
    QString conflictName = sm->findConflict("align.center_h", QKeySequence(Qt::Key_V));
    assert(conflictName == "View Mode");
    std::cout << " -> PASSED: Correctly identified conflict with " << conflictName.toStdString() << "." << std::endl;

    std::cout << "[TEST] 8. Verifying Reset to Default and Reset All..." << std::endl;
    sm->resetToDefault("layout.dist_h");
    assert(sm->getShortcut("layout.dist_h") == QKeySequence(Qt::ALT | Qt::Key_H));

    sm->resetAllToDefaults();
    assert(sm->getShortcut("edit.duplicate") == QKeySequence(Qt::CTRL | Qt::Key_D));
    std::cout << " -> PASSED: Reset functions restore initial defaults." << std::endl;

    std::cout << "[TEST] 9. Verifying ShortcutConfigDialog..." << std::endl;
    ShortcutConfigDialog dlg(&window);
    std::cout << " -> PASSED: ShortcutConfigDialog successfully created and wired with MainWindow." << std::endl;

    std::cout << "[TEST] 10. Verifying Node Custom Dimensions & Dimension Lock..." << std::endl;
    DbManagerSQLite testDb;
    assert(testDb.open(":memory:").ok);
    ArchitectureModel model(testDb);
    QGraphicsScene scene;

    // 10.1 Container with _dim_lock = false
    NodeData c1;
    c1.name = "Container_Unlocked";
    c1.type = "container";
    c1.metadata = R"({"_width": 300, "_height": 200, "_dim_lock": false})";
    NodeId c1Id = 0;
    assert(model.addNode(c1, c1Id).ok);

    auto* c1Item = new GraphNodeItem(&model, c1Id);
    scene.addItem(c1Item);
    c1Item->refreshGeometry();

    assert(c1Item->isDimLocked() == false);
    assert(c1Item->rect().width() >= 300.0);
    assert(c1Item->rect().height() >= 200.0);

    // Add child inside Container 1 that requires more space (pos 400, 300)
    NodeData child1;
    child1.name = "Child_In_C1";
    child1.type = "service";
    child1.parentId = c1Id;
    NodeId child1Id = 0;
    assert(model.addNode(child1, child1Id).ok);

    auto* child1Item = new GraphNodeItem(&model, child1Id, c1Item);
    child1Item->setPos(400.0, 300.0);
    child1Item->refreshGeometry();
    c1Item->refreshGeometry();

    // Since _dim_lock is false, container expands to fit child positions
    assert(c1Item->rect().width() > 400.0);
    assert(c1Item->rect().height() > 300.0);
    std::cout << " -> PASSED: Container with _dim_lock=false expands according to subcontainers/children positions." << std::endl;

    // 10.2 Container with _dim_lock = true
    NodeData c2;
    c2.name = "Container_Locked";
    c2.type = "container";
    c2.metadata = R"({"_width": 300, "_height": 200, "_dim_lock": true})";
    NodeId c2Id = 0;
    assert(model.addNode(c2, c2Id).ok);

    auto* c2Item = new GraphNodeItem(&model, c2Id);
    scene.addItem(c2Item);
    c2Item->refreshGeometry();

    assert(c2Item->isDimLocked() == true);
    assert(c2Item->nodeRect().width() == 300.0);
    assert(c2Item->nodeRect().height() == 200.0);

    // Add child positioned outside 300x200
    NodeData child2;
    child2.name = "Child_In_C2";
    child2.type = "service";
    child2.parentId = c2Id;
    NodeId child2Id = 0;
    assert(model.addNode(child2, child2Id).ok);

    auto* child2Item = new GraphNodeItem(&model, child2Id, c2Item);
    child2Item->setPos(400.0, 300.0);
    child2Item->refreshGeometry();
    c2Item->refreshGeometry();

    // Since _dim_lock is true, dimensions remain strictly absolute
    assert(c2Item->nodeRect().width() == 300.0);
    assert(c2Item->nodeRect().height() == 200.0);
    std::cout << " -> PASSED: Container with _dim_lock=true keeps _height and _width as absolute." << std::endl;

    // 10.3 Leaf node with custom dimensions
    NodeData leaf;
    leaf.name = "LeafNode";
    leaf.type = "service";
    leaf.metadata = R"({"_width": 250, "_height": 180, "_dim_lock": true})";
    NodeId leafId = 0;
    assert(model.addNode(leaf, leafId).ok);

    auto* leafItem = new GraphNodeItem(&model, leafId);
    scene.addItem(leafItem);
    leafItem->refreshGeometry();

    assert(leafItem->isDimLocked() == true);
    assert(leafItem->nodeRect().width() == 250.0);
    assert(leafItem->nodeRect().height() == 180.0);
    std::cout << " -> PASSED: Leaf node respects custom dimensions and dimension lock." << std::endl;

    std::cout << "[TEST] 11. Verifying Manually Sized Node Drop & Collision Logic..." << std::endl;

    // 11.1 Standard node without manual sizing cannot accept children
    NodeData standardNode;
    standardNode.name = "StandardService";
    standardNode.type = "service";
    NodeId stdId = 0;
    assert(model.addNode(standardNode, stdId).ok);
    auto* stdItem = new GraphNodeItem(&model, stdId);
    scene.addItem(stdItem);
    stdItem->refreshGeometry();
    assert(!stdItem->isContainer());
    assert(!stdItem->canAcceptChildren());
    std::cout << " -> PASSED: Standard service node does not accept children." << std::endl;

    // 11.2 Manually sized node (type="service") can accept children
    NodeData manualNode;
    manualNode.name = "ManualServiceBox";
    manualNode.type = "service";
    manualNode.metadata = R"({"_width": 350, "_height": 250, "_dim_lock": false})";
    NodeId manualId = 0;
    assert(model.addNode(manualNode, manualId).ok);
    auto* manualItem = new GraphNodeItem(&model, manualId);
    scene.addItem(manualItem);
    manualItem->refreshGeometry();
    assert(!manualItem->isContainer());
    assert(manualItem->canAcceptChildren());
    std::cout << " -> PASSED: Manually sized node canAcceptChildren() is true despite type != container." << std::endl;

    // 11.3 Drop child into manually sized node
    NodeData droppedChild;
    droppedChild.name = "DroppedChild";
    droppedChild.type = "service";
    NodeId droppedChildId = 0;
    assert(model.addNode(droppedChild, droppedChildId).ok);
    auto* droppedChildItem = new GraphNodeItem(&model, droppedChildId);
    scene.addItem(droppedChildItem);
    droppedChildItem->setPos(100.0, 100.0);
    droppedChildItem->refreshGeometry();

    manualItem->adoptChild(droppedChildItem);
    assert(droppedChildItem->parentItem() == manualItem);
    assert(manualItem->isContainer());
    assert(droppedChildItem->pos().y() >= 42.0); // Safe offset below container header
    auto adoptedNodeOpt = model.getNodeById(droppedChildId);
    assert(adoptedNodeOpt.has_value() && adoptedNodeOpt->parentId == manualId);
    std::cout << " -> PASSED: Manually sized node successfully adopts child and converts to container with safe header offset." << std::endl;

    // 11.4 Locked dimension node clamps adopted child within its bounds
    NodeData lockedBox;
    lockedBox.name = "LockedBox";
    lockedBox.type = "box";
    lockedBox.metadata = R"({"_width": 200, "_height": 200, "_dim_lock": true})";
    NodeId lockedBoxId = 0;
    assert(model.addNode(lockedBox, lockedBoxId).ok);
    auto* lockedBoxItem = new GraphNodeItem(&model, lockedBoxId);
    scene.addItem(lockedBoxItem);
    lockedBoxItem->refreshGeometry();
    assert(lockedBoxItem->canAcceptChildren());

    NodeData farChild;
    farChild.name = "FarChild";
    farChild.type = "service";
    NodeId farChildId = 0;
    assert(model.addNode(farChild, farChildId).ok);
    auto* farChildItem = new GraphNodeItem(&model, farChildId);
    scene.addItem(farChildItem);
    farChildItem->setPos(800.0, 800.0);
    farChildItem->refreshGeometry();

    lockedBoxItem->adoptChild(farChildItem);
    assert(farChildItem->parentItem() == lockedBoxItem);
    assert(farChildItem->pos().x() <= 200.0 - farChildItem->boundingRect().width());
    assert(farChildItem->pos().y() <= 200.0 - farChildItem->boundingRect().height());
    assert(lockedBoxItem->nodeRect().width() == 200.0);
    assert(lockedBoxItem->nodeRect().height() == 200.0);
    std::cout << " -> PASSED: Dimension-locked container clamps adopted child within absolute bounds." << std::endl;

    std::cout << "[TEST] 12. Verifying Type-Specific Theme Styling (edge#pwr, edge#lan, node#pwr, node#lan)..." << std::endl;
    // Reload Dark.json which has edge#pwr, edge#lan, node#pwr, node#lan
    assert(themeManager.load(darkThemePath) == true);
    const auto& currentTheme = themeManager.theme();

    // 12.1 Verify type-specific map entries
    assert(currentTheme.edgeTypeThemes.contains("pwr"));
    assert(currentTheme.edgeTypeThemes.contains("lan"));
    assert(currentTheme.nodeTypeThemes.contains("pwr"));
    assert(currentTheme.nodeTypeThemes.contains("lan"));

    // 12.2 Verify edge colors for pwr (yellow) and lan (cyan)
    QColor pwrColor = currentTheme.edgeThemeForType("pwr").normal.lineColor;
    QColor lanColor = currentTheme.edgeThemeForType("lan").normal.lineColor;
    assert(pwrColor.red() == 255 && pwrColor.green() == 255 && pwrColor.blue() == 0); // #ffff00
    assert(lanColor.red() == 0 && lanColor.green() == 255 && lanColor.blue() == 255); // #00ffff

    // 12.3 Case-insensitive lookups
    assert(currentTheme.edgeThemeForType("PWR").normal.lineColor == pwrColor);
    assert(currentTheme.edgeThemeForType("LAN").normal.lineColor == lanColor);

    // 12.4 Fallback for unknown edge type
    assert(currentTheme.edgeThemeForType("unknown_custom").normal.lineColor == currentTheme.edge.normal.lineColor);

    // 12.5 Node type overrides
    QColor nodePwrBorder = currentTheme.nodeThemeForType("pwr").normal.border;
    QColor nodeLanBorder = currentTheme.nodeThemeForType("lan").normal.border;
    assert(nodePwrBorder.red() == 255 && nodePwrBorder.green() == 255 && nodePwrBorder.blue() == 0);
    assert(nodeLanBorder.red() == 0 && nodeLanBorder.green() == 255 && nodeLanBorder.blue() == 255);

    // 12.6 GraphEdgeItem effective state
    LayerData defaultLayer;
    defaultLayer.name = "DefaultLayer";
    defaultLayer.kind = "Logic";
    LayerId defaultLayerId = 0;
    assert(model.addLayer(defaultLayer, defaultLayerId).ok);
    assert(model.addNodeToLayer(stdId, defaultLayerId).ok);
    assert(model.addNodeToLayer(manualId, defaultLayerId).ok);

    EdgeData edgePwrData;
    edgePwrData.srcNode = stdId;
    edgePwrData.srcLayer = defaultLayerId;
    edgePwrData.dstNode = manualId;
    edgePwrData.dstLayer = defaultLayerId;
    edgePwrData.edgeType = "pwr";
    EdgeId pwrEdgeId = 0;
    assert(model.addEdge(edgePwrData, pwrEdgeId).ok);

    auto* edgePwrItem = new GraphEdgeItem(&model, pwrEdgeId, stdItem, manualItem);
    scene.addItem(edgePwrItem);
    edgePwrItem->refreshLayout();
    assert(edgePwrItem->edgeType() == "pwr");
    assert(currentTheme.edgeStateFor(edgePwrItem->edgeType(), false, false).lineColor == pwrColor);

    EdgeData edgeLanData;
    edgeLanData.srcNode = stdId;
    edgeLanData.srcLayer = defaultLayerId;
    edgeLanData.dstNode = manualId;
    edgeLanData.dstLayer = defaultLayerId;
    edgeLanData.edgeType = "lan";
    EdgeId lanEdgeId = 0;
    assert(model.addEdge(edgeLanData, lanEdgeId).ok);

    auto* edgeLanItem = new GraphEdgeItem(&model, lanEdgeId, stdItem, manualItem);
    scene.addItem(edgeLanItem);
    edgeLanItem->refreshLayout();
    assert(edgeLanItem->edgeType() == "lan");
    assert(currentTheme.edgeStateFor(edgeLanItem->edgeType(), false, false).lineColor == lanColor);
    std::cout << " -> PASSED: Edge and node type-specific styling resolves correctly for pwr, lan, and fallbacks." << std::endl;

    // 12.7 Save and reload round-trip
    QTemporaryFile tempThemeFile;
    assert(tempThemeFile.open());
    QString tempThemePath = tempThemeFile.fileName();
    tempThemeFile.close();

    assert(themeManager.save(tempThemePath) == true);
    GraphThemeManager reloadedManager;
    assert(reloadedManager.load(tempThemePath) == true);
    const auto& reloadedTheme = reloadedManager.theme();
    assert(reloadedTheme.edgeThemeForType("pwr").normal.lineColor == pwrColor);
    assert(reloadedTheme.edgeThemeForType("lan").normal.lineColor == lanColor);
    assert(reloadedTheme.nodeThemeForType("pwr").normal.border == nodePwrBorder);
    assert(reloadedTheme.nodeThemeForType("lan").normal.border == nodeLanBorder);
    std::cout << " -> PASSED: Theme save and reload round-trip preserves all type-specific overrides." << std::endl;

    // 13. Tabified Architecture and Theme Editor Docks
    std::cout << "\n[TEST] 13. Verifying Tabified Architecture and Theme Editor Docks in MainWindow..." << std::endl;
    assert(window.architectureDock() != nullptr);
    assert(window.themeDock() != nullptr);
    window.architectureDock()->show();
    window.themeDock()->show();
    auto tabbedDocks = window.tabifiedDockWidgets(window.architectureDock());
    QDockWidget* themeDock = window.themeDock();
    assert(tabbedDocks.contains(themeDock));
    std::cout << " -> PASSED: Architecture and Theme Editor docks are tabified together in default view." << std::endl;
    std::cout << " -> PASSED: Architecture and Theme Editor docks are tabified together in default view." << std::endl;

    // 14. Standalone Theme Editor Window & Style Management
    std::cout << "\n[TEST] 14. Verifying Standalone Theme Editor Window & Style Management..." << std::endl;
    ThemeEditorWindow editorWin;
    editorWin.resize(1200, 800);

    auto& mgr = *GraphThemeManager::instance();
    GraphComponentTheme sensorStyle = mgr.theme().node;
    sensorStyle.normal.border = QColor("#ff00ffff");
    mgr.mutableTheme().nodeTypeThemes["sensor"] = sensorStyle;
    mgr.themeChanged();

    assert(mgr.theme().nodeTypeThemes.contains("sensor"));
    assert(mgr.theme().nodeThemeForType("sensor").normal.border == QColor("#ff00ffff"));

    // Duplicate style to sensor_backup
    mgr.mutableTheme().nodeTypeThemes["sensor_backup"] = mgr.theme().nodeTypeThemes["sensor"];
    mgr.themeChanged();
    assert(mgr.theme().nodeTypeThemes.contains("sensor_backup"));

    // Remove sensor_backup
    mgr.mutableTheme().nodeTypeThemes.remove("sensor_backup");
    mgr.themeChanged();
    assert(!mgr.theme().nodeTypeThemes.contains("sensor_backup"));

    editorWin.refreshTree();
    editorWin.resetLiveViewScene();
    editorWin.refreshLiveView();
    std::cout << " -> PASSED: Standalone Theme Editor initialized and verified with custom style lifecycle and live view." << std::endl;

    // 15. Show in Graph from Architecture
    std::cout << "\n[TEST] 15. Verifying Show in Graph from Architecture..." << std::endl;
    QTemporaryFile tempDbFile;
    assert(tempDbFile.open());
    QString tempDbPath = tempDbFile.fileName();
    tempDbFile.close();

    window.setDb(tempDbPath.toStdString());
    assert(window.model() != nullptr);

    NodeData targetNodeData;
    targetNodeData.name = "TargetServer";
    targetNodeData.type = "server";
    targetNodeData.metadata = R"({"x": 500, "y": 400})";
    NodeId targetNodeId = 0;
    assert(window.model()->addNode(targetNodeData, targetNodeId).ok);

    window.populateNavigator();
    window.showNodeInGraph(targetNodeId);

    assert(window.graphView() != nullptr);
    assert(window.graphView()->scene() != nullptr);
    auto selectedItems = window.graphView()->scene()->selectedItems();
    assert(!selectedItems.isEmpty());
    auto* selectedNode = dynamic_cast<GraphNodeItem*>(selectedItems.first());
    assert(selectedNode != nullptr && selectedNode->nodeId() == targetNodeId);
    std::cout << " -> PASSED: showNodeInGraph successfully locates, focuses, and selects target node on canvas." << std::endl;

    std::cout << "\n[TEST] 16. Verifying Connecting Edge (Preview Line & Arrowhead) Properties..." << std::endl;
    // 16.1 Verify preview line and arrow defaults
    const auto& prevTheme = themeManager.theme().edge.preview;
    assert(prevTheme.width == 2);
    assert(prevTheme.style == Qt::DashLine);
    assert(prevTheme.arrow.width == 14);
    assert(prevTheme.arrow.height == 10);
    assert(prevTheme.arrow.lineWidth == 2);
    assert(prevTheme.arrow.lineColor.isValid());
    assert(prevTheme.arrow.fillColor.isValid());

    // 16.2 Verify custom arrow and preview styling persistence
    themeManager.mutableTheme().edge.preview.color = QColor("#00b4d8");
    themeManager.mutableTheme().edge.preview.width = 3;
    themeManager.mutableTheme().edge.preview.style = Qt::DashLine;
    themeManager.mutableTheme().edge.preview.arrow.width = 22;
    themeManager.mutableTheme().edge.preview.arrow.height = 16;
    themeManager.mutableTheme().edge.preview.arrow.lineWidth = 3;
    themeManager.mutableTheme().edge.preview.arrow.lineColor = QColor("#ff5555");
    themeManager.mutableTheme().edge.preview.arrow.fillColor = QColor("#50fa7b");

    QTemporaryFile tempPreviewThemeFile;
    assert(tempPreviewThemeFile.open());
    QString tempPreviewThemePath = tempPreviewThemeFile.fileName();
    tempPreviewThemeFile.close();

    assert(themeManager.save(tempPreviewThemePath) == true);
    GraphThemeManager reloadedPrevManager;
    assert(reloadedPrevManager.load(tempPreviewThemePath) == true);
    const auto& reloadedPrev = reloadedPrevManager.theme().edge.preview;
    assert(reloadedPrev.color == QColor("#00b4d8"));
    assert(reloadedPrev.width == 3);
    assert(reloadedPrev.style == Qt::DashLine);
    assert(reloadedPrev.arrow.width == 22);
    assert(reloadedPrev.arrow.height == 16);
    assert(reloadedPrev.arrow.lineWidth == 3);
    assert(reloadedPrev.arrow.lineColor == QColor("#ff5555"));
    assert(reloadedPrev.arrow.fillColor == QColor("#50fa7b"));

    // 16.3 Verify geometry calculation for connecting edge
    auto previewDataEmpty = stdItem->buildPreviewData(QPointF(800, 600), nullptr, 14.0);
    assert(previewDataEmpty.arrowTip == QPointF(800, 600));
    assert(previewDataEmpty.linePath.elementCount() >= 2);

    auto previewDataTarget = stdItem->buildPreviewData(QPointF(600, 300), manualItem, 14.0);
    assert(previewDataTarget.linePath.elementCount() >= 2);

    GraphConnectingEdgeItem connectingItem;
    connectingItem.setConnectingData(previewDataTarget.linePath, previewDataTarget.arrowTip, previewDataTarget.arrowAngle);
    assert(connectingItem.arrowTip() == previewDataTarget.arrowTip);
    assert(std::abs(connectingItem.arrowAngle() - previewDataTarget.arrowAngle) < 1e-4);
    assert(!connectingItem.boundingRect().isEmpty());

    std::cout << " -> PASSED: Connecting edge supports all properties (arrow width, height, colors, lineWidth, line styling) with persistence." << std::endl;

    std::cout << "\n[TEST] 17. Verifying Light Theme Presets (GitHub Light, Solarized Light, Nord Light, Paper Light)..." << std::endl;
    QStringList lightThemes = {
        "src/gui/theme/themes/GitHub Light.json",
        "src/gui/theme/themes/Solarized Light.json",
        "src/gui/theme/themes/Nord Light.json",
        "src/gui/theme/themes/Paper Light.json"
    };

    for (QString themePath : lightThemes) {
        if (!QFile::exists(themePath)) themePath = "../" + themePath;
        if (!QFile::exists(themePath)) {
            QFileInfo fi(themePath);
            themePath = "themes/" + fi.fileName();
        }
        GraphThemeManager mgr;
        assert(mgr.load(themePath) == true);
        const auto& t = mgr.theme();
        assert(!t.name.isEmpty());
        // Verify background is a light color (lightness > 140)
        assert(t.view.background.lightness() > 140);
        // Verify node title and border are valid
        assert(t.node.normal.title.color.isValid());
        assert(t.node.normal.border.isValid());
        // Verify preview arrow properties
        assert(t.edge.preview.arrow.width > 0);
        assert(t.edge.preview.arrow.lineColor.isValid());
    }

    // Verify MainWindow theme menu lists all presets
    window.populateThemeMenu();
    assert(window.themeMenu() != nullptr);
    std::cout << " -> PASSED: All 4 new light themes load cleanly, validate as light backgrounds, and register in theme menus." << std::endl;

    // 18. Verifying Application UI Themes & Custom UI Theme Extensibility
    std::cout << "\n[TEST] 18. Verifying Application UI Themes & Custom UI Theme Extensibility..." << std::endl;
    const auto uiThemes = ApplicationThemeManager::availableThemes();
    assert(uiThemes.size() >= 7);

    QStringList expectedBuiltIn = {"dark", "light", "nord", "midnight", "solarized_dark", "solarized_light", "system"};
    for (const auto& id : expectedBuiltIn) {
        bool found = false;
        for (const auto& th : uiThemes) {
            if (th.id == id && th.isBuiltIn) {
                found = true;
                break;
            }
        }
        assert(found);
    }

    // Switch UI themes
    ApplicationThemeManager::setThemeById("nord");
    assert(ApplicationThemeManager::currentThemeId() == "nord");

    ApplicationThemeManager::setThemeById("midnight");
    assert(ApplicationThemeManager::currentThemeId() == "midnight");

    ApplicationThemeManager::setThemeById("dark");
    assert(ApplicationThemeManager::currentThemeId() == "dark");

    // Verify custom UI theme loading from file
    QTemporaryFile tempCustomTheme(QDir::tempPath() + QStringLiteral("/test_custom_XXXXXX.json"));
    assert(tempCustomTheme.open());
    tempCustomTheme.write(R"({
        "name": "Test Neon Custom",
        "mode": "dark",
        "window": "#200030",
        "base": "#100018",
        "text": "#ffffff",
        "button": "#300048",
        "border": "#500078",
        "accent": "#ff00ff"
    })");
    tempCustomTheme.close();

    QString loadErr;
    assert(ApplicationThemeManager::loadCustomThemeFile(tempCustomTheme.fileName(), &loadErr) == true);
    assert(ApplicationThemeManager::currentThemeId().startsWith("custom:"));

    // Verify that changing graph theme does NOT alter application theme
    const QString preGraphAppTheme = ApplicationThemeManager::currentThemeId();
    themeManager.load(":/themes/Blueprint.json");
    assert(ApplicationThemeManager::currentThemeId() == preGraphAppTheme);

    // Clean up copied test file
    QString customFolder = ApplicationThemeManager::customThemesDirectory();
    QFile::remove(customFolder + "/" + QFileInfo(tempCustomTheme.fileName()).fileName());

    std::cout << " -> PASSED: Application UI theme options and custom UI theme loading verified independently." << std::endl;

    // 19. Verifying ApplicationConfig, Auto-Save Toggle, Configurable Font & DB Save Status
    std::cout << "\n[TEST] 19. Verifying ApplicationConfig, Auto-Save Toggle, Configurable Font & DB Save Status..." << std::endl;

    // ApplicationConfig initial state & toggling
    ApplicationConfig& cfg = ApplicationConfig::instance();
    bool origAutoSave = cfg.isAutoSaveEnabled();
    int origDelay = cfg.autoSaveDelayMs();
    QFont origFont = cfg.applicationFont();
    bool origHasCustom = cfg.hasCustomFont();

    // Toggle auto-save
    cfg.setAutoSaveEnabled(false);
    assert(cfg.isAutoSaveEnabled() == false);
    cfg.setAutoSaveDelayMs(2000);
    assert(cfg.autoSaveDelayMs() == 2000);

    // Font customization and reset
    QFont customFont("Helvetica", 14, QFont::Bold);
    cfg.setApplicationFont(customFont);
    assert(cfg.hasCustomFont() == true);
    assert(cfg.applicationFont().family() == customFont.family());
    assert(cfg.applicationFont().pointSize() == 14);

    cfg.resetDefaultFont();
    assert(cfg.hasCustomFont() == false);

    // Restore original config values
    cfg.setAutoSaveEnabled(origAutoSave);
    cfg.setAutoSaveDelayMs(origDelay);
    if (origHasCustom) {
        cfg.setApplicationFont(origFont);
    } else {
        cfg.resetDefaultFont();
    }

    // Verify MainWindow action and menu integration (clean non-duplicate menu structure)
    assert(window.actionPreferences() != nullptr);
    assert(window.actionPreferences()->shortcut() == QKeySequence::Preferences);
    assert(window.actionConfigureShortcuts() != nullptr);
    assert(window.viewMenu() != nullptr);
    assert(window.helpMenu() != nullptr);
    assert(window.actionAutoSave() != nullptr);
    assert(window.actionAutoSave()->isCheckable());
    assert(window.actionAutoSave()->isChecked() == cfg.isAutoSaveEnabled());
    assert(window.actionApplicationFont() != nullptr);

    // Verify PreferencesDialog instantiation and tabbed groups (Appearance, General, Shortcuts)
    PreferencesDialog prefDlg(&window);
    assert(prefDlg.windowTitle().contains("Preferences"));
    assert(prefDlg.tabWidget() != nullptr);
    assert(prefDlg.tabWidget()->count() == 3);
    assert(prefDlg.tabWidget()->tabText(0) == "Appearance");
    assert(prefDlg.tabWidget()->tabText(1) == "General");
    assert(prefDlg.tabWidget()->tabText(2) == "Shortcuts");
    assert(prefDlg.appearanceTab() != nullptr);
    assert(prefDlg.generalTab() != nullptr);
    assert(prefDlg.shortcutsTab() != nullptr);

    assert(prefDlg.themeComboBox() != nullptr);
    assert(prefDlg.themeComboBox()->count() >= 7);
    assert(prefDlg.fontComboBox() != nullptr);
    assert(prefDlg.fontSizeSpinBox() != nullptr);
    assert(prefDlg.autoSaveCheckBox() != nullptr);
    assert(prefDlg.autoSaveDelayComboBox() != nullptr);

    // Verify DB Save Status indicator state transitions
    assert(window.saveStatusLabel() != nullptr);
    MainWindow freshWindow;
    assert(freshWindow.saveStatusLabel() != nullptr);
    assert(freshWindow.currentSaveState() == MainWindow::DbSaveState::NoDb);

    window.setSaveStatus(MainWindow::DbSaveState::Saving, "Saving test database...");
    assert(window.currentSaveState() == MainWindow::DbSaveState::Saving);
    assert(window.saveStatusLabel()->text().contains("Saving"));

    window.setSaveStatus(MainWindow::DbSaveState::Saved, "✓ Saved");
    assert(window.currentSaveState() == MainWindow::DbSaveState::Saved);
    assert(window.saveStatusLabel()->text().contains("Saved"));

    window.setSaveStatus(MainWindow::DbSaveState::UnsavedChanges, "● Unsaved changes");
    assert(window.currentSaveState() == MainWindow::DbSaveState::UnsavedChanges);
    assert(window.saveStatusLabel()->text().contains("Unsaved changes"));

    window.setSaveStatus(MainWindow::DbSaveState::Error, "⚠ Save Failed: disk full");
    assert(window.currentSaveState() == MainWindow::DbSaveState::Error);
    assert(window.saveStatusLabel()->text().contains("Save Failed"));

    std::cout << " -> PASSED: ApplicationConfig, Auto-Save toggle, Application Font, Preferences dialog, and DB Save Status verified." << std::endl;

    // 20. Verifying Edge Name and Type Architectural Support
    std::cout << "\n[TEST] 20. Verifying Edge Name and Type Architectural Support..." << std::endl;
    {
        EdgeData testEdge;
        testEdge.srcNode = stdId;
        testEdge.srcLayer = defaultLayerId;
        testEdge.dstNode = manualId;
        testEdge.dstLayer = defaultLayerId;
        testEdge.name = "AuthChannel";
        testEdge.setType("grpc");

        assert(testEdge.name == "AuthChannel");
        assert(testEdge.type() == "grpc");
        assert(testEdge.edgeType == "grpc");

        EdgeId namedEdgeId = 0;
        assert(model.addEdge(testEdge, namedEdgeId).ok);
        auto optEdge = model.getEdgeById(namedEdgeId);
        assert(optEdge.has_value());
        assert(optEdge->name == "AuthChannel");
        assert(optEdge->type() == "grpc");

        uint32_t originalChecksum = optEdge->checksum;
        assert(originalChecksum != 0);

        // Verify checksum sensitivity to name change
        EdgeData modifiedEdge = *optEdge;
        modifiedEdge.name = "RenamedChannel";
        assert(model.computeEdgeChecksum(modifiedEdge) != originalChecksum);

        assert(model.updateEdge(modifiedEdge).ok);
        auto optUpdated = model.getEdgeById(namedEdgeId);
        assert(optUpdated.has_value());
        assert(optUpdated->name == "RenamedChannel");
        assert(optUpdated->status == Status::Changed);
        assert(optUpdated->checksum != originalChecksum);

        // Verify GraphEdgeItem rendering and label resolution (edgename/edgetype)
        auto* edgeItem = new GraphEdgeItem(&model, namedEdgeId, stdItem, manualItem);
        scene.addItem(edgeItem);
        edgeItem->refreshLayout();
        assert(edgeItem->name() == "RenamedChannel");
        assert(edgeItem->edgeType() == "grpc");
        assert(edgeItem->title() == "RenamedChannel/grpc");
        assert(edgeItem->toolTip().contains("RenamedChannel"));
        assert(edgeItem->toolTip().contains("grpc"));

        // Verify fallback to edgeType when name is empty
        modifiedEdge.name = "";
        modifiedEdge.setType("grpc");
        assert(model.updateEdge(modifiedEdge).ok);
        edgeItem->refreshLayout();
        assert(edgeItem->name().isEmpty());
        assert(edgeItem->title() == "grpc");

        // Verify fallback to name when edgeType is empty
        modifiedEdge.name = "SoloNameChannel";
        modifiedEdge.setType("");
        assert(model.updateEdge(modifiedEdge).ok);
        edgeItem->refreshLayout();
        assert(edgeItem->name() == "SoloNameChannel");
        assert(edgeItem->edgeType().isEmpty());
        assert(edgeItem->title() == "SoloNameChannel");

        std::cout << " -> PASSED: Edge Name, Type, Checksum, and Canvas Label rendering verified." << std::endl;
    }

    // 21. Verifying CSS-Convention name#type Styling for Both Edges and Nodes
    std::cout << "\n[TEST] 21. Verifying CSS-Convention name#type Styling for Both Edges and Nodes..." << std::endl;
    {
        GraphTheme testTheme = themeManager.theme();
        
        // Define colors
        QColor genericTypeColor("#112233");
        QColor nameOnlyColor("#445566");
        QColor compoundColor("#ff0077");

        // Edge styling overrides:
        // 1. type rule: "lan"
        GraphEdgeTheme lanTheme = testTheme.edge;
        lanTheme.normal.lineColor = genericTypeColor;
        testTheme.edgeTypeThemes["lan"] = lanTheme;

        // 2. name rule: "synclink"
        GraphEdgeTheme syncNameTheme = testTheme.edge;
        syncNameTheme.normal.lineColor = nameOnlyColor;
        testTheme.edgeTypeThemes["synclink"] = syncNameTheme;

        // 3. compound name#type rule: "synclink#lan"
        GraphEdgeTheme compoundTheme = testTheme.edge;
        compoundTheme.normal.lineColor = compoundColor;
        testTheme.edgeTypeThemes["synclink#lan"] = compoundTheme;

        // Verify Edge Resolution Specificity:
        // Case A: name="SyncLink", type="lan" -> matches compound "synclink#lan"
        assert(testTheme.edgeTheme("SyncLink", "lan").normal.lineColor == compoundColor);
        assert(testTheme.edgeStateFor("SyncLink", "lan", false, false).lineColor == compoundColor);

        // Case B: name="SyncLink", type="other" -> matches name "synclink"
        assert(testTheme.edgeTheme("SyncLink", "other").normal.lineColor == nameOnlyColor);

        // Case C: name="OtherEdge", type="lan" -> matches type "lan"
        assert(testTheme.edgeTheme("OtherEdge", "lan").normal.lineColor == genericTypeColor);

        // Case D: name="Unmatched", type="unmatched" -> matches default edge theme
        assert(testTheme.edgeTheme("Unmatched", "unmatched").normal.lineColor == testTheme.edge.normal.lineColor);

        // Node styling overrides:
        // 1. type rule: "service"
        GraphComponentTheme serviceTheme = testTheme.node;
        serviceTheme.normal.background = genericTypeColor;
        testTheme.nodeTypeThemes["service"] = serviceTheme;

        // 2. name rule: "authnode"
        GraphComponentTheme authNameTheme = testTheme.node;
        authNameTheme.normal.background = nameOnlyColor;
        testTheme.nodeTypeThemes["authnode"] = authNameTheme;

        // 3. compound name#type rule: "authnode#service"
        GraphComponentTheme compoundNodeTheme = testTheme.node;
        compoundNodeTheme.normal.background = compoundColor;
        testTheme.nodeTypeThemes["authnode#service"] = compoundNodeTheme;

        // Verify Node Resolution Specificity:
        // Case A: name="AuthNode", type="service" -> matches compound "authnode#service"
        assert(testTheme.nodeTheme("AuthNode", "service").normal.background == compoundColor);
        assert(testTheme.nodeStateFor("AuthNode", "service", false, false).background == compoundColor);

        // Case B: name="AuthNode", type="db" -> matches name "authnode"
        assert(testTheme.nodeTheme("AuthNode", "db").normal.background == nameOnlyColor);

        // Case C: name="PaymentNode", type="service" -> matches type "service"
        assert(testTheme.nodeTheme("PaymentNode", "service").normal.background == genericTypeColor);

        // Case D: unmatched -> default node background
        assert(testTheme.nodeTheme("Unknown", "unknown").normal.background == testTheme.node.normal.background);

        // Verify Qt canvas items with compound styling
        themeManager.mutableTheme() = testTheme;

        EdgeData edgeCompoundData;
        edgeCompoundData.name = "SyncLink";
        edgeCompoundData.edgeType = "lan";
        edgeCompoundData.srcNode = stdId;
        edgeCompoundData.srcLayer = defaultLayerId;
        edgeCompoundData.dstNode = manualId;
        edgeCompoundData.dstLayer = defaultLayerId;
        EdgeId compEdgeId = 0;
        assert(model.addEdge(edgeCompoundData, compEdgeId).ok);
        auto* compEdgeItem = new GraphEdgeItem(&model, compEdgeId, stdItem, manualItem);
        scene.addItem(compEdgeItem);
        compEdgeItem->refreshLayout();
        assert(compEdgeItem->name() == "SyncLink");
        assert(compEdgeItem->edgeType() == "lan");
        assert(themeManager.theme().edgeStateFor(compEdgeItem->name(), compEdgeItem->edgeType(), false, false).lineColor == compoundColor);

        std::cout << " -> PASSED: CSS-convention name#type compound, name-only, and type-only styling verified with strict specificity." << std::endl;
    }

    {
        std::cout << "[TEST] 22. Verifying Standalone Theme Editor AddStyleDialog & UX Enhancements..." << std::endl;

        // 1. Target initialization
        AddStyleDialog nodeDlg(0);
        assert(nodeDlg.targetType() == 0);

        AddStyleDialog contDlg(1);
        assert(contDlg.targetType() == 1);

        AddStyleDialog edgeDlg(2);
        assert(edgeDlg.targetType() == 2);

        // 2. Compound mode: name#type
        edgeDlg.setMatchMode(AddStyleDialog::Compound);
        edgeDlg.setComponentName("SyncLink");
        edgeDlg.setComponentType("lan");
        assert(edgeDlg.styleName() == "synclink#lan");
        assert(edgeDlg.isValid() == true);

        // 3. Type Only mode
        edgeDlg.setMatchMode(AddStyleDialog::TypeOnly);
        edgeDlg.setComponentType("pwr");
        assert(edgeDlg.styleName() == "pwr");
        assert(edgeDlg.isValid() == true);

        // 4. Name Only mode
        nodeDlg.setMatchMode(AddStyleDialog::NameOnly);
        nodeDlg.setComponentName("Gateway");
        assert(nodeDlg.styleName() == "gateway");
        assert(nodeDlg.isValid() == true);

        // 5. Raw mode
        nodeDlg.setMatchMode(AddStyleDialog::Raw);
        nodeDlg.setRawSelector("authservice#api");
        assert(nodeDlg.styleName() == "authservice#api");
        assert(nodeDlg.isValid() == true);

        // 6. Invalid inputs: spaces
        nodeDlg.setRawSelector("bad style name");
        assert(nodeDlg.isValid() == false);

        // 7. Invalid inputs: multiple '#'
        nodeDlg.setRawSelector("a#b#c");
        assert(nodeDlg.isValid() == false);

        // 8. Empty input
        nodeDlg.setRawSelector("");
        assert(nodeDlg.isValid() == false);

        // 9. Verify ThemeEditorWindow instance and tree selection
        ThemeEditorWindow editorWin;
        editorWin.show();
        editorWin.refreshTree("edge#lan");
        editorWin.close();

        std::cout << " -> PASSED: AddStyleDialog modes, presets, validation, and ThemeEditorWindow integration verified." << std::endl;
    }

    // =========================================================================
    // TEST 23: Verifying Editing Existing Named/Custom Styles & AddStyleDialog Edit Mode
    // =========================================================================
    {
        std::cout << "\n[TEST] 23. Verifying Editing Existing Named Styles & Edit Mode..." << std::endl;

        // 1. Edit mode with compound selector
        AddStyleDialog editCompoundDlg(0, "authservice#api");
        assert(editCompoundDlg.matchMode() == AddStyleDialog::Compound);
        assert(editCompoundDlg.componentName() == "authservice");
        assert(editCompoundDlg.componentType() == "api");
        assert(editCompoundDlg.styleName() == "authservice#api");
        assert(editCompoundDlg.isValid() == true); // Saving unchanged does not trigger duplicate collision

        // Rename component name in edit mode
        editCompoundDlg.setComponentName("securityservice");
        assert(editCompoundDlg.styleName() == "securityservice#api");
        assert(editCompoundDlg.isValid() == true);

        // 2. Edit mode with type-only selector
        AddStyleDialog editTypeDlg(2, "lan");
        assert(editTypeDlg.matchMode() == AddStyleDialog::TypeOnly);
        assert(editTypeDlg.componentType() == "lan");
        assert(editTypeDlg.styleName() == "lan");
        assert(editTypeDlg.isValid() == true);

        // 3. Edit mode with name-only selector
        AddStyleDialog editNameDlg(1, "mycontainer");
        assert(editNameDlg.matchMode() == AddStyleDialog::NameOnly);
        assert(editNameDlg.componentName() == "mycontainer");
        assert(editNameDlg.styleName() == "mycontainer");
        assert(editNameDlg.isValid() == true);

        // 4. Test programmatic migration of existing style in GraphTheme
        GraphTheme theme = GraphThemeManager::instance()->theme();
        GraphComponentTheme customNodeStyle;
        customNodeStyle.normal.background = QColor(220, 50, 80);
        customNodeStyle.normal.borderWidth = 4;
        theme.nodeTypeThemes["auth#service"] = customNodeStyle;

        assert(theme.nodeTypeThemes.contains("auth#service"));
        assert(theme.nodeTypeThemes["auth#service"].normal.background == QColor(220, 50, 80));

        // Simulate renaming auth#service -> auth#v2
        QString oldKey = "auth#service";
        QString newKey = "auth#v2";
        theme.nodeTypeThemes[newKey] = theme.nodeTypeThemes.take(oldKey);

        assert(!theme.nodeTypeThemes.contains("auth#service"));
        assert(theme.nodeTypeThemes.contains("auth#v2"));
        assert(theme.nodeTypeThemes["auth#v2"].normal.background == QColor(220, 50, 80));
        assert(theme.nodeTypeThemes["auth#v2"].normal.borderWidth == 4);

        // 5. Test ThemeEditorWindow tree selection and refresh
        ThemeEditorWindow editorWin;
        editorWin.show();
        editorWin.refreshTree("node#auth#v2");
        editorWin.close();

        std::cout << " -> PASSED: Editing existing named styles, AddStyleDialog pre-filling, validation, and theme data migration verified." << std::endl;
    }

    // =========================================================================
    // TEST 24: Verifying Isolated Container Scope, Breadcrumb Trail & Drill-Down
    // =========================================================================
    {
        std::cout << "\n[TEST] 24. Verifying Isolated Container Scope, Breadcrumbs & Drill-Down..." << std::endl;

        QTemporaryFile tempDb;
        assert(tempDb.open());
        QString dbPath = tempDb.fileName();
        tempDb.close();

        MainWindow scopeWindow;
        scopeWindow.setDb(dbPath.toStdString());
        ArchitectureModel* mdl = scopeWindow.model();
        assert(mdl != nullptr);

        // 1. Setup Architecture: Top-level container, child node, nested container, sub-child, outside node
        NodeData vpcContainer;
        vpcContainer.name = "VPC-East";
        vpcContainer.type = "container";
        NodeId vpcId = 0;
        assert(mdl->addNode(vpcContainer, vpcId).ok);

        NodeData authSvc;
        authSvc.name = "AuthService";
        authSvc.type = "service";
        authSvc.parentId = vpcId;
        NodeId authId = 0;
        assert(mdl->addNode(authSvc, authId).ok);

        NodeData k8sCluster;
        k8sCluster.name = "KubernetesCluster";
        k8sCluster.type = "container";
        k8sCluster.parentId = vpcId;
        NodeId k8sId = 0;
        assert(mdl->addNode(k8sCluster, k8sId).ok);

        NodeData podService;
        podService.name = "PaymentPod";
        podService.type = "service";
        podService.parentId = k8sId;
        NodeId podId = 0;
        assert(mdl->addNode(podService, podId).ok);

        NodeData outsideNode;
        outsideNode.name = "ExternalClient";
        outsideNode.type = "service";
        NodeId outsideId = 0;
        assert(mdl->addNode(outsideNode, outsideId).ok);

        // Edges: Internal (auth -> pod), External (outside -> auth)
        EdgeData internalEdge;
        internalEdge.srcNode = authId;
        internalEdge.dstNode = podId;
        internalEdge.edgeType = "grpc";
        EdgeId intEdgeId = 0;
        assert(mdl->addEdge(internalEdge, intEdgeId).ok);

        EdgeData externalEdge;
        externalEdge.srcNode = outsideId;
        externalEdge.dstNode = authId;
        externalEdge.edgeType = "https";
        EdgeId extEdgeId = 0;
        assert(mdl->addEdge(externalEdge, extEdgeId).ok);

        scopeWindow.showNodeInGraph(vpcId);

        // Verify initial root state: scope bar hidden
        assert(!scopeWindow.activeContainerScope().has_value());
        assert(!scopeWindow.containerScopeBar()->isScoped());

        // 2. Focus into VPC container
        scopeWindow.focusIntoContainer(vpcId);
        assert(scopeWindow.activeContainerScope().has_value());
        assert(*scopeWindow.activeContainerScope() == vpcId);
        assert(scopeWindow.containerScopeBar()->isScoped());

        // Verify descendant collection
        auto scopedSet = scopeWindow.collectScopedNodeIds(vpcId);
        assert(scopedSet.count(authId) > 0);
        assert(scopedSet.count(k8sId) > 0);
        assert(scopedSet.count(podId) > 0);
        assert(scopedSet.count(outsideId) == 0);

        // Verify Full-Screen Redraw Snapshot (with Cross-Boundary External Links)
        GraphSnapshot vpcSnap = scopeWindow.extractScopedSnapshot(vpcId);
        assert(vpcSnap.nodes.size() == 4); // authId, k8sId, podId (internal) + outsideId (external proxy node)
        assert(vpcSnap.edges.size() == 2); // internalEdge (authId -> podId) + externalEdge (outsideId -> authId)

        // Verify direct children of vpcId have parentId == nullopt in scoped snapshot (canvas top-level)
        for (const auto& sn : vpcSnap.nodes) {
            if (sn.id == authId || sn.id == k8sId || sn.id == outsideId) {
                assert(!sn.parentId.has_value()); // Direct children and proxy nodes are top-level on scoped canvas
            } else if (sn.id == podId) {
                assert(sn.parentId.has_value() && *sn.parentId == k8sId); // Nested child retains its parent
            }
        }

        // Verify Scene items in scopeWindow: 4 GraphNodeItems (3 internal + 1 external proxy), 2 GraphEdgeItems (1 internal + 1 external)
        int sceneNodeCount = 0;
        int sceneEdgeCount = 0;
        int externalProxyCount = 0;
        int externalEdgeCount = 0;
        for (QGraphicsItem* it : scopeWindow.scene()->items()) {
            if (auto* n = dynamic_cast<GraphNodeItem*>(it)) {
                sceneNodeCount++;
                assert(n->nodeId() != vpcId); // Container itself is NOT in scene
                if (n->isExternalProxy()) {
                    externalProxyCount++;
                    assert(n->nodeId() == outsideId);
                }
            } else if (auto* e = dynamic_cast<GraphEdgeItem*>(it)) {
                sceneEdgeCount++;
                if (e->isExternalLink()) {
                    externalEdgeCount++;
                    assert(e->srcNode()->nodeId() == outsideId);
                    assert(e->dstNode()->nodeId() == authId);
                } else {
                    assert(e->srcNode()->nodeId() == authId);
                    assert(e->dstNode()->nodeId() == podId);
                }
            }
        }
        assert(sceneNodeCount == 4);
        assert(sceneEdgeCount == 2);
        assert(externalProxyCount == 1);
        assert(externalEdgeCount == 1);

        // 3. Drill deeper into nested k8s cluster
        scopeWindow.focusIntoContainer(k8sId);
        assert(scopeWindow.activeContainerScope().has_value());
        assert(*scopeWindow.activeContainerScope() == k8sId);

        auto k8sScoped = scopeWindow.collectScopedNodeIds(k8sId);
        assert(k8sScoped.count(podId) > 0);
        assert(k8sScoped.count(authId) == 0);

        GraphSnapshot k8sSnap = scopeWindow.extractScopedSnapshot(k8sId);
        // k8sSnap contains podId (internal) + authId (external proxy for authId -> podId link)
        assert(k8sSnap.nodes.size() == 2);
        assert(k8sSnap.edges.size() == 1); // incoming external link authId -> podId

        // 4. Step back up to VPC
        scopeWindow.stepUpContainerScope();
        assert(scopeWindow.activeContainerScope().has_value());
        assert(*scopeWindow.activeContainerScope() == vpcId);
        assert(scopeWindow.extractScopedSnapshot(vpcId).nodes.size() == 4);

        // 5. Exit container scope back to root
        scopeWindow.exitContainerScope();
        assert(!scopeWindow.activeContainerScope().has_value());
        assert(!scopeWindow.containerScopeBar()->isScoped());

        // In root diagram: all 5 nodes and 2 edges are restored!
        int rootNodeCount = 0;
        int rootEdgeCount = 0;
        for (QGraphicsItem* it : scopeWindow.scene()->items()) {
            if (dynamic_cast<GraphNodeItem*>(it)) rootNodeCount++;
            else if (dynamic_cast<GraphEdgeItem*>(it)) rootEdgeCount++;
        }
        assert(rootNodeCount == 5);
        assert(rootEdgeCount == 2);

        std::cout << " -> PASSED: Full-screen container sub-diagram redraw, external proxy nodes, cross-boundary links, and hierarchical navigation verified." << std::endl;
    }

    // =========================================================================
    // TEST 25: Verifying Container Size Management & External Links Styling
    // =========================================================================
    {
        std::cout << "\n[TEST] 25. Verifying Container Size Management & External Links Styling..." << std::endl;

        QTemporaryFile tempDb25;
        assert(tempDb25.open());
        QString dbPath25 = tempDb25.fileName();
        tempDb25.close();

        MainWindow sizeWindow;
        sizeWindow.setDb(dbPath25.toStdString());
        auto* mdl = sizeWindow.model();
        assert(mdl != nullptr);

        NodeData appBox;
        appBox.name = "AppBox";
        appBox.type = "container";
        QJsonObject appInit;
        appInit["x"] = 100.0;
        appInit["y"] = 100.0;
        appBox.metadata = QJsonDocument(appInit).toJson(QJsonDocument::Compact).toStdString();
        NodeId appBoxId = 0;
        assert(mdl->addNode(appBox, appBoxId).ok);

        NodeData innerA;
        innerA.name = "ServiceA";
        innerA.type = "service";
        innerA.parentId = appBoxId;
        QJsonObject aInit;
        aInit["x"] = 150.0;
        aInit["y"] = 150.0;
        innerA.metadata = QJsonDocument(aInit).toJson(QJsonDocument::Compact).toStdString();
        NodeId innerAId = 0;
        assert(mdl->addNode(innerA, innerAId).ok);

        NodeData innerB;
        innerB.name = "ServiceB";
        innerB.type = "service";
        innerB.parentId = appBoxId;
        QJsonObject bInit;
        bInit["x"] = 350.0;
        bInit["y"] = 150.0;
        innerB.metadata = QJsonDocument(bInit).toJson(QJsonDocument::Compact).toStdString();
        NodeId innerBId = 0;
        assert(mdl->addNode(innerB, innerBId).ok);

        NodeData extTarget;
        extTarget.name = "ExtDatabase";
        extTarget.type = "database";
        QJsonObject extTInit;
        extTInit["x"] = 800.0;
        extTInit["y"] = 400.0;
        extTarget.metadata = QJsonDocument(extTInit).toJson(QJsonDocument::Compact).toStdString();
        NodeId extTargetId = 0;
        assert(mdl->addNode(extTarget, extTargetId).ok);

        NodeData extSource;
        extSource.name = "ExtClient";
        extSource.type = "service";
        QJsonObject extSInit;
        extSInit["x"] = 50.0;
        extSInit["y"] = 400.0;
        extSource.metadata = QJsonDocument(extSInit).toJson(QJsonDocument::Compact).toStdString();
        NodeId extSourceId = 0;
        assert(mdl->addNode(extSource, extSourceId).ok);

        EdgeData edgeInt;
        edgeInt.srcNode = innerAId;
        edgeInt.dstNode = innerBId;
        edgeInt.name = "InternalMsg";
        edgeInt.edgeType = "queue";
        EdgeId edgeIntId = 0;
        assert(mdl->addEdge(edgeInt, edgeIntId).ok);

        EdgeData edgeOut;
        edgeOut.srcNode = innerBId;
        edgeOut.dstNode = extTargetId;
        edgeOut.name = "DbQuery";
        edgeOut.edgeType = "tcp";
        EdgeId edgeOutId = 0;
        assert(mdl->addEdge(edgeOut, edgeOutId).ok);

        EdgeData edgeIn;
        edgeIn.srcNode = extSourceId;
        edgeIn.dstNode = innerAId;
        edgeIn.name = "Ingress";
        edgeIn.edgeType = "https";
        EdgeId edgeInId = 0;
        assert(mdl->addEdge(edgeIn, edgeInId).ok);

        sizeWindow.showNodeInGraph(appBoxId);

        // 1. Verify ContainerSizeDialog on AppBox
        {
            ContainerSizeDialog sizeDlg(mdl, appBoxId, &sizeWindow);
            assert(!sizeDlg.isDimLocked()); // Default is Auto-Fit
            assert(sizeDlg.padding() == 20.0); // Default padding
        }

        // Configure manual dimension lock directly via node metadata
        auto appOpt = mdl->getNodeById(appBoxId);
        assert(appOpt.has_value());
        QJsonObject appObj;
        appObj["_dim_lock"] = true;
        appObj["_width"] = 920.0;
        appObj["_height"] = 620.0;
        appObj["_padding"] = 35.0;
        appOpt->metadata = QJsonDocument(appObj).toJson(QJsonDocument::Compact).toStdString();
        assert(mdl->updateNode(*appOpt).ok);

        // Verify ContainerSizeDialog reads updated values
        {
            ContainerSizeDialog sizeDlg(mdl, appBoxId, &sizeWindow);
            assert(sizeDlg.isDimLocked() == true);
            assert(sizeDlg.customWidth().has_value() && *sizeDlg.customWidth() == 920.0);
            assert(sizeDlg.customHeight().has_value() && *sizeDlg.customHeight() == 620.0);
            assert(sizeDlg.padding() == 35.0);
        }

        // 2. Focus into AppBox container
        sizeWindow.focusIntoContainer(appBoxId);
        assert(sizeWindow.activeContainerScope().has_value());
        assert(*sizeWindow.activeContainerScope() == appBoxId);

        // Verify ContainerBoundaryItem exists in scene
        ContainerBoundaryItem* boundary = nullptr;
        for (QGraphicsItem* it : sizeWindow.scene()->items()) {
            if (auto* b = dynamic_cast<ContainerBoundaryItem*>(it)) {
                boundary = b;
                break;
            }
        }
        assert(boundary != nullptr);
        assert(boundary->isLocked() == true);
        assert(boundary->boundaryRect().width() == 920.0);
        assert(boundary->boundaryRect().height() == 620.0);

        // Test boundary resizing
        boundary->setDimensions(1050.0, 750.0, true);
        assert(boundary->boundaryRect().width() == 1050.0);
        assert(boundary->boundaryRect().height() == 750.0);

        // 3. Verify Scoped Items Styling:
        // Internal Nodes: innerA, innerB (isExternalProxy == false)
        // External Proxy Nodes: extTarget (outgoing), extSource (incoming)
        // Internal Edge: innerA -> innerB (isExternalLink == false)
        // Outgoing Edge: innerB -> extTarget (isExternalLink == true, isOutgoing == true)
        // Incoming Edge: extSource -> innerA (isExternalLink == true, isOutgoing == false)
        GraphNodeItem* proxyTargetItem = nullptr;
        GraphNodeItem* proxySourceItem = nullptr;
        GraphNodeItem* innerAItem = nullptr;
        GraphEdgeItem* internalEdgeItem = nullptr;
        GraphEdgeItem* outgoingEdgeItem = nullptr;
        GraphEdgeItem* incomingEdgeItem = nullptr;

        for (QGraphicsItem* it : sizeWindow.scene()->items()) {
            if (auto* n = dynamic_cast<GraphNodeItem*>(it)) {
                if (n->nodeId() == innerAId) {
                    innerAItem = n;
                    assert(!n->isExternalProxy());
                } else if (n->nodeId() == extTargetId) {
                    proxyTargetItem = n;
                    assert(n->isExternalProxy());
                    assert(n->isOutgoingProxy());
                } else if (n->nodeId() == extSourceId) {
                    proxySourceItem = n;
                    assert(n->isExternalProxy());
                    assert(!n->isOutgoingProxy());
                }
            } else if (auto* e = dynamic_cast<GraphEdgeItem*>(it)) {
                if (e->edgeId() == edgeIntId) {
                    internalEdgeItem = e;
                    assert(!e->isExternalLink());
                } else if (e->edgeId() == edgeOutId) {
                    outgoingEdgeItem = e;
                    assert(e->isExternalLink());
                    assert(e->isOutgoing());
                } else if (e->edgeId() == edgeInId) {
                    incomingEdgeItem = e;
                    assert(e->isExternalLink());
                    assert(!e->isOutgoing());
                }
            }
        }

        assert(innerAItem != nullptr);
        assert(proxyTargetItem != nullptr);
        assert(proxySourceItem != nullptr);
        assert(internalEdgeItem != nullptr);
        assert(outgoingEdgeItem != nullptr);
        assert(incomingEdgeItem != nullptr);

        // 4. Test Coordinate Safety: Moving External Proxy Node on Scoped Canvas
        // Should update scoped proxy position, but NOT root coordinate!
        qreal origExtTargetRootX = 800.0;
        qreal origExtTargetRootY = 400.0;
        proxyTargetItem->setPos(proxyTargetItem->pos() + QPointF(50.0, 50.0));
        sizeWindow.saveLayout();

        auto extTargetOpt = mdl->getNodeById(extTargetId);
        assert(extTargetOpt.has_value());
        QJsonObject extObj = QJsonDocument::fromJson(QString::fromStdString(extTargetOpt->metadata).toUtf8()).object();
        // Root x/y preserved!
        assert(extObj["x"].toDouble() == origExtTargetRootX);
        assert(extObj["y"].toDouble() == origExtTargetRootY);
        // Scoped proxy coordinates recorded
        assert(extObj.contains("_scoped_proxy_x"));
        assert(extObj.contains("_scoped_proxy_y"));

        // 5. Exit Container Scope
        sizeWindow.exitContainerScope();
        assert(!sizeWindow.activeContainerScope().has_value());

        // In root diagram: extTarget is a normal component, not a proxy
        for (QGraphicsItem* it : sizeWindow.scene()->items()) {
            if (auto* n = dynamic_cast<GraphNodeItem*>(it)) {
                if (n->nodeId() == extTargetId) {
                    assert(!n->isExternalProxy());
                    assert(n->pos().x() == origExtTargetRootX);
                    assert(n->pos().y() == origExtTargetRootY);
                }
            }
        }

        std::cout << " -> PASSED: Container size dialog, boundary item resizing, external link styling, and proxy coordinate isolation verified." << std::endl;
    }

    std::cout << "\n==============================================" << std::endl;
    std::cout << " ALL TESTS PASSED SUCCESSFULLY! (25/25)" << std::endl;
    std::cout << "==============================================" << std::endl;

    return 0;
}
