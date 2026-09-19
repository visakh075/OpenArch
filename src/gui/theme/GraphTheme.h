#pragma once

#include <QColor>
#include <QString>
#include <QVector>
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
};