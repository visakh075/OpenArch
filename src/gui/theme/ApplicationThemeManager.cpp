// src/gui/theme/ApplicationThemeManager.cpp

#include "ApplicationThemeManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

ApplicationThemeManager* ApplicationThemeManager::s_instance = nullptr;

ApplicationThemeManager* ApplicationThemeManager::instance()
{
    if (!s_instance)
    {
        s_instance = new ApplicationThemeManager(qApp);
    }
    return s_instance;
}

ApplicationThemeManager::ApplicationThemeManager(QObject* parent)
    : QObject(parent)
{
    if (!s_instance)
    {
        s_instance = this;
    }
}

ApplicationThemeManager::~ApplicationThemeManager()
{
    if (s_instance == this)
    {
        s_instance = nullptr;
    }
}

QString ApplicationThemeManager::currentThemeId()
{
    return instance()->m_currentThemeId;
}

AppThemeMode ApplicationThemeManager::currentThemeMode()
{
    const QString id = currentThemeId().toLower();
    if (id == "dark")             return AppThemeMode::Dark;
    if (id == "light")            return AppThemeMode::Light;
    if (id == "nord")             return AppThemeMode::NordDark;
    if (id == "midnight")         return AppThemeMode::Midnight;
    if (id == "solarized_dark")   return AppThemeMode::SolarizedDark;
    if (id == "solarized_light")  return AppThemeMode::SolarizedLight;
    if (id == "system")           return AppThemeMode::System;
    return AppThemeMode::Custom;
}

void ApplicationThemeManager::setThemeMode(AppThemeMode mode)
{
    switch (mode)
    {
    case AppThemeMode::Dark:           setThemeById("dark"); break;
    case AppThemeMode::Light:          setThemeById("light"); break;
    case AppThemeMode::NordDark:       setThemeById("nord"); break;
    case AppThemeMode::Midnight:       setThemeById("midnight"); break;
    case AppThemeMode::SolarizedDark:  setThemeById("solarized_dark"); break;
    case AppThemeMode::SolarizedLight: setThemeById("solarized_light"); break;
    case AppThemeMode::System:         setThemeById("system"); break;
    case AppThemeMode::Custom:         break;
    }
}

QString ApplicationThemeManager::customThemesDirectory()
{
    // 1. If workspace themes/ui exists and is writable, use it
    const QString workspaceDir = QDir::currentPath() + QStringLiteral("/themes/ui");
    if (QDir(workspaceDir).exists())
    {
        ensureSampleThemeExists();
        return workspaceDir;
    }

    // 2. Standard user config location
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (baseDir.isEmpty() || baseDir == (QDir::homePath() + QStringLiteral("/.config")))
    {
        baseDir = QDir::homePath() + QStringLiteral("/.config/OpenArch");
    }

    QString customDir = baseDir + QStringLiteral("/ui_themes");
    if (!QDir().mkpath(customDir))
    {
        // Fallback for sandboxed, restricted, or portable environments
        customDir = QDir::tempPath() + QStringLiteral("/openarch_ui_themes");
        QDir().mkpath(customDir);
    }

    ensureSampleThemeExists();
    return customDir;
}

void ApplicationThemeManager::ensureSampleThemeExists()
{
    QString targetDir = QDir::currentPath() + QStringLiteral("/themes/ui");
    if (!QDir(targetDir).exists())
    {
        QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        if (baseDir.isEmpty() || baseDir == (QDir::homePath() + QStringLiteral("/.config")))
        {
            baseDir = QDir::homePath() + QStringLiteral("/.config/OpenArch");
        }
        targetDir = baseDir + QStringLiteral("/ui_themes");
        if (!QDir(targetDir).exists())
        {
            targetDir = QDir::tempPath() + QStringLiteral("/openarch_ui_themes");
        }
    }

    const QString samplePath = targetDir + QStringLiteral("/emerald_dark.json");

    if (!QFile::exists(samplePath))
    {
        QJsonObject root;
        root["name"] = "Emerald Dark";
        root["mode"] = "dark";
        root["window"] = "#111f18";
        root["base"] = "#0a130f";
        root["text"] = "#e0f2e9";
        root["button"] = "#182c22";
        root["border"] = "#254536";
        root["accent"] = "#10b981";

        QJsonDocument doc(root);
        QFile f(samplePath);
        if (f.open(QIODevice::WriteOnly))
        {
            f.write(doc.toJson(QJsonDocument::Indented));
        }
    }
}

QList<UiThemeInfo> ApplicationThemeManager::availableThemes()
{
    QList<UiThemeInfo> list;
    QSet<QString> seenIds;

    // 1. Built-in curated UI themes
    auto addBuiltIn = [&](const QString& id, const QString& name) {
        list.append({id, name, true, QString()});
        seenIds.insert(id.toLower());
    };

    addBuiltIn("dark", QStringLiteral("Dark"));
    addBuiltIn("light", QStringLiteral("Light"));
    addBuiltIn("nord", QStringLiteral("Nord Dark"));
    addBuiltIn("midnight", QStringLiteral("Midnight OLED"));
    addBuiltIn("solarized_dark", QStringLiteral("Solarized Dark"));
    addBuiltIn("solarized_light", QStringLiteral("Solarized Light"));
    addBuiltIn("system", QStringLiteral("System Default"));

    // 2. Custom user UI themes from customThemesDirectory() and themes/ui folders
    const QStringList candidateDirs = {
        customThemesDirectory(),
        QStringLiteral("themes/ui"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/themes/ui")
    };

    for (const QString& dirPath : candidateDirs)
    {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;

        const QStringList entries = dir.entryList(QStringList() << "*.json" << "*.qss", QDir::Files, QDir::Name);
        for (const QString& file : entries)
        {
            const QString customId = QStringLiteral("custom:") + file;
            if (seenIds.contains(customId.toLower()))
                continue;

            const QString fullPath = dir.filePath(file);
            QFileInfo fi(fullPath);

            UiThemeInfo info;
            info.id = customId;
            info.isBuiltIn = false;
            info.filePath = fullPath;

            if (fi.suffix().compare("json", Qt::CaseInsensitive) == 0)
            {
                QFile f(fullPath);
                if (f.open(QIODevice::ReadOnly))
                {
                    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                    info.name = doc.object().value("name").toString(fi.baseName());
                }
                else
                {
                    info.name = fi.baseName();
                }
            }
            else
            {
                info.name = fi.baseName() + QStringLiteral(" (QSS)");
            }

            seenIds.insert(customId.toLower());
            list.append(info);
        }
    }

    return list;
}

bool ApplicationThemeManager::loadCustomThemeFile(const QString& filePath, QString* errorOut)
{
    QFileInfo fi(filePath);
    if (!fi.exists())
    {
        if (errorOut) *errorOut = QStringLiteral("File does not exist: ") + filePath;
        return false;
    }

    const QString targetDir = customThemesDirectory();
    const QString targetPath = QDir(targetDir).filePath(fi.fileName());

    // Copy to customThemesDirectory if not already there
    if (fi.canonicalFilePath() != QFileInfo(targetPath).canonicalFilePath())
    {
        if (QFile::exists(targetPath))
        {
            QFile::remove(targetPath);
        }
        if (!QFile::copy(filePath, targetPath))
        {
            if (errorOut) *errorOut = QStringLiteral("Failed to copy theme file to user themes folder.");
            return false;
        }
    }

    const QString customId = QStringLiteral("custom:") + fi.fileName();
    setThemeById(customId);
    return true;
}

void ApplicationThemeManager::initialize(QApplication* app)
{
    if (!app)
    {
        app = qobject_cast<QApplication*>(QCoreApplication::instance());
    }

    QSettings settings;
    const QString saved = settings.value("UI/Theme", "dark").toString();
    instance()->m_currentThemeId = saved;

    applyTheme(saved, app);
}

void ApplicationThemeManager::setThemeById(const QString& id)
{
    instance()->m_currentThemeId = id;

    QSettings settings;
    settings.setValue("UI/Theme", id);

    applyTheme(id);
    emit instance()->themeChanged(id);
}

ApplicationThemeManager::ThemeColors ApplicationThemeManager::colorsForTheme(const QString& id)
{
    const QString lower = id.trimmed().toLower();
    ThemeColors c;

    if (lower == "light")
    {
        c.window = QColor("#f3f4f6");
        c.base = QColor("#ffffff");
        c.text = QColor("#1f2937");
        c.button = QColor("#e5e7eb");
        c.border = QColor("#d1d5db");
        c.accent = QColor("#2563eb");
        c.isDark = false;
    }
    else if (lower == "nord")
    {
        c.window = QColor("#2e3440");
        c.base = QColor("#242933");
        c.text = QColor("#eceff4");
        c.button = QColor("#3b4252");
        c.border = QColor("#434c5e");
        c.accent = QColor("#88c0d0");
        c.isDark = true;
    }
    else if (lower == "midnight")
    {
        c.window = QColor("#121212");
        c.base = QColor("#000000");
        c.text = QColor("#f5f5f5");
        c.button = QColor("#1e1e1e");
        c.border = QColor("#2d2d2d");
        c.accent = QColor("#00e5ff");
        c.isDark = true;
    }
    else if (lower == "solarized_dark")
    {
        c.window = QColor("#002b36");
        c.base = QColor("#00212b");
        c.text = QColor("#93a1a1");
        c.button = QColor("#073642");
        c.border = QColor("#0d404d");
        c.accent = QColor("#268bd2");
        c.isDark = true;
    }
    else if (lower == "solarized_light")
    {
        c.window = QColor("#fdf6e3");
        c.base = QColor("#ffffff");
        c.text = QColor("#586e75");
        c.button = QColor("#eee8d5");
        c.border = QColor("#d3cbaf");
        c.accent = QColor("#268bd2");
        c.isDark = false;
    }
    else if (lower == "system")
    {
        QPalette pal = qApp ? qApp->palette() : QPalette();
        c.window = pal.color(QPalette::Window);
        c.base = pal.color(QPalette::Base);
        c.text = pal.color(QPalette::WindowText);
        c.button = pal.color(QPalette::Button);
        c.border = pal.color(QPalette::Mid);
        c.accent = pal.color(QPalette::Highlight);
        c.isDark = (c.window.lightness() < 128);
    }
    else if (lower.startsWith("custom:"))
    {
        const QString fileName = id.mid(7);
        QString fullPath = QDir(customThemesDirectory()).filePath(fileName);
        if (!QFile::exists(fullPath))
        {
            if (QFile::exists(QStringLiteral("themes/ui/") + fileName))
                fullPath = QStringLiteral("themes/ui/") + fileName;
            else if (QFile::exists(QCoreApplication::applicationDirPath() + "/themes/ui/" + fileName))
                fullPath = QCoreApplication::applicationDirPath() + "/themes/ui/" + fileName;
        }

        QFileInfo fi(fullPath);
        if (fi.suffix().compare("json", Qt::CaseInsensitive) == 0)
        {
            QFile f(fullPath);
            if (f.open(QIODevice::ReadOnly))
            {
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                QJsonObject obj = doc.object();

                c.isDark = (obj.value("mode").toString("dark").toLower() != "light");
                c.window = QColor(obj.value("window").toString(c.isDark ? "#21252b" : "#f3f4f6"));
                c.base = QColor(obj.value("base").toString(c.isDark ? "#1b1d23" : "#ffffff"));
                c.text = QColor(obj.value("text").toString(c.isDark ? "#d7dae0" : "#1f2937"));
                c.button = QColor(obj.value("button").toString(c.isDark ? "#282c34" : "#e5e7eb"));
                c.border = QColor(obj.value("border").toString(c.isDark ? "#3e4451" : "#d1d5db"));
                c.accent = QColor(obj.value("accent").toString(c.isDark ? "#4d78cc" : "#2563eb"));
                return c;
            }
        }

        QPalette pal = qApp ? qApp->palette() : QPalette();
        c.window = pal.color(QPalette::Window);
        c.base = pal.color(QPalette::Base);
        c.text = pal.color(QPalette::WindowText);
        c.button = pal.color(QPalette::Button);
        c.border = pal.color(QPalette::Mid);
        c.accent = pal.color(QPalette::Highlight);
        c.isDark = (c.window.lightness() < 128);
    }
    else
    {
        // Default "dark"
        c.window = QColor("#21252b");
        c.base = QColor("#1b1d23");
        c.text = QColor("#d7dae0");
        c.button = QColor("#282c34");
        c.border = QColor("#3e4451");
        c.accent = QColor("#4d78cc");
        c.isDark = true;
    }

    return c;
}

ApplicationThemeManager::ThemeColors ApplicationThemeManager::currentThemeColors()
{
    return colorsForTheme(currentThemeId());
}

void ApplicationThemeManager::applyTheme(const QString& id, QApplication* app)
{
    if (!app)
    {
        app = qobject_cast<QApplication*>(QCoreApplication::instance());
    }
    if (!app)
    {
        return;
    }

    const QString lower = id.trimmed().toLower();

    if (lower == "system")
    {
        app->setStyle(QStyleFactory::create("Fusion"));
        app->setPalette(app->style()->standardPalette());
        app->setStyleSheet(QString());
        return;
    }

    // Force Fusion style for cross-platform stability across Windows 11, Linux, and macOS
    if (!app->style() || app->style()->objectName().compare("Fusion", Qt::CaseInsensitive) != 0)
    {
        app->setStyle(QStyleFactory::create("Fusion"));
    }

    const ThemeColors c = colorsForTheme(id);
    app->setPalette(paletteForColors(c.window, c.base, c.text, c.button, c.border, c.accent, c.isDark));

    QString qss = styleSheetForColors(c.window, c.base, c.text, c.button, c.border, c.accent, c.isDark);

    if (lower.startsWith("custom:"))
    {
        const QString fileName = id.mid(7);
        QString fullPath = QDir(customThemesDirectory()).filePath(fileName);
        if (!QFile::exists(fullPath))
        {
            if (QFile::exists(QStringLiteral("themes/ui/") + fileName))
                fullPath = QStringLiteral("themes/ui/") + fileName;
            else if (QFile::exists(QCoreApplication::applicationDirPath() + "/themes/ui/" + fileName))
                fullPath = QCoreApplication::applicationDirPath() + "/themes/ui/" + fileName;
        }

        QFileInfo fi(fullPath);
        if (fi.suffix().compare("qss", Qt::CaseInsensitive) == 0)
        {
            QFile f(fullPath);
            if (f.open(QIODevice::ReadOnly))
            {
                app->setStyleSheet(QString::fromUtf8(f.readAll()));
                return;
            }
        }
        else if (fi.suffix().compare("json", Qt::CaseInsensitive) == 0)
        {
            QFile f(fullPath);
            if (f.open(QIODevice::ReadOnly))
            {
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                QJsonObject obj = doc.object();
                if (obj.contains("styleSheet"))
                {
                    qss += "\n" + obj.value("styleSheet").toString();
                }
            }
        }
    }

    app->setStyleSheet(qss);
}

QPalette ApplicationThemeManager::paletteForColors(const QColor& window, const QColor& base, const QColor& text,
                                                  const QColor& button, const QColor& border, const QColor& accent,
                                                  bool isDark)
{
    const QColor alternateBase = isDark ? base.lighter(120) : base.darker(104);
    const QColor textDisabled = isDark ? text.darker(160) : text.lighter(180);
    const QColor tooltipBg = isDark ? button : window;
    const qreal accentLum = 0.299 * accent.redF() + 0.587 * accent.greenF() + 0.114 * accent.blueF();
    const QColor highlightedText = (accentLum < 0.55) ? QColor("#ffffff") : QColor("#000000");

    QPalette pal;

    for (auto group : {QPalette::Active, QPalette::Inactive})
    {
        pal.setColor(group, QPalette::Window, window);
        pal.setColor(group, QPalette::WindowText, text);
        pal.setColor(group, QPalette::Base, base);
        pal.setColor(group, QPalette::AlternateBase, alternateBase);
        pal.setColor(group, QPalette::ToolTipBase, tooltipBg);
        pal.setColor(group, QPalette::ToolTipText, text);
        pal.setColor(group, QPalette::Text, text);
        pal.setColor(group, QPalette::Button, button);
        pal.setColor(group, QPalette::ButtonText, text);
        pal.setColor(group, QPalette::BrightText, isDark ? QColor("#e06c75") : QColor("#dc2626"));
        pal.setColor(group, QPalette::Highlight, accent);
        pal.setColor(group, QPalette::HighlightedText, highlightedText);
        pal.setColor(group, QPalette::Link, accent);
        pal.setColor(group, QPalette::LinkVisited, isDark ? accent.lighter(130) : accent.darker(130));
        pal.setColor(group, QPalette::Midlight, button.lighter(110));
        pal.setColor(group, QPalette::Mid, border);
        pal.setColor(group, QPalette::Dark, border.darker(120));
        pal.setColor(group, QPalette::Shadow, isDark ? QColor("#000000") : border.darker(130));
    }

    pal.setColor(QPalette::Disabled, QPalette::Window, window);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::Base, base);
    pal.setColor(QPalette::Disabled, QPalette::AlternateBase, alternateBase);
    pal.setColor(QPalette::Disabled, QPalette::ToolTipBase, tooltipBg);
    pal.setColor(QPalette::Disabled, QPalette::ToolTipText, textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::Text, textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::Button, button);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::BrightText, isDark ? QColor("#e06c75") : QColor("#dc2626"));
    pal.setColor(QPalette::Disabled, QPalette::Highlight, isDark ? QColor("#30363d") : QColor("#e5e7eb"));
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, textDisabled);

    return pal;
}

QString ApplicationThemeManager::styleSheetForColors(const QColor& window, const QColor& base, const QColor& text,
                                                    const QColor& button, const QColor& border, const QColor& accent,
                                                    bool isDark)
{
    const qreal accentLum = 0.299 * accent.redF() + 0.587 * accent.greenF() + 0.114 * accent.blueF();
    const QString textOnAccent = (accentLum < 0.55) ? QStringLiteral("#ffffff") : QStringLiteral("#000000");
    const QString hoverItemBg = isDark ? QStringLiteral("rgba(255, 255, 255, 18)") : QStringLiteral("rgba(0, 0, 0, 14)");

    return QString(R"(
        QMainWindow::separator {
            width: 4px;
            height: 4px;
            background: %1;
        }
        QMainWindow::separator:hover {
            background: %2;
        }
        QMenuBar {
            border-bottom: 1px solid %1;
            padding: 2px 4px;
        }
        QMenuBar::item {
            padding: 4px 8px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background: %3;
        }
        QMenu {
            border: 1px solid %1;
            border-radius: 6px;
            padding: 4px;
            background: %4;
        }
        QMenu::item {
            padding: 4px 24px 4px 12px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background: %2;
            color: %5;
        }
        QMenu::separator {
            height: 1px;
            background: %1;
            margin: 4px 6px;
        }
        QToolBar {
            border-bottom: 1px solid %1;
            spacing: 3px;
            padding: 3px;
        }
        QToolButton {
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 3px;
        }
        QToolButton:hover {
            border: 1px solid %1;
            background: %3;
        }
        QToolButton:pressed {
            background: %2;
            color: %5;
        }
        QDockWidget {
            titlebar-close-icon: none;
            titlebar-normal-icon: none;
        }
        QDockWidget::title {
            text-align: left;
            padding: 6px;
            background: %3;
            border-bottom: 1px solid %1;
            font-weight: bold;
        }
        QTabBar::tab {
            padding: 6px 12px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background: %6;
            border-bottom: 2px solid %2;
            font-weight: bold;
        }
        QTabBar::tab:!selected:hover {
            background: %3;
        }
        QHeaderView::section {
            padding: 4px 6px;
            border: none;
            border-bottom: 1px solid %1;
            border-right: 1px solid %1;
            background: %6;
        }
        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            border: 1px solid %1;
            border-radius: 4px;
            padding: 4px 6px;
            background: %7;
            color: %8;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border: 1px solid %2;
        }
        QPushButton {
            border: 1px solid %1;
            border-radius: 4px;
            padding: 5px 12px;
            background: %6;
            color: %8;
        }
        QPushButton:hover {
            border-color: %2;
            background: %3;
        }
        QPushButton:pressed {
            background: %2;
            color: %5;
        }
        QScrollBar:vertical {
            border: none;
            width: 10px;
            margin: 0px;
            background: transparent;
        }
        QScrollBar::handle:vertical {
            background: %1;
            min-height: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical:hover {
            background: %2;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            height: 0px;
            background: none;
        }
        QScrollBar:horizontal {
            border: none;
            height: 10px;
            margin: 0px;
            background: transparent;
        }
        QScrollBar::handle:horizontal {
            background: %1;
            min-width: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal:hover {
            background: %2;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            width: 0px;
            background: none;
        }
        QToolTip {
            border: 1px solid %1;
            border-radius: 4px;
            padding: 4px 8px;
            background: %6;
            color: %8;
        }
    )")
    .arg(border.name(QColor::HexRgb))   // %1
    .arg(accent.name(QColor::HexRgb))   // %2
    .arg(hoverItemBg)                   // %3
    .arg(window.name(QColor::HexRgb))   // %4
    .arg(textOnAccent)                  // %5
    .arg(button.name(QColor::HexRgb))   // %6
    .arg(base.name(QColor::HexRgb))     // %7
    .arg(text.name(QColor::HexRgb));    // %8
}
