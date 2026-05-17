// src/gui/theme/GraphThemeManager.h

#pragma once

#include "GraphTheme.h"

#include <QString>

class GraphThemeManager
{
public:

    static bool load(const QString& path);

    static const GraphTheme& theme();

private:

    static GraphTheme m_theme;
};