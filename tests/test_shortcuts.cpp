#include <QApplication>
#include <QAction>
#include <cassert>
#include <iostream>
#include "MainWindow.h"
#include "ShortcutManager.h"
#include "ShortcutConfigDialog.h"
#include "GraphThemeManager.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    // Initialize theme manager with default theme
    GraphThemeManager themeManager;
    themeManager.load("src/gui/theme/themes/Dark.json");

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

    std::cout << "\n==============================================" << std::endl;
    std::cout << " ALL SHORTCUT TESTS PASSED SUCCESSFULLY! (9/9)" << std::endl;
    std::cout << "==============================================" << std::endl;

    return 0;
}
