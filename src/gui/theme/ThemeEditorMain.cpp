#include <QApplication>
#include <QFileInfo>
#include "ThemeEditorWindow.h"
#include "GraphThemeManager.h"
#include "ApplicationThemeManager.h"

int main(int argc, char* argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("OpenArch Theme Editor");
    app.setOrganizationName("OpenArch");

    // Initialize cross-platform application UI theme (independent of graph theme)
    ApplicationThemeManager::initialize(&app);

    GraphThemeManager themeManager;

    // Load default theme initially from self-contained presets
    themeManager.load(GraphThemeManager::defaultThemePath());

    ThemeEditorWindow window;
    window.resize(1320, 820);

    if (argc > 1) {
        QString path = QString::fromLocal8Bit(argv[1]);
        if (QFileInfo::exists(path)) {
            window.loadTheme(path);
        }
    }

    window.show();
    return app.exec();
}
