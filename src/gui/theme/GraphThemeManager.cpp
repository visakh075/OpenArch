
// src/gui/theme/GraphThemeManager.cpp

#include "GraphThemeManager.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

GraphThemeManager*
GraphThemeManager::s_instance = nullptr;

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
 * LOAD TEXT STYLE
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
 * LOAD NODE STYLE
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
 * LOAD ARROW STYLE
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
 * LOAD EDGE LABEL STYLE
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
 * LOAD EDGE STYLE
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

/*
 * SAVE TEXT STYLE
 */

QJsonObject saveTextStyle(
    const GraphTextStyle& style)
{
    QJsonObject obj;

    obj["color"] =
        style.color.name(
            QColor::HexArgb);

    obj["size"] =
        style.size;

    obj["bold"] =
        style.bold;

    return obj;
}

/*
 * SAVE NODE STYLE
 */

QJsonObject saveNodeStyle(
    const GraphNodeStyle& style)
{
    QJsonObject obj;

    obj["background"] =
        style.background.name(
            QColor::HexArgb);

    obj["border"] =
        style.border.name(
            QColor::HexArgb);

    obj["borderWidth"] =
        style.borderWidth;

    obj["radius"] =
        style.radius;

    obj["title"] =
        saveTextStyle(
            style.title);

    obj["body"] =
        saveTextStyle(
            style.body);

    return obj;
}

/*
 * SAVE ARROW STYLE
 */

QJsonObject saveArrowStyle(
    const GraphArrowStyle& style)
{
    QJsonObject obj;

    obj["lineColor"] =
        style.lineColor.name(
            QColor::HexArgb);

    obj["fillColor"] =
        style.fillColor.name(
            QColor::HexArgb);

    obj["width"] =
        style.width;

    obj["height"] =
        style.height;

    obj["lineWidth"] =
        style.lineWidth;

    return obj;
}

/*
 * SAVE EDGE LABEL STYLE
 */

QJsonObject saveEdgeLabelStyle(
    const GraphEdgeLabelStyle& style)
{
    QJsonObject obj;

    obj["textColor"] =
        style.textColor.name(
            QColor::HexArgb);

    obj["backgroundColor"] =
        style.backgroundColor.name(
            QColor::HexArgb);

    obj["borderColor"] =
        style.borderColor.name(
            QColor::HexArgb);

    obj["borderWidth"] =
        style.borderWidth;

    obj["fontSize"] =
        style.fontSize;

    obj["bold"] =
        style.bold;

    obj["paddingX"] =
        style.paddingX;

    obj["paddingY"] =
        style.paddingY;

    obj["radius"] =
        style.radius;

    obj["offset"] =
        style.offset;

    return obj;
}

/*
 * SAVE EDGE STYLE
 */

QJsonObject saveEdgeStyle(
    const GraphEdgeStyle& style)
{
    QJsonObject obj;

    obj["lineColor"] =
        style.lineColor.name(
            QColor::HexArgb);

    obj["lineWidth"] =
        style.lineWidth;

    obj["arrow"] =
        saveArrowStyle(
            style.arrow);

    obj["label"] =
        saveEdgeLabelStyle(
            style.label);

    return obj;
}

}

/*
 * PUBLIC
 */

GraphThemeManager::GraphThemeManager(
    QObject* parent)
    : QObject(parent)
{
    s_instance = this;
}

GraphThemeManager*
GraphThemeManager::instance()
{
    return s_instance;
}

bool GraphThemeManager::load(
    const QString& path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug()
            << "Failed to open theme file:"
            << path;

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

    emit themeChanged();

    qDebug()
        << "Theme loaded successfully";

    return true;
}

bool GraphThemeManager::save(
    const QString& path)
{
    QJsonObject root;

    root["name"] =
        m_theme.name;

    /*
     * NODE
     */

    {
        QJsonObject nodeObj;

        nodeObj["normal"] =
            saveNodeStyle(
                m_theme.node.normal);

        nodeObj["hover"] =
            saveNodeStyle(
                m_theme.node.hover);

        nodeObj["selected"] =
            saveNodeStyle(
                m_theme.node.selected);

        root["node"] =
            nodeObj;
    }

    /*
     * EDGE
     */

    {
        QJsonObject edgeObj;

        edgeObj["normal"] =
            saveEdgeStyle(
                m_theme.edge.normal);

        edgeObj["hover"] =
            saveEdgeStyle(
                m_theme.edge.hover);

        edgeObj["selected"] =
            saveEdgeStyle(
                m_theme.edge.selected);

        root["edge"] =
            edgeObj;
    }

    QJsonDocument doc(root);

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    file.write(
        doc.toJson(
            QJsonDocument::Indented));

    return true;
}

const GraphTheme&
GraphThemeManager::theme() const
{
    return m_theme;
}

void GraphThemeManager::notifyThemeChanged()
{
    emit themeChanged();
}
GraphTheme&
GraphThemeManager::mutableTheme()
{
    return m_theme;
}