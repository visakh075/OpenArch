#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QFileInfo>

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
        "SQLite database file path",
        "db");

    parser.addOption(themeOption);
    parser.addOption(dbOption);

    parser.process(app);

    QString themePath = parser.value(themeOption);
    QString dbPath    = parser.value(dbOption);

    // Fall back to testdb.db if no CLI argument is provided
    if (dbPath.isEmpty())
    {
        dbPath = "testdb.db";
    }

    qDebug() << "Theme:" << (themePath.isEmpty() ? "dark.json (default)" : themePath);
    qDebug() << "DB:" << dbPath;

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
        themeManager.load("dark.json");
    }

    /*
     * MAIN WINDOW
     */
    MainWindow w;
    w.setDb(dbPath.toStdString());
    w.show();

    return app.exec();
}