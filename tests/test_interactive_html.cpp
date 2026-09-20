#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <iostream>
#include <cassert>

#include "MainWindow.h"
#include "GraphThemeManager.h"

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
    window.setDb("/workspace/OpenArch/build/architecture.json");

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

    // 11. Overwrite /workspace/OpenArch/build/architecture.html with the new export
    QFile archHtml("/workspace/OpenArch/build/architecture.html");
    if (archHtml.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&archHtml);
        out << html;
        archHtml.close();
        std::cout << "[INFO] Updated /workspace/OpenArch/build/architecture.html with high-fidelity export.\n";
    }

    // 12. Cleanup temporary test file
    file.remove();

    std::cout << "\n===================================================\n";
    std::cout << " ALL INTERACTIVE HTML TESTS PASSED SUCCESSFULLY! \n";
    std::cout << "===================================================\n";
    return 0;
}
