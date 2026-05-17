#include <QApplication>

#include "MainWindow.h"
#include "GraphThemeManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    /*
     * CREATE THEME MANAGER FIRST
     */

    GraphThemeManager themeManager;

    themeManager.load("dark.json");

    /*
     * MAIN WINDOW
     */

    MainWindow w;

    w.show();

    return app.exec();
}