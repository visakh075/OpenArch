// src/gui/theme/GraphThemeManager.h

#pragma once

#include "GraphTheme.h"

#include <QObject>
#include <QString>

/*
 * =========================================================
 * GRAPH THEME MANAGER
 * =========================================================
 */

class GraphThemeManager : public QObject
{
    Q_OBJECT

public:

    explicit GraphThemeManager(
        QObject* parent = nullptr);

    /*
     * SINGLETON
     */

    static GraphThemeManager* instance();

    /*
     * LOAD / SAVE
     */

    bool load(
        const QString& path);

    bool save(
        const QString& path) const;

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

private:

    /*
     * INTERNAL HELPERS
     */

    void initializeDefaults();

private:

    static GraphThemeManager* s_instance;

    GraphTheme m_theme;
};