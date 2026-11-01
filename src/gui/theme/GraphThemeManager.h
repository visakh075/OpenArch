// src/gui/theme/GraphThemeManager.h

#pragma once

#include "GraphTheme.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QPalette>

class QApplication;

/*
 * =========================================================
 * GRAPH THEME MANAGER
 * =========================================================
 */

class GraphThemeManager : public QObject
{
    Q_OBJECT

public:

    struct PresetThemeInfo
    {
        QString id;
        QString name;
        QString path;
        bool isBuiltIn{false};
    };

    explicit GraphThemeManager(
        QObject* parent = nullptr);
    ~GraphThemeManager() override;

    /*
     * SINGLETON
     */

    static GraphThemeManager* instance();

    /*
     * PRESETS & RESOLUTION (SELF-CONTAINED)
     */

    static QString defaultThemePath();
    static QString resolveThemePath(const QString& path);
    static QList<PresetThemeInfo> availablePresets();

    /*
     * LOAD / SAVE
     */

    bool load(
        const QString& path);

    bool save(
        const QString& path);

    bool save();

    const QString& currentThemePath() const;
    void setCurrentThemePath(const QString& path);

    /*
     * ACCESS
     */

    const GraphTheme& theme() const;

    GraphTheme& mutableTheme();

    /*
     * UTILITIES
     */

    void resetDefaults();

    void notifyThemeChanged();

signals:

    void themeChanged();
    void themeLoaded(const QString& path);
    void themeSaved(const QString& path);

private:

    /*
     * INTERNAL HELPERS
     */

    void initializeDefaults();

private:

    static GraphThemeManager* s_instance;

    GraphTheme m_theme;
    QString m_currentPath;
};