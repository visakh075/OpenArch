// src/gui/theme/GraphThemeManager.cpp

#include "GraphThemeManager.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

GraphTheme GraphThemeManager::m_theme;

namespace
{

QColor loadColor(
    const QJsonObject& obj,
    const QString& key,
    const QString& fallback = "#ffffff")
{
    return QColor(
        obj.value(key).toString(fallback));
}

int loadInt(
    const QJsonObject& obj,
    const QString& key,
    int fallback)
{
    return obj.value(key).toInt(fallback);
}

bool loadBool(
    const QJsonObject& obj,
    const QString& key,
    bool fallback)
{
    return obj.value(key).toBool(fallback);
}

/*
 * TEXT STYLE
 */

GraphTextStyle loadTextStyle(
    const QJsonObject& obj)
{
    GraphTextStyle style;

    style.color =
        loadColor(obj,
                  "color",
                  "#ffffff");

    style.size =
        loadInt(obj,
                "size",
                12);

    style.bold =
        loadBool(obj,
                 "bold",
                 false);

    return style;
}

/*
 * NODE STYLE
 */

GraphNodeStyle loadNodeStyle(
    const QJsonObject& obj)
{
    GraphNodeStyle style;

    style.background =
        loadColor(obj,
                  "background",
                  "#2d2d30");

    style.border =
        loadColor(obj,
                  "border",
                  "#6a95ff");

    style.borderWidth =
        loadInt(obj,
                "borderWidth",
                2);

    style.radius =
        loadInt(obj,
                "radius",
                8);

    style.title =
        loadTextStyle(
            obj.value("title")
                .toObject());

    style.body =
        loadTextStyle(
            obj.value("body")
                .toObject());

    return style;
}

/*
 * EDGE LABEL STYLE
 */

GraphEdgeLabelStyle loadEdgeLabelStyle(
    const QJsonObject& obj)
{
    GraphEdgeLabelStyle style;

    style.textColor =
        loadColor(obj,
                  "textColor",
                  "#ffffff");

    style.backgroundColor =
        loadColor(obj,
                  "backgroundColor",
                  "#202020");

    style.borderColor =
        loadColor(obj,
                  "borderColor",
                  "#404040");

    style.borderWidth =
        loadInt(obj,
                "borderWidth",
                1);

    style.fontSize =
        loadInt(obj,
                "fontSize",
                11);

    style.bold =
        loadBool(obj,
                 "bold",
                 false);

    style.paddingX =
        loadInt(obj,
                "paddingX",
                6);

    style.paddingY =
        loadInt(obj,
                "paddingY",
                3);

    style.radius =
        loadInt(obj,
                "radius",
                4);

    style.offset =
        loadInt(obj,
                "offset",
                8);

    return style;
}

/*
 * ARROW STYLE
 */

GraphArrowStyle loadArrowStyle(
    const QJsonObject& obj)
{
    GraphArrowStyle style;

    style.lineColor =
        loadColor(obj,
                  "lineColor",
                  "#ffffff");

    style.fillColor =
        loadColor(obj,
                  "fillColor",
                  "#ffffff");

    style.width =
        loadInt(obj,
                "width",
                14);

    style.height =
        loadInt(obj,
                "height",
                10);

    style.lineWidth =
        loadInt(obj,
                "lineWidth",
                2);

    return style;
}

/*
 * EDGE STYLE
 */

GraphEdgeStyle loadEdgeStyle(
    const QJsonObject& obj)
{
    GraphEdgeStyle style;

    style.lineColor =
        loadColor(obj,
                  "lineColor",
                  "#ffffff");

    style.lineWidth =
        loadInt(obj,
                "lineWidth",
                2);

    style.arrow =
        loadArrowStyle(
            obj.value("arrow")
                .toObject());

    style.label =
        loadEdgeLabelStyle(
            obj.value("label")
                .toObject());

    return style;
}

}

/*
 * PUBLIC
 */

bool GraphThemeManager::load(
    const QString& path)
{
    qDebug()
        << "Loading theme:"
        << path;

    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug()
            << "Failed to open theme file";

        return false;
    }

    QByteArray data =
        file.readAll();

    QJsonParseError error;

    QJsonDocument doc =
        QJsonDocument::fromJson(
            data,
            &error);

    if (error.error !=
        QJsonParseError::NoError)
    {
        qDebug()
            << "Theme parse error:"
            << error.errorString();

        return false;
    }

    if (!doc.isObject())
    {
        qDebug()
            << "Theme root is not object";

        return false;
    }

    QJsonObject root =
        doc.object();

    /*
     * NAME
     */

    m_theme.name =
        root.value("name")
            .toString("Unnamed Theme");

    /*
     * VIEW
     */

    {
        QJsonObject viewObj =
            root.value("view")
                .toObject();

        m_theme.view.background =
            loadColor(viewObj,
                      "background",
                      "#202020");

        QJsonObject gridObj =
            viewObj.value("grid")
                .toObject();

        m_theme.view.grid.enabled =
            loadBool(gridObj,
                     "enabled",
                     true);

        m_theme.view.grid.minorColor =
            loadColor(gridObj,
                      "minorColor",
                      "#2a2a2a");

        m_theme.view.grid.majorColor =
            loadColor(gridObj,
                      "majorColor",
                      "#353535");

        m_theme.view.grid.spacing =
            loadInt(gridObj,
                    "spacing",
                    20);

        m_theme.view.grid.majorSpacing =
            loadInt(gridObj,
                    "majorSpacing",
                    100);
    }

    /*
     * NODE
     */

    {
        QJsonObject nodeObj =
            root.value("node")
                .toObject();

        m_theme.node.normal =
            loadNodeStyle(
                nodeObj.value("normal")
                    .toObject());

        m_theme.node.hover =
            loadNodeStyle(
                nodeObj.value("hover")
                    .toObject());

        m_theme.node.selected =
            loadNodeStyle(
                nodeObj.value("selected")
                    .toObject());
    }

    /*
     * EDGE
     */

    {
        QJsonObject edgeObj =
            root.value("edge")
                .toObject();

        m_theme.edge.normal =
            loadEdgeStyle(
                edgeObj.value("normal")
                    .toObject());

        m_theme.edge.hover =
            loadEdgeStyle(
                edgeObj.value("hover")
                    .toObject());

        m_theme.edge.selected =
            loadEdgeStyle(
                edgeObj.value("selected")
                    .toObject());
    }

    /*
     * PORT
     */

    {
        QJsonObject portObj =
            root.value("port")
                .toObject();

        m_theme.port.inputColor =
            loadColor(portObj,
                      "inputColor",
                      "#4ec9b0");

        m_theme.port.outputColor =
            loadColor(portObj,
                      "outputColor",
                      "#dcdcaa");

        m_theme.port.hoverColor =
            loadColor(portObj,
                      "hoverColor",
                      "#ffffff");

        m_theme.port.radius =
            loadInt(portObj,
                    "radius",
                    6);
    }

    /*
     * SELECTION
     */

    {
        QJsonObject selectionObj =
            root.value("selection")
                .toObject();

        m_theme.selection.outline =
            loadColor(selectionObj,
                      "outline",
                      "#ffcc00");

        m_theme.selection.fill =
            loadColor(selectionObj,
                      "fill",
                      "#ffcc0022");
    }

    /*
     * INTERACTION
     */

    {
        QJsonObject interactionObj =
            root.value("interaction")
                .toObject();

        m_theme.interaction.hoverOutline =
            loadColor(interactionObj,
                      "hoverOutline",
                      "#ffffff");

        m_theme.interaction.invalidConnection =
            loadColor(interactionObj,
                      "invalidConnection",
                      "#ff4444");

        m_theme.interaction.dropTarget =
            loadColor(interactionObj,
                      "dropTarget",
                      "#00ff88");
    }

    qDebug()
        << "Theme loaded successfully";

    return true;
}

const GraphTheme&
GraphThemeManager::theme()
{
    return m_theme;
}