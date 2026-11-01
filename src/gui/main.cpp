#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QFileInfo>
#include <QFile>

#include "MainWindow.h"
#include "GraphThemeManager.h"
#include "theme/ApplicationThemeManager.h"

int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("OpenArch");
    app.setOrganizationName("OpenArch");

    // Initialize cross-platform application UI theme (independent of graph theme)
    ApplicationThemeManager::initialize(&app);

    QCommandLineParser parser;
    parser.setApplicationDescription("OpenArch GUI");
    parser.addHelpOption();

    QCommandLineOption themeOption(
        QStringList() << "t" << "theme",
        "Theme file path",
        "theme");

    QCommandLineOption dbOption(
        QStringList() << "d" << "db",
        "SQLite database or JSON architecture file path",
        "db");

    parser.addOption(themeOption);
    parser.addOption(dbOption);
    parser.addPositionalArgument("file", "Optional file to open directly (DB or JSON)", "[file]");

    parser.process(app);

    QString themePath = parser.value(themeOption);
    QString dbPath    = parser.value(dbOption);

    // Support opening files directly via positional argument (e.g., ./openarch-gui arch.json)
    const QStringList positionalArgs = parser.positionalArguments();
    if (dbPath.isEmpty() && !positionalArgs.isEmpty())
    {
        dbPath = positionalArgs.first();
    }

    qDebug() << "Theme:" << (themePath.isEmpty() ? "dark.json (default)" : themePath);
    if (!dbPath.isEmpty())
    {
        qDebug() << "DB:" << dbPath;
    }

    /*
     * THEME INITIALIZATION
     */
    GraphThemeManager themeManager;
    const QString initialTheme = themePath.isEmpty() ? GraphThemeManager::defaultThemePath() : themePath;
    themeManager.load(initialTheme);

    /*
     * MAIN WINDOW
     */
    MainWindow w;

    // Only load database if explicitly specified via CLI; otherwise show Welcome view
    if (!dbPath.isEmpty())
    {
        w.setDb(dbPath.toStdString());
    }

    w.show();

    return app.exec();
}