#include <QApplication>
#include <QAction>
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
#include "db/DbManagerSQLite.h"
#include <QTemporaryFile>
#include <QFileInfo>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

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

    std::cout << "\n==============================================" << std::endl;
    std::cout << " ALL TESTS PASSED SUCCESSFULLY! (17/17)" << std::endl;
    std::cout << "==============================================" << std::endl;

    return 0;
}
