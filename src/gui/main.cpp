#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QFileInfo>
#include <QFile>

#include "MainWindow.h"
#include "GraphThemeManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

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

    if (!themePath.isEmpty())
    {
        themeManager.load(themePath);
    }
    else
    {
        QString defaultTheme = "dark.json";
        if (!QFile::exists(defaultTheme)) {
            QStringList candidates = {
                QCoreApplication::applicationDirPath() + "/dark.json",
                QCoreApplication::applicationDirPath() + "/themes/Dark.json",
                "themes/Dark.json",
                "src/gui/theme/themes/Dark.json",
                QCoreApplication::applicationDirPath() + "/../src/gui/theme/themes/Dark.json"
            };
            for (const auto& candidate : candidates) {
                if (QFile::exists(candidate)) {
                    defaultTheme = candidate;
                    break;
                }
            }
        }
        themeManager.load(defaultTheme);
    }

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