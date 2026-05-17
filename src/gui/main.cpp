
#include <QApplication>
#include "MainWindow.h"
#include "theme/GraphThemeManager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    GraphThemeManager::load(
        "dark.json");
    w.show();
    return app.exec();
}
