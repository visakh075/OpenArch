#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>

#include "MainWindow.h"
#include "GraphThemeManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "OpenArch GUI");
    parser.addHelpOption();

    QCommandLineOption themeOption(
        QStringList() << "t" << "theme",
        "Theme file path",
        "theme");

    QCommandLineOption dbOption(
        QStringList() << "d" << "db",
        "Database file path",
        "db");

    parser.addOption(themeOption);
    parser.addOption(dbOption);

    parser.process(app);

    QString themePath =
        parser.value(themeOption);

    QString dbPath =
        parser.value(dbOption);

    qDebug() << "Theme:" << themePath;
    qDebug() << "DB:" << dbPath;

    /*
     * CREATE THEME MANAGER FIRST
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

    MainWindow w;

    // Open database
    if (!dbPath.isEmpty())
    {
        w.setDb(dbPath.toStdString());
    }

    w.show();

    return app.exec();
}