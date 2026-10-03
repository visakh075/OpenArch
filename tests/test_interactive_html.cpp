#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStringList>
#include <QTextStream>
#include <iostream>
#include <cassert>

#include "MainWindow.h"
#include "GraphThemeManager.h"
#include "GraphModules/GraphEdgeItem.h"

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "========================================\n";
    std::cout << " STARTING INTERACTIVE HTML EXPORT TESTS \n";
    std::cout << "========================================\n";

    // 1. Initialize Theme
    GraphThemeManager themeManager;
    themeManager.load("src/gui/theme/themes/Dark.json");

    // 2. Initialize MainWindow and load existing test architecture
    MainWindow window;

    QString dbPath;
    QStringList candidates = {
        "build/architecture.json",
        "../build/architecture.json",
        "architecture.json",
        "../architecture.json",
        "testdbx.db",
        "../testdbx.db",
        "/workspace/OpenArch/build/architecture.json",
        "/workspace/OpenArch/testdbx.db"
    };
    for (const auto& c : candidates) {
        if (QFile::exists(c)) {
            dbPath = c;
            break;
        }
    }

    bool createdTempJson = false;
    QString tempJsonPath = "test_sample_architecture.json";
    if (dbPath.isEmpty()) {
        dbPath = tempJsonPath;
        createdTempJson = true;
        QFile sampleFile(dbPath);
        if (sampleFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream out(&sampleFile);
            out << R"({
  "layers": [
    {
      "id": 1,
      "name": "Backend Layer",
      "kind": "Services",
      "attributes": "{}",
      "checksum": 100,
      "status": "approved",
      "reviewer": "Security"
    }
  ],
  "nodes": [
    {
      "id": 1,
      "name": "Cluster Container",
      "type": "Container",
      "parent_id": null,
      "attributes": "{\"env\": \"prod\"}",
      "checksum": 101,
      "status": "approved",
      "reviewer": "DevOps"
    },
    {
      "id": 2,
      "name": "Auth Service",
      "type": "Microservice",
      "parent_id": 1,
      "attributes": "{\"port\": 8080}",
      "checksum": 102,
      "status": "changed",
      "reviewer": "Alice"
    },
    {
      "id": 3,
      "name": "API Gateway",
      "type": "Gateway",
      "parent_id": null,
      "attributes": "{\"protocol\": \"HTTPS\"}",
      "checksum": 103,
      "status": "approved",
      "reviewer": "Bob"
    }
  ],
  "node_layers": [
    { "node_id": 1, "layer_id": 1 },
    { "node_id": 2, "layer_id": 1 },
    { "node_id": 3, "layer_id": 1 }
  ],
  "edges": [
    {
      "id": 1,
      "src_node_id": 3,
      "src_layer_id": 1,
      "dst_node_id": 2,
      "dst_layer_id": 1,
      "edge_type": "Route",
      "attributes": "{\"timeout\": 30}",
      "checksum": 104,
      "status": "approved",
      "reviewer": "Alice"
    }
  ],
  "layout": {
    "nodes": {
      "1": "{\"x\": 350.0, \"y\": 100.0, \"w\": 260.0, \"h\": 180.0}",
      "2": "{\"x\": 380.0, \"y\": 160.0, \"w\": 140.0, \"h\": 60.0}",
      "3": "{\"x\": 50.0, \"y\": 150.0, \"w\": 140.0, \"h\": 60.0}"
    },
    "layers": {},
    "edges": {}
  }
})";
            sampleFile.close();
        }
    }

    window.setDb(dbPath.toStdString());

    // 3. Export to Interactive HTML
    QString testOutPath = "test_interactive_export.html";
    window.exportToInteractiveHtml(testOutPath);

    std::cout << "[TEST] 1. Verifying HTML File Creation...\n";
    QFile file(testOutPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        std::cerr << "FAIL: Could not open generated HTML file!\n";
        return 1;
    }
    QString html = QTextStream(&file).readAll();
    file.close();
    assert(!html.isEmpty() && "Generated HTML is empty!");
    std::cout << " -> PASSED: Generated HTML file (" << html.size() << " bytes)\n";

    // 4. Test Edge Hit Detection Path & Clickability
    std::cout << "[TEST] 2. Verifying Edge Hitbox & Hover Structure...\n";
    assert(html.contains("<path class=\"edge-hit\"") && "Missing edge hit hitbox path!");
    assert(html.contains("class=\"edge-hit\"") && "Missing edge-hit class!");
    assert(html.contains("stroke-width: 20px") && "Missing 20px hit target width in CSS!");
    assert(html.contains("pointer-events: stroke") && "Missing pointer-events on hit area!");
    assert(!html.contains("data-kind=\"node\"\"") && "Found malformed double quote in node data-kind!");
    assert(!html.contains("data-kind=\"edge\"\"") && "Found malformed double quote in edge data-kind!");
    assert(!html.contains("data-kind=\"container\"\"") && "Found malformed double quote in container data-kind!");
    assert(!html.contains("mouseenter") && "Found invalid mouseenter listener mutating DOM!");
    std::cout << " -> PASSED: Invisible wide hitbox path with pointer-events stroke and clean attributes verified.\n";

    // 5. Test Edge Line and Arrow Styling
    std::cout << "[TEST] 3. Verifying Clean Edge Styling (No blurry drop-shadow)...\n";
    assert(!html.contains("filter: drop-shadow") && "Found invalid fuzzy drop-shadow on edge!");
    assert(html.contains("stroke-linecap: round") && "Missing rounded line caps on edge!");
    assert(html.contains("stroke-linejoin: round") && "Missing rounded line joins on edge!");
    assert(html.contains("<polygon class=\"arrow\"") && "Missing arrow polygon!");
    std::cout << " -> PASSED: Crisp rounded line styling and arrowhead polygon verified.\n";

    // 6. Test Edge Label Rotation and Geometry
    std::cout << "[TEST] 4. Verifying Edge Label Orientation & Slope Alignment...\n";
    assert(html.contains("<g class=\"edge-label\" transform=\"translate(") && "Missing translated edge label group!");
    assert(html.contains("rotate(") && "Missing rotated label group along edge slope!");
    assert(html.contains("<rect class=\"label-bg\"") && "Missing label background badge!");
    assert(html.contains("class=\"graph-edge\"") && "Missing graph edge elements!");
    std::cout << " -> PASSED: Rotated badge layout matching Qt View slope verified.\n";

    // 7. Test Layer Ordering
    std::cout << "[TEST] 5. Verifying Stacking Layer Ordering...\n";
    int contIdx = html.indexOf("id=\"containers-layer\"");
    int edgeIdx = html.indexOf("id=\"edges-layer\"");
    int nodeIdx = html.indexOf("id=\"nodes-layer\"");
    assert(contIdx > 0 && edgeIdx > contIdx && nodeIdx > edgeIdx && "Incorrect SVG layer ordering!");
    std::cout << " -> PASSED: Correct layer hierarchy (containers -> edges -> nodes) verified.\n";

    // 8. Test Canvas Background Grid
    std::cout << "[TEST] 6. Verifying Background Grid Pattern...\n";
    assert(html.contains("<pattern id=\"canvas-grid\"") && "Missing canvas grid pattern!");
    assert(html.contains("id=\"grid-bg\"") && "Missing grid background rect!");
    std::cout << " -> PASSED: Viewport background grid verified.\n";

    // 9. Test Floating Controls and Navigation Toolbar
    std::cout << "[TEST] 7. Verifying Floating Controls Toolbar & Zoom...\n";
    assert(html.contains("id=\"controls-bar\"") && "Missing controls bar!");
    assert(html.contains("id=\"btn-zoom-in\"") && "Missing zoom in button!");
    assert(html.contains("id=\"btn-zoom-out\"") && "Missing zoom out button!");
    assert(html.contains("id=\"btn-zoom-reset\"") && "Missing fit/reset button!");
    std::cout << " -> PASSED: Navigation toolbar verified.\n";

    // 10. Test Sidebar Inspector Data for Both Nodes and Edges
    std::cout << "[TEST] 8. Verifying Full Inspector Sidebar Support...\n";
    assert(html.contains("data-kind=\"edge\"") && "Missing data-kind edge!");
    assert(html.contains("data-src-name=") && "Missing edge source name!");
    assert(html.contains("data-dst-name=") && "Missing edge destination name!");
    assert(html.contains("data-status=") && "Missing edge status!");
    assert(html.contains("Edge Selection") && "Missing JS edge selection handler!");
    assert(html.contains("status-badge status-") && "Missing status badge in sidebar!");
    std::cout << " -> PASSED: Inspector data binding for nodes and edges verified.\n";

    // 11. Test Smooth Bezier Spline SVG Export (C command)
    std::cout << "[TEST] 9. Verifying Smooth Bezier Spline SVG Export (C command)...\n";
    window.setGlobalRoutingAlgorithm(EdgeRoutingAlgorithm::SmoothBezier);
    QString bezierOutPath = "test_bezier_export.html";
    window.exportToInteractiveHtml(bezierOutPath);
    QFile bezierFile(bezierOutPath);
    if (bezierFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString bezierHtml = QTextStream(&bezierFile).readAll();
        bezierFile.close();
        bezierFile.remove();
        assert(bezierHtml.contains(" C ") && "Missing SVG cubic bezier curve command in SmoothBezier export!");
        std::cout << " -> PASSED: Smooth Bezier exported as true SVG cubic curve (C cp1 cp2 ep).\n";
    }

    // 12. Overwrite architecture.html if output directory exists
    QStringList htmlDestinations = {
        "/workspace/OpenArch/build/architecture.html",
        "build/architecture.html",
        "../build/architecture.html",
        "architecture.html"
    };
    for (const auto& dest : htmlDestinations) {
        QFileInfo fi(dest);
        if (fi.dir().exists()) {
            QFile archHtml(dest);
            if (archHtml.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                QTextStream out(&archHtml);
                out << html;
                archHtml.close();
                std::cout << "[INFO] Updated " << dest.toStdString() << " with high-fidelity export.\n";
                break;
            }
        }
    }

    // 13. Cleanup temporary test files
    file.remove();
    if (createdTempJson && QFile::exists(tempJsonPath)) {
        QFile::remove(tempJsonPath);
    }

    std::cout << "\n===================================================\n";
    std::cout << " ALL INTERACTIVE HTML TESTS PASSED SUCCESSFULLY! \n";
    std::cout << "===================================================\n";
    return 0;
}
