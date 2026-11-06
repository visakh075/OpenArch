#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <QMap>
#include <Qt>

/*
 * =========================================================
 * TEXT STYLE
 * =========================================================
 */
struct GraphTextStyle
{
    QColor color{"#ffffff"};
    int size = 12;
    bool bold = false;
    bool italic = false;
    int pX = 5;
    int pY = 5;
    int mX = 5;
    int mY = 5;
    Qt::Alignment align = Qt::AlignCenter;
};

/*
 * =========================================================
 * VIEW & GRID
 * =========================================================
 */
struct GraphGridTheme
{
    bool enabled = true;
    QColor minorColor{"#2a2a2a"};
    QColor majorColor{"#353535"};
    int spacing = 20;
    int majorSpacing = 100;
    int lineWidth = 1;
};

struct GraphViewTheme
{
    QColor background{"#202020"};
    GraphGridTheme grid;
};

/*
 * =========================================================
 * UNIFIED COMPONENT STATE (Used by both Node and Container)
 * =========================================================
 */
struct GraphComponentState
{
    QColor background{"#2d2d30"};
    QColor border{"#6a95ff"};
    int borderWidth = 2;
    Qt::PenStyle borderStyle = Qt::SolidLine;
    QVector<qreal> dashPattern;

    int radius = 8;
    int padding = 8;

    QColor headerBackground{"#6a95ff"};
    int headerHeightPadding = 12;

    GraphTextStyle title;
    GraphTextStyle body;
    GraphTextStyle tertiary{QColor(160, 160, 160, 170), 9, false, true, 5, 5, 5, 5, Qt::AlignCenter};
};

using GraphNodeState = GraphComponentState;
using GraphContainerState = GraphComponentState;

/*
 * =========================================================
 * COMPONENT THEMES (Node & Container share identical templates)
 * =========================================================
 */
struct GraphComponentTheme
{
    GraphComponentState normal;
    GraphComponentState hover;
    GraphComponentState selected;
    qreal minWidth = 120.0;
    qreal minHeight = 50.0;
};

using GraphNodeTheme = GraphComponentTheme;
using GraphContainerTheme = GraphComponentTheme;

/*
 * =========================================================
 * EDGE LABEL & ARROW
 * =========================================================
 */
struct GraphEdgeLabelState
{
    QColor textColor{"#ffffff"};
    QColor backgroundColor{"#202020"};
    QColor borderColor{"#404040"};
    int borderWidth = 1;
    int fontSize = 11;
    bool bold = false;
    int paddingX = 6;
    int paddingY = 3;
    int radius = 4;
    int offset = 8;
};

struct GraphArrowState
{
    QColor lineColor{"#ffffff"};
    QColor fillColor{"#ffffff"};
    QColor borderColor{"#000000"};
    int width = 14;
    int height = 10;
    int lineWidth = 2;
    int borderWidth = 1;
};

/*
 * =========================================================
 * PREVIEW LINE (Connecting Interaction)
 * =========================================================
 */
struct GraphPreviewLineTheme
{
    QColor color{0, 180, 216};
    int width = 2;
    Qt::PenStyle style = Qt::DashLine;
    QVector<qreal> dashPattern;
    GraphArrowState arrow;

    GraphPreviewLineTheme()
    {
        arrow.lineColor = color;
        arrow.fillColor = color;
        arrow.borderColor = QColor("#000000");
        arrow.width = 14;
        arrow.height = 10;
        arrow.lineWidth = 2;
        arrow.borderWidth = 1;
    }

    inline QColor lineColor() const { return color; }
    inline void setLineColor(const QColor& c) { color = c; }
    inline int lineWidth() const { return width; }
    inline void setLineWidth(int w) { width = w; }
    inline Qt::PenStyle lineStyle() const { return style; }
    inline void setLineStyle(Qt::PenStyle s) { style = s; }
};

/*
 * =========================================================
 * EDGE STATE & STYLE
 * =========================================================
 */
struct GraphEdgeState
{
    QColor lineColor{"#ffffff"};
    int lineWidth = 2;
    Qt::PenStyle lineStyle = Qt::SolidLine;
    QVector<qreal> dashPattern;
    bool dashed = false;

    GraphArrowState arrow;
    GraphEdgeLabelState label;
};

struct GraphEdgeTheme
{
    GraphEdgeState normal;
    GraphEdgeState hover;
    GraphEdgeState selected;
    GraphPreviewLineTheme preview;
};

/*
 * =========================================================
 * PORT STATE & STYLE
 * =========================================================
 */
struct GraphPortState
{
    QColor inputColor{"#4ec9b0"};
    QColor outputColor{"#dcdcaa"};
    QColor hoverColor{"#ffffff"};
    int radius = 6;
    int borderWidth = 1;
    QColor borderColor{"#000000"};
};

struct GraphPortTheme
{
    GraphPortState normal;
    GraphPortState hover;
    GraphPortState selected;
};

/*
 * =========================================================
 * SELECTION & INTERACTION
 * =========================================================
 */
struct GraphSelectionTheme
{
    QColor outline{"#ffcc00"};
    QColor fill{"#ffcc0022"};
    int borderWidth = 1;
};

struct GraphInteractionTheme
{
    QColor hoverOutline{"#ffffff"};
    QColor invalidConnection{"#ff4444"};
    QColor dropTarget{"#00ff88"};
    QColor snapGuide{"#00b4d8"};
    int snapGuideWidth = 1;
};

/*
 * =========================================================
 * ROOT THEME
 * =========================================================
 */
struct GraphTheme
{
    QString name{"Default"};
    GraphViewTheme view;
    GraphNodeTheme node;
    GraphContainerTheme container;
    GraphEdgeTheme edge;
    GraphPortTheme port;
    GraphSelectionTheme selection;
    GraphInteractionTheme interaction;

    // Type- and Name-specific overrides (keys stored lowercase, e.g. "pwr", "lan", "auth#service")
    QMap<QString, GraphEdgeTheme> edgeTypeThemes;
    QMap<QString, GraphComponentTheme> nodeTypeThemes;
    QMap<QString, GraphComponentTheme> containerTypeThemes;

    const GraphEdgeTheme& edgeTheme(const QString& name, const QString& type) const
    {
        if (!edgeTypeThemes.isEmpty())
        {
            QString cleanName = name.trimmed().toLower();
            QString cleanType = type.trimmed().toLower();

            // 1. Highest Specificity: Compound "name#type"
            if (!cleanName.isEmpty() && !cleanType.isEmpty())
            {
                auto it = edgeTypeThemes.find(cleanName + "#" + cleanType);
                if (it != edgeTypeThemes.end())
                    return it.value();
            }

            // 2. Specificity 2: Exact Name "name"
            if (!cleanName.isEmpty())
            {
                auto it = edgeTypeThemes.find(cleanName);
                if (it != edgeTypeThemes.end())
                    return it.value();
            }

            // 3. Specificity 3: Type "type"
            if (!cleanType.isEmpty())
            {
                auto it = edgeTypeThemes.find(cleanType);
                if (it != edgeTypeThemes.end())
                    return it.value();
            }
        }
        return edge;
    }

    const GraphEdgeTheme& edgeThemeForType(const QString& type) const
    {
        return edgeTheme(QString(), type);
    }

    const GraphComponentTheme& nodeTheme(const QString& name, const QString& type) const
    {
        if (!nodeTypeThemes.isEmpty())
        {
            QString cleanName = name.trimmed().toLower();
            QString cleanType = type.trimmed().toLower();

            // 1. Highest Specificity: Compound "name#type"
            if (!cleanName.isEmpty() && !cleanType.isEmpty())
            {
                auto it = nodeTypeThemes.find(cleanName + "#" + cleanType);
                if (it != nodeTypeThemes.end())
                    return it.value();
            }

            // 2. Specificity 2: Exact Name "name"
            if (!cleanName.isEmpty())
            {
                auto it = nodeTypeThemes.find(cleanName);
                if (it != nodeTypeThemes.end())
                    return it.value();
            }

            // 3. Specificity 3: Type "type"
            if (!cleanType.isEmpty())
            {
                auto it = nodeTypeThemes.find(cleanType);
                if (it != nodeTypeThemes.end())
                    return it.value();
            }
        }
        return node;
    }

    const GraphComponentTheme& nodeThemeForType(const QString& type) const
    {
        return nodeTheme(QString(), type);
    }

    const GraphComponentTheme& containerTheme(const QString& name, const QString& type) const
    {
        QString cleanName = name.trimmed().toLower();
        QString cleanType = type.trimmed().toLower();

        // 1. Highest Specificity: Compound "name#type"
        if (!cleanName.isEmpty() && !cleanType.isEmpty())
        {
            QString compound = cleanName + "#" + cleanType;
            auto it = containerTypeThemes.find(compound);
            if (it != containerTypeThemes.end())
                return it.value();
            auto itNode = nodeTypeThemes.find(compound);
            if (itNode != nodeTypeThemes.end())
                return itNode.value();
        }

        // 2. Specificity 2: Exact Name "name"
        if (!cleanName.isEmpty())
        {
            auto it = containerTypeThemes.find(cleanName);
            if (it != containerTypeThemes.end())
                return it.value();
            auto itNode = nodeTypeThemes.find(cleanName);
            if (itNode != nodeTypeThemes.end())
                return itNode.value();
        }

        // 3. Specificity 3: Type "type"
        if (!cleanType.isEmpty())
        {
            auto it = containerTypeThemes.find(cleanType);
            if (it != containerTypeThemes.end())
                return it.value();
            auto itNode = nodeTypeThemes.find(cleanType);
            if (itNode != nodeTypeThemes.end())
                return itNode.value();
        }

        return container;
    }

    const GraphComponentTheme& containerThemeForType(const QString& type) const
    {
        return containerTheme(QString(), type);
    }

    const GraphEdgeState& edgeStateFor(const QString& name, const QString& type, bool selected, bool hovered) const
    {
        const auto& t = edgeTheme(name, type);
        if (selected) return t.selected;
        if (hovered)  return t.hover;
        return t.normal;
    }

    const GraphEdgeState& edgeStateFor(const QString& type, bool selected, bool hovered) const
    {
        return edgeStateFor(QString(), type, selected, hovered);
    }

    const GraphComponentState& nodeStateFor(const QString& name, const QString& type, bool selected, bool hovered) const
    {
        const auto& t = nodeTheme(name, type);
        if (selected) return t.selected;
        if (hovered)  return t.hover;
        return t.normal;
    }

    const GraphComponentState& nodeStateFor(const QString& type, bool selected, bool hovered) const
    {
        return nodeStateFor(QString(), type, selected, hovered);
    }

    const GraphComponentState& containerStateFor(const QString& name, const QString& type, bool selected, bool hovered) const
    {
        const auto& t = containerTheme(name, type);
        if (selected) return t.selected;
        if (hovered)  return t.hover;
        return t.normal;
    }

    const GraphComponentState& containerStateFor(const QString& type, bool selected, bool hovered) const
    {
        return containerStateFor(QString(), type, selected, hovered);
    }
};