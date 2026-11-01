// src/gui/theme/ApplicationThemeManager.h

#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QPalette>
#include <QColor>

class QApplication;
class QActionGroup;
class QMenu;

enum class AppThemeMode
{
    Dark,
    Light,
    NordDark,
    Midnight,
    SolarizedDark,
    SolarizedLight,
    System,
    Custom
};

struct UiThemeInfo
{
    QString id;
    QString name;
    bool isBuiltIn{true};
    QString filePath;
};

/*
 * =========================================================
 * APPLICATION THEME MANAGER
 * =========================================================
 * Manages the application window, widget, menu, dock, and dialog
 * styling completely independently from the graph/canvas theme.
 */

class ApplicationThemeManager : public QObject
{
    Q_OBJECT

public:
    static ApplicationThemeManager* instance();

    static QString currentThemeId();
    static void setThemeById(const QString& id);

    static AppThemeMode currentThemeMode();
    static void setThemeMode(AppThemeMode mode);

    struct ThemeColors
    {
        QColor window;
        QColor base;
        QColor text;
        QColor button;
        QColor border;
        QColor accent;
        bool isDark{true};
    };

    static ThemeColors currentThemeColors();
    static ThemeColors colorsForTheme(const QString& id);

    static QList<UiThemeInfo> availableThemes();

    // Directory where user custom UI themes (.json or .qss) are stored
    static QString customThemesDirectory();

    // Loads a custom UI theme from file (.json or .qss), copies to custom folder if needed, and applies it
    static bool loadCustomThemeFile(const QString& filePath, QString* errorOut = nullptr);

    // Initializes and applies the saved UI theme from QSettings (defaults to Dark)
    static void initialize(QApplication* app = nullptr);

    // Applies the given theme ID immediately to QApplication
    static void applyTheme(const QString& id, QApplication* app = nullptr);

    // Helper palette and stylesheet generators for built-in and custom color sets
    static QPalette paletteForColors(const QColor& window, const QColor& base, const QColor& text,
                                     const QColor& button, const QColor& border, const QColor& accent,
                                     bool isDark);
    static QString styleSheetForColors(const QColor& window, const QColor& base, const QColor& text,
                                       const QColor& button, const QColor& border, const QColor& accent,
                                       bool isDark);

signals:
    void themeChanged(const QString& themeId);

private:
    explicit ApplicationThemeManager(QObject* parent = nullptr);
    ~ApplicationThemeManager() override;

    static void ensureSampleThemeExists();

    static ApplicationThemeManager* s_instance;
    QString m_currentThemeId{QStringLiteral("dark")};
};
