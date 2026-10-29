#include <QApplication>
#include <QFileInfo>
#include "ThemeEditorWindow.h"
#include "GraphThemeManager.h"

int main(int argc, char* argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("OpenArch Theme Editor");
    app.setOrganizationName("OpenArch");

    GraphThemeManager themeManager;

    // Load default theme initially if available
    if (QFileInfo::exists("src/gui/theme/themes/Dark.json")) {
        themeManager.load("src/gui/theme/themes/Dark.json");
    } else if (QFileInfo::exists("themes/Dark.json")) {
        themeManager.load("themes/Dark.json");
    }

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
