// src/gui/theme/GraphTheme.h

#pragma once

#include <QColor>
#include <QString>

/*
 * VIEW
 */

struct GraphGridTheme
{
    bool enabled = true;

    QColor minorColor;
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
 * TEXT
 */

struct GraphTextStyle
{
    QColor color;

    int size = 12;

    bool bold = false;
};

/*
 * NODE
 */

struct GraphNodeStyle
{
    QColor background;

    QColor border;

    int borderWidth = 2;

    int radius = 8;

    GraphTextStyle title;

    GraphTextStyle body;
};

struct GraphNodeTheme
{
    GraphNodeStyle normal;

    GraphNodeStyle hover;

    GraphNodeStyle selected;
};

/*
 * EDGE
 */

struct GraphEdgeLabelStyle
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

struct GraphArrowStyle
{
    QColor lineColor;
    QColor fillColor;

    int width = 14;
    int height = 10;

    int lineWidth = 2;
};

struct GraphEdgeStyle
{
    QColor lineColor;

    int lineWidth = 2;

    GraphArrowStyle arrow;

    GraphEdgeLabelStyle label;
};

struct GraphEdgeTheme
{
    GraphEdgeStyle normal;

    GraphEdgeStyle hover;

    GraphEdgeStyle selected;
};

/*
 * PORT
 */

struct GraphPortTheme
{
    QColor inputColor;
    QColor outputColor;

    QColor hoverColor;

    int radius = 6;
};

/*
 * SELECTION
 */

struct GraphSelectionTheme
{
    QColor outline;
    QColor fill;
};

/*
 * INTERACTION
 */

struct GraphInteractionTheme
{
    QColor hoverOutline;

    QColor invalidConnection;

    QColor dropTarget;
};

/*
 * ROOT THEME
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