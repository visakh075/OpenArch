// src/gui/theme/GraphThemeManager.cpp

#include "GraphThemeManager.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

GraphThemeManager*
GraphThemeManager::s_instance = nullptr;

/*
 * =========================================================
 * HELPERS
 * =========================================================
 */

namespace
{

QColor loadColor(
    const QJsonObject& obj,
    const QString& key,
    const QString& fallback = "#ffffff")
{
    return QColor(
        obj.value(key)
            .toString(fallback));
}

int loadInt(
    const QJsonObject& obj,
    const QString& key,
    int fallback)
{
    return obj.value(key)
        .toInt(fallback);
}

bool loadBool(
    const QJsonObject& obj,
    const QString& key,
    bool fallback)
{
    return obj.value(key)
        .toBool(fallback);
}

/*
 * ---------------------------------------------------------
 * TEXT STYLE
 * ---------------------------------------------------------
 */

GraphTextStyle loadTextStyle(
    const QJsonObject& obj)
{
    GraphTextStyle style;

    style.color =
        loadColor(
            obj,
            "color",
            "#ffffff");

    style.size =
        loadInt(
            obj,
            "size",
            12);

    style.bold =
        loadBool(
            obj,
            "bold",
            false);

    style.italic =
        loadBool(
            obj,
            "italic",
            false);

    return style;
}

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

    obj["italic"] =
        style.italic;

    return obj;
}

/*
 * ---------------------------------------------------------
 * NODE STATE
 * ---------------------------------------------------------
 */

GraphNodeState loadNodeState(
    const QJsonObject& obj)
{
    GraphNodeState state;

    state.background =
        loadColor(
            obj,
            "background",
            "#2d2d30");

    state.border =
        loadColor(
            obj,
            "border",
            "#6a95ff");

    state.borderWidth =
        loadInt(
            obj,
            "borderWidth",
            2);

    state.radius =
        loadInt(
            obj,
            "radius",
            8);

    state.padding =
        loadInt(
            obj,
            "padding",
            8);

    state.title =
        loadTextStyle(
            obj.value("title")
                .toObject());

    state.body =
        loadTextStyle(
            obj.value("body")
                .toObject());

    return state;
}

QJsonObject saveNodeState(
    const GraphNodeState& state)
{
    QJsonObject obj;

    obj["background"] =
        state.background.name(
            QColor::HexArgb);

    obj["border"] =
        state.border.name(
            QColor::HexArgb);

    obj["borderWidth"] =
        state.borderWidth;

    obj["radius"] =
        state.radius;

    obj["padding"] =
        state.padding;

    obj["title"] =
        saveTextStyle(
            state.title);

    obj["body"] =
        saveTextStyle(
            state.body);

    return obj;
}

/*
 * ---------------------------------------------------------
 * ARROW STATE
 * ---------------------------------------------------------
 */

GraphArrowState loadArrowState(
    const QJsonObject& obj)
{
    GraphArrowState state;

    state.lineColor =
        loadColor(
            obj,
            "lineColor",
            "#ffffff");

    state.fillColor =
        loadColor(
            obj,
            "fillColor",
            "#ffffff");

    state.borderColor =
        loadColor(
            obj,
            "borderColor",
            "#000000");

    state.width =
        loadInt(
            obj,
            "width",
            14);

    state.height =
        loadInt(
            obj,
            "height",
            10);

    state.lineWidth =
        loadInt(
            obj,
            "lineWidth",
            2);

    state.borderWidth =
        loadInt(
            obj,
            "borderWidth",
            1);

    return state;
}

QJsonObject saveArrowState(
    const GraphArrowState& state)
{
    QJsonObject obj;

    obj["lineColor"] =
        state.lineColor.name(
            QColor::HexArgb);

    obj["fillColor"] =
        state.fillColor.name(
            QColor::HexArgb);

    obj["borderColor"] =
        state.borderColor.name(
            QColor::HexArgb);

    obj["width"] =
        state.width;

    obj["height"] =
        state.height;

    obj["lineWidth"] =
        state.lineWidth;

    obj["borderWidth"] =
        state.borderWidth;

    return obj;
}

/*
 * ---------------------------------------------------------
 * EDGE LABEL STATE
 * ---------------------------------------------------------
 */

GraphEdgeLabelState loadEdgeLabelState(
    const QJsonObject& obj)
{
    GraphEdgeLabelState state;

    state.textColor =
        loadColor(
            obj,
            "textColor",
            "#ffffff");

    state.backgroundColor =
        loadColor(
            obj,
            "backgroundColor",
            "#202020");

    state.borderColor =
        loadColor(
            obj,
            "borderColor",
            "#404040");

    state.borderWidth =
        loadInt(
            obj,
            "borderWidth",
            1);

    state.fontSize =
        loadInt(
            obj,
            "fontSize",
            11);

    state.bold =
        loadBool(
            obj,
            "bold",
            false);

    state.paddingX =
        loadInt(
            obj,
            "paddingX",
            6);

    state.paddingY =
        loadInt(
            obj,
            "paddingY",
            3);

    state.radius =
        loadInt(
            obj,
            "radius",
            4);

    state.offset =
        loadInt(
            obj,
            "offset",
            8);

    return state;
}

QJsonObject saveEdgeLabelState(
    const GraphEdgeLabelState& state)
{
    QJsonObject obj;

    obj["textColor"] =
        state.textColor.name(
            QColor::HexArgb);

    obj["backgroundColor"] =
        state.backgroundColor.name(
            QColor::HexArgb);

    obj["borderColor"] =
        state.borderColor.name(
            QColor::HexArgb);

    obj["borderWidth"] =
        state.borderWidth;

    obj["fontSize"] =
        state.fontSize;

    obj["bold"] =
        state.bold;

    obj["paddingX"] =
        state.paddingX;

    obj["paddingY"] =
        state.paddingY;

    obj["radius"] =
        state.radius;

    obj["offset"] =
        state.offset;

    return obj;
}

/*
 * ---------------------------------------------------------
 * EDGE STATE
 * ---------------------------------------------------------
 */

GraphEdgeState loadEdgeState(
    const QJsonObject& obj)
{
    GraphEdgeState state;

    state.lineColor =
        loadColor(
            obj,
            "lineColor",
            "#ffffff");

    state.lineWidth =
        loadInt(
            obj,
            "lineWidth",
            2);

    state.dashed =
        loadBool(
            obj,
            "dashed",
            false);

    state.arrow =
        loadArrowState(
            obj.value("arrow")
                .toObject());

    state.label =
        loadEdgeLabelState(
            obj.value("label")
                .toObject());

    return state;
}

QJsonObject saveEdgeState(
    const GraphEdgeState& state)
{
    QJsonObject obj;

    obj["lineColor"] =
        state.lineColor.name(
            QColor::HexArgb);

    obj["lineWidth"] =
        state.lineWidth;

    obj["dashed"] =
        state.dashed;

    obj["arrow"] =
        saveArrowState(
            state.arrow);

    obj["label"] =
        saveEdgeLabelState(
            state.label);

    return obj;
}

}

/*
 * =========================================================
 * PUBLIC
 * =========================================================
 */

GraphThemeManager::GraphThemeManager(
    QObject* parent)
    : QObject(parent)
{
    s_instance = this;

    initializeDefaults();
}

GraphThemeManager*
GraphThemeManager::instance()
{
    return s_instance;
}

/*
 * ---------------------------------------------------------
 * DEFAULTS
 * ---------------------------------------------------------
 */

void GraphThemeManager::initializeDefaults()
{
    m_theme.name =
        "Default";

    /*
     * VIEW
     */

    m_theme.view.background =
        QColor("#202020");

    m_theme.view.grid.enabled =
        true;

    m_theme.view.grid.color =
        QColor("#2a2a2a");

    m_theme.view.grid.majorColor =
        QColor("#353535");
}

void GraphThemeManager::resetDefaults()
{
    initializeDefaults();

    emit themeChanged();
}

/*
 * ---------------------------------------------------------
 * LOAD
 * ---------------------------------------------------------
 */

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
            loadColor(
                viewObj,
                "background",
                "#202020");

        QJsonObject gridObj =
            viewObj.value("grid")
                .toObject();

        m_theme.view.grid.enabled =
            loadBool(
                gridObj,
                "enabled",
                true);

        m_theme.view.grid.color =
            loadColor(
                gridObj,
                "color",
                "#2a2a2a");

        m_theme.view.grid.majorColor =
            loadColor(
                gridObj,
                "majorColor",
                "#353535");

        m_theme.view.grid.spacing =
            loadInt(
                gridObj,
                "spacing",
                20);

        m_theme.view.grid.majorSpacing =
            loadInt(
                gridObj,
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
            loadNodeState(
                nodeObj.value("normal")
                    .toObject());

        m_theme.node.hover =
            loadNodeState(
                nodeObj.value("hover")
                    .toObject());

        m_theme.node.selected =
            loadNodeState(
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
            loadEdgeState(
                edgeObj.value("normal")
                    .toObject());

        m_theme.edge.hover =
            loadEdgeState(
                edgeObj.value("hover")
                    .toObject());

        m_theme.edge.selected =
            loadEdgeState(
                edgeObj.value("selected")
                    .toObject());
    }

    emit themeChanged();

    qDebug()
        << "Theme loaded successfully";

    return true;
}

/*
 * ---------------------------------------------------------
 * SAVE
 * ---------------------------------------------------------
 */

bool GraphThemeManager::save(
    const QString& path) const
{
    QJsonObject root;

    root["name"] =
        m_theme.name;

    /*
     * VIEW
     */

    {
        QJsonObject viewObj;

        viewObj["background"] =
            m_theme.view.background.name(
                QColor::HexArgb);

        QJsonObject gridObj;

        gridObj["enabled"] =
            m_theme.view.grid.enabled;

        gridObj["color"] =
            m_theme.view.grid.color.name(
                QColor::HexArgb);

        gridObj["majorColor"] =
            m_theme.view.grid.majorColor.name(
                QColor::HexArgb);

        gridObj["spacing"] =
            m_theme.view.grid.spacing;

        gridObj["majorSpacing"] =
            m_theme.view.grid.majorSpacing;

        viewObj["grid"] =
            gridObj;

        root["view"] =
            viewObj;
    }

    /*
     * NODE
     */

    {
        QJsonObject nodeObj;

        nodeObj["normal"] =
            saveNodeState(
                m_theme.node.normal);

        nodeObj["hover"] =
            saveNodeState(
                m_theme.node.hover);

        nodeObj["selected"] =
            saveNodeState(
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
            saveEdgeState(
                m_theme.edge.normal);

        edgeObj["hover"] =
            saveEdgeState(
                m_theme.edge.hover);

        edgeObj["selected"] =
            saveEdgeState(
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

/*
 * ---------------------------------------------------------
 * ACCESS
 * ---------------------------------------------------------
 */

const GraphTheme&
GraphThemeManager::theme() const
{
    return m_theme;
}

GraphTheme&
GraphThemeManager::mutableTheme()
{
    return m_theme;
}

/*
 * ---------------------------------------------------------
 * SIGNALS
 * ---------------------------------------------------------
 */

void GraphThemeManager::notifyThemeChanged()
{
    emit themeChanged();
}