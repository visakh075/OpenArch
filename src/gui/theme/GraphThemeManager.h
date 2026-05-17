// src/gui/theme/GraphThemeManager.h

#pragma once

#include "GraphTheme.h"

#include <QObject>
#include <QString>

class GraphThemeManager : public QObject
{
    Q_OBJECT

public:

    explicit GraphThemeManager(
        QObject* parent = nullptr);

    static GraphThemeManager* instance();

    /*
     * LOAD / SAVE
     */

    bool load(
        const QString& path);

    bool save(
        const QString& path);

    /*
     * ACCESS
     */

    const GraphTheme& theme() const;

    GraphTheme& mutableTheme();

    void notifyThemeChanged();

signals:

    void themeChanged();

private:

    static GraphThemeManager* s_instance;

    GraphTheme m_theme;
};