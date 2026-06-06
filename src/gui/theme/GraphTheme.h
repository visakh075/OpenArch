// src/gui/theme/GraphTheme.h

#pragma once

#include <QColor>
#include <QString>

/*
 * =========================================================
 * TEXT STYLE
 * =========================================================
 */

struct GraphTextStyle
{
    QColor color;

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
 * VIEW
 * =========================================================
 */

struct GraphGridTheme
{
    bool enabled = true;

    QColor color;

    QColor majorColor;

    int spacing = 20;

    int majorSpacing = 100;
};

struct GraphViewTheme
{
    QColor background;

    GraphGridTheme grid;
};

/*
 * =========================================================
 * NODE STATE
 * =========================================================
 */

struct GraphNodeState
{
    QColor background;

    QColor border;

    int borderWidth = 2;

    int radius = 8;

    int padding = 8;

    GraphTextStyle title;

    GraphTextStyle body;
};

/*
 * =========================================================
 * NODE STYLE
 * =========================================================
 */

struct GraphNodeTheme
{
    GraphNodeState normal;

    GraphNodeState hover;

    GraphNodeState selected;
};

/*
 * =========================================================
 * EDGE LABEL STATE
 * =========================================================
 */

struct GraphEdgeLabelState
{
    QColor textColor;

    QColor backgroundColor;

    QColor borderColor;

    int borderWidth = 1;

    int fontSize = 11;

    bool bold = false;

    int paddingX = 6;

    int paddingY = 3;

    int radius = 4;

    int offset = 8;
};

/*
 * =========================================================
 * ARROW STATE
 * =========================================================
 */

struct GraphArrowState
{
    QColor lineColor;

    QColor fillColor;

    QColor borderColor;

    int width = 14;

    int height = 10;

    int lineWidth = 2;

    int borderWidth = 1;
};

/*
 * =========================================================
 * EDGE STATE
 * =========================================================
 */

struct GraphEdgeState
{
    QColor lineColor;

    int lineWidth = 2;

    bool dashed = false;

    GraphArrowState arrow;

    GraphEdgeLabelState label;
};

/*
 * =========================================================
 * EDGE STYLE
 * =========================================================
 */

struct GraphEdgeTheme
{
    GraphEdgeState normal;

    GraphEdgeState hover;

    GraphEdgeState selected;
};

/*
 * =========================================================
 * PORT STATE
 * =========================================================
 */

struct GraphPortState
{
    QColor inputColor;

    QColor outputColor;

    QColor hoverColor;

    int radius = 6;

    int borderWidth = 1;

    QColor borderColor;
};

/*
 * =========================================================
 * PORT STYLE
 * =========================================================
 */

struct GraphPortTheme
{
    GraphPortState normal;

    GraphPortState hover;

    GraphPortState selected;
};

/*
 * =========================================================
 * SELECTION
 * =========================================================
 */

struct GraphSelectionTheme
{
    QColor outline;

    QColor fill;

    int borderWidth = 1;
};

/*
 * =========================================================
 * INTERACTION
 * =========================================================
 */

struct GraphInteractionTheme
{
    QColor hoverOutline;

    QColor invalidConnection;

    QColor dropTarget;

    QColor snapGuide;

    int snapGuideWidth = 1;
};

/*
 * =========================================================
 * ROOT THEME
 * =========================================================
 */

struct GraphTheme
{
    QString name;

    GraphViewTheme view;

    GraphNodeTheme node;

    GraphEdgeTheme edge;

    GraphPortTheme port;

    GraphSelectionTheme selection;

    GraphInteractionTheme interaction;
};