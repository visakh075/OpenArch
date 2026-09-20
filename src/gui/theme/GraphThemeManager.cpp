#include "GraphThemeManager.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

GraphThemeManager* GraphThemeManager::s_instance = nullptr;

namespace
{

QColor loadColor(const QJsonObject& obj, const QString& key, const QString& fallback = "#ffffff")
{
    return QColor(obj.value(key).toString(fallback));
}

int loadInt(const QJsonObject& obj, const QString& key, int fallback)
{
    return obj.value(key).toInt(fallback);
}

qreal loadDouble(const QJsonObject& obj, const QString& key, qreal fallback)
{
    return obj.value(key).toDouble(fallback);
}

bool loadBool(const QJsonObject& obj, const QString& key, bool fallback)
{
    return obj.value(key).toBool(fallback);
}

Qt::PenStyle loadPenStyle(const QJsonObject& obj, const QString& key, Qt::PenStyle fallback = Qt::SolidLine)
{
    if (!obj.contains(key) || !obj.value(key).isString())
        return fallback;

    const QString val = obj.value(key).toString().trimmed().toLower();
    if (val == "dash" || val == "dashed")      return Qt::DashLine;
    if (val == "dot" || val == "dotted")       return Qt::DotLine;
    if (val == "dashdot")                      return Qt::DashDotLine;
    if (val == "dashdotdot")                   return Qt::DashDotDotLine;
    if (val == "none" || val == "nopen")       return Qt::NoPen;
    return Qt::SolidLine;
}

QString savePenStyle(Qt::PenStyle style)
{
    switch (style)
    {
    case Qt::DashLine:       return "dashed";
    case Qt::DotLine:        return "dotted";
    case Qt::DashDotLine:    return "dashdot";
    case Qt::DashDotDotLine: return "dashdotdot";
    case Qt::NoPen:          return "none";
    default:                 return "solid";
    }
}

Qt::Alignment loadAlign(const QJsonObject& obj, const QString& key, Qt::Alignment fallback)
{
    if (!obj.contains(key) || !obj.value(key).isString())
        return fallback;

    const QString value = obj.value(key).toString().trimmed().toLower();
    if (value == "left")    return Qt::AlignLeft;
    if (value == "right")   return Qt::AlignRight;
    if (value == "center")  return Qt::AlignHCenter;
    if (value == "justify") return Qt::AlignJustify;
    if (value == "top")     return Qt::AlignTop;
    if (value == "bottom")  return Qt::AlignBottom;
    if (value == "vcenter") return Qt::AlignVCenter;
    return fallback;
}

GraphTextStyle loadTextStyle(const QJsonObject& obj)
{
    GraphTextStyle style;
    style.color = loadColor(obj, "color", "#ffffff");
    style.size = loadInt(obj, "size", 12);
    style.bold = loadBool(obj, "bold", false);
    style.italic = loadBool(obj, "italic", false);
    style.pX = loadInt(obj, "pX", 5);
    style.pY = loadInt(obj, "pY", 5);
    style.mX = loadInt(obj, "mX", 5);
    style.mY = loadInt(obj, "mY", 5);
    style.align = loadAlign(obj, "align", Qt::AlignCenter);
    return style;
}

QString saveAlign(Qt::Alignment align)
{
    if (align & Qt::AlignLeft)    return "left";
    if (align & Qt::AlignRight)   return "right";
    if (align & Qt::AlignJustify) return "justify";
    return "center";
}

QJsonObject saveTextStyle(const GraphTextStyle& style)
{
    QJsonObject obj;
    obj["color"] = style.color.name(QColor::HexArgb);
    obj["size"] = style.size;
    obj["bold"] = style.bold;
    obj["italic"] = style.italic;
    obj["align"] = saveAlign(style.align);
    return obj;
}

GraphComponentState loadComponentState(const QJsonObject& obj, bool isContainerDefault = false)
{
    GraphComponentState state;

    // Default container colors carry alpha in the color string itself
    QString defBg = isContainerDefault ? "#2d2d302d" : "#2d2d30ff";
    QString defBorder = "#6a95ffff";
    QString defHeader = isContainerDefault ? "#6a95ff23" : "#6a95ffff";

    state.background = loadColor(obj, "background", defBg);

    // Support legacy files that had separate backgroundAlpha
    if (obj.contains("backgroundAlpha")) {
        int a = obj["backgroundAlpha"].toInt(255);
        state.background.setAlpha(a);
    }

    state.border = loadColor(obj, "border", defBorder);
    state.borderWidth = loadInt(obj, "borderWidth", 2);
    state.borderStyle = loadPenStyle(obj, "borderStyle", isContainerDefault ? Qt::DashLine : Qt::SolidLine);

    if (obj.contains("dashPattern") && obj["dashPattern"].isArray())
    {
        for (const auto& val : obj["dashPattern"].toArray())
            state.dashPattern.append(val.toDouble(4.0));
    }
    else if (isContainerDefault)
    {
        state.dashPattern = {6.0, 4.0};
    }

    state.radius = loadInt(obj, "radius", 8);
    state.padding = loadInt(obj, "padding", 8);

    state.headerBackground = loadColor(obj, "headerBackground", defHeader);

    // Support legacy files that had separate headerAlpha
    if (obj.contains("headerAlpha")) {
        int ha = obj["headerAlpha"].toInt(35);
        state.headerBackground.setAlpha(ha);
    }

    state.headerHeightPadding = loadInt(obj, "headerHeightPadding", 12);

    state.title = loadTextStyle(obj.value("title").toObject());
    state.body = loadTextStyle(obj.value("body").toObject());
    return state;
}

QJsonObject saveComponentState(const GraphComponentState& state)
{
    QJsonObject obj;
    obj["background"] = state.background.name(QColor::HexArgb);
    obj["border"] = state.border.name(QColor::HexArgb);
    obj["borderWidth"] = state.borderWidth;
    obj["borderStyle"] = savePenStyle(state.borderStyle);

    if (!state.dashPattern.isEmpty())
    {
        QJsonArray arr;
        for (qreal v : state.dashPattern) arr.append(v);
        obj["dashPattern"] = arr;
    }

    obj["radius"] = state.radius;
    obj["padding"] = state.padding;

    obj["headerBackground"] = state.headerBackground.name(QColor::HexArgb);
    obj["headerHeightPadding"] = state.headerHeightPadding;

    obj["title"] = saveTextStyle(state.title);
    obj["body"] = saveTextStyle(state.body);
    return obj;
}

GraphComponentTheme loadComponentTheme(const QJsonObject& obj, bool isContainer = false)
{
    GraphComponentTheme theme;
    theme.minWidth = loadDouble(obj, "minWidth", isContainer ? 220.0 : 120.0);
    theme.minHeight = loadDouble(obj, "minHeight", isContainer ? 140.0 : 50.0);

    theme.normal = loadComponentState(obj.value("normal").toObject(), isContainer);
    theme.hover = loadComponentState(obj.value("hover").toObject(), isContainer);
    theme.selected = loadComponentState(obj.value("selected").toObject(), isContainer);
    return theme;
}

QJsonObject saveComponentTheme(const GraphComponentTheme& theme)
{
    QJsonObject obj;
    obj["minWidth"] = theme.minWidth;
    obj["minHeight"] = theme.minHeight;
    obj["normal"] = saveComponentState(theme.normal);
    obj["hover"] = saveComponentState(theme.hover);
    obj["selected"] = saveComponentState(theme.selected);
    return obj;
}

GraphArrowState loadArrowState(const QJsonObject& obj)
{
    GraphArrowState state;
    state.lineColor = loadColor(obj, "lineColor", "#ffffff");
    state.fillColor = loadColor(obj, "fillColor", "#ffffff");
    state.borderColor = loadColor(obj, "borderColor", "#000000");
    state.width = loadInt(obj, "width", 14);
    state.height = loadInt(obj, "height", 10);
    state.lineWidth = loadInt(obj, "lineWidth", 2);
    state.borderWidth = loadInt(obj, "borderWidth", 1);
    return state;
}

QJsonObject saveArrowState(const GraphArrowState& state)
{
    QJsonObject obj;
    obj["lineColor"] = state.lineColor.name(QColor::HexArgb);
    obj["fillColor"] = state.fillColor.name(QColor::HexArgb);
    obj["borderColor"] = state.borderColor.name(QColor::HexArgb);
    obj["width"] = state.width;
    obj["height"] = state.height;
    obj["lineWidth"] = state.lineWidth;
    obj["borderWidth"] = state.borderWidth;
    return obj;
}

GraphEdgeLabelState loadEdgeLabelState(const QJsonObject& obj)
{
    GraphEdgeLabelState state;
    state.textColor = loadColor(obj, "textColor", "#ffffff");
    state.backgroundColor = loadColor(obj, "backgroundColor", "#202020");
    state.borderColor = loadColor(obj, "borderColor", "#404040");
    state.borderWidth = loadInt(obj, "borderWidth", 1);
    state.fontSize = loadInt(obj, "fontSize", 11);
    state.bold = loadBool(obj, "bold", false);
    state.paddingX = loadInt(obj, "paddingX", 6);
    state.paddingY = loadInt(obj, "paddingY", 3);
    state.radius = loadInt(obj, "radius", 4);
    state.offset = loadInt(obj, "offset", 8);
    return state;
}

QJsonObject saveEdgeLabelState(const GraphEdgeLabelState& state)
{
    QJsonObject obj;
    obj["textColor"] = state.textColor.name(QColor::HexArgb);
    obj["backgroundColor"] = state.backgroundColor.name(QColor::HexArgb);
    obj["borderColor"] = state.borderColor.name(QColor::HexArgb);
    obj["borderWidth"] = state.borderWidth;
    obj["fontSize"] = state.fontSize;
    obj["bold"] = state.bold;
    obj["paddingX"] = state.paddingX;
    obj["paddingY"] = state.paddingY;
    obj["radius"] = state.radius;
    obj["offset"] = state.offset;
    return obj;
}

GraphEdgeState loadEdgeState(const QJsonObject& obj)
{
    GraphEdgeState state;
    state.lineColor = loadColor(obj, "lineColor", "#ffffff");
    state.lineWidth = loadInt(obj, "lineWidth", 2);

    // Support both the new "lineStyle" string and legacy "dashed" boolean
    state.dashed = loadBool(obj, "dashed", false);
    Qt::PenStyle fallbackStyle = state.dashed ? Qt::DashLine : Qt::SolidLine;
    state.lineStyle = loadPenStyle(obj, "lineStyle", fallbackStyle);

    if (obj.contains("dashPattern") && obj["dashPattern"].isArray())
    {
        for (const auto& val : obj["dashPattern"].toArray())
            state.dashPattern.append(val.toDouble(4.0));
    }

    state.arrow = loadArrowState(obj.value("arrow").toObject());
    state.label = loadEdgeLabelState(obj.value("label").toObject());
    return state;
}

QJsonObject saveEdgeState(const GraphEdgeState& state)
{
    QJsonObject obj;
    obj["lineColor"] = state.lineColor.name(QColor::HexArgb);
    obj["lineWidth"] = state.lineWidth;
    obj["lineStyle"] = savePenStyle(state.lineStyle);
    obj["dashed"] = (state.lineStyle == Qt::DashLine);

    if (!state.dashPattern.isEmpty())
    {
        QJsonArray arr;
        for (qreal v : state.dashPattern) arr.append(v);
        obj["dashPattern"] = arr;
    }

    obj["arrow"] = saveArrowState(state.arrow);
    obj["label"] = saveEdgeLabelState(state.label);
    return obj;
}

GraphPreviewLineTheme loadPreviewLineTheme(const QJsonObject& obj)
{
    GraphPreviewLineTheme preview;
    preview.color = loadColor(obj, "color", "#00b4d8");
    preview.width = loadInt(obj, "width", 2);
    preview.style = loadPenStyle(obj, "style", Qt::DashLine);
    return preview;
}

QJsonObject savePreviewLineTheme(const GraphPreviewLineTheme& preview)
{
    QJsonObject obj;
    obj["color"] = preview.color.name(QColor::HexArgb);
    obj["width"] = preview.width;
    obj["style"] = savePenStyle(preview.style);
    return obj;
}

} // anonymous namespace

GraphThemeManager::GraphThemeManager(QObject* parent)
    : QObject(parent)
{
    s_instance = this;
    initializeDefaults();
}

GraphThemeManager* GraphThemeManager::instance()
{
    return s_instance;
}

void GraphThemeManager::initializeDefaults()
{
    m_theme.name = "OpenArch Dark";

    m_theme.view.background = QColor("#202020");
    m_theme.view.grid.enabled = true;
    m_theme.view.grid.minorColor = QColor("#2a2a2a");
    m_theme.view.grid.majorColor = QColor("#353535");
    m_theme.view.grid.spacing = 20;
    m_theme.view.grid.majorSpacing = 100;
    m_theme.view.grid.lineWidth = 1;

    // Node defaults
    m_theme.node.minWidth = 120.0;
    m_theme.node.minHeight = 50.0;
    m_theme.node.normal.background = QColor(45, 45, 48, 255);
    m_theme.node.normal.border = QColor(106, 149, 255, 255);
    m_theme.node.normal.borderWidth = 2;
    m_theme.node.normal.borderStyle = Qt::SolidLine;
    m_theme.node.normal.radius = 8;
    m_theme.node.normal.padding = 8;
    m_theme.node.normal.title = {QColor("#ffffff"), 14, true, false, 5, 5, 5, 5, Qt::AlignCenter};
    m_theme.node.normal.body = {QColor("#d0d0d0"), 11, false, false, 5, 5, 5, 5, Qt::AlignCenter};

    m_theme.node.hover.background = QColor(53, 53, 58, 255);
    m_theme.node.hover.border = QColor(255, 255, 255, 255);
    m_theme.node.hover.borderWidth = 3;
    m_theme.node.hover.borderStyle = Qt::SolidLine;
    m_theme.node.hover.radius = 8;
    m_theme.node.hover.padding = 8;
    m_theme.node.hover.title = {QColor("#ffffff"), 14, true, false, 5, 5, 5, 5, Qt::AlignCenter};
    m_theme.node.hover.body = {QColor("#ffffff"), 11, false, false, 5, 5, 5, 5, Qt::AlignCenter};

    m_theme.node.selected.background = QColor(58, 53, 32, 255);
    m_theme.node.selected.border = QColor(255, 204, 0, 255);
    m_theme.node.selected.borderWidth = 4;
    m_theme.node.selected.borderStyle = Qt::SolidLine;
    m_theme.node.selected.radius = 8;
    m_theme.node.selected.padding = 8;
    m_theme.node.selected.title = {QColor("#ffcc00"), 14, true, false, 5, 5, 5, 5, Qt::AlignCenter};
    m_theme.node.selected.body = {QColor("#ffffff"), 11, false, false, 5, 5, 5, 5, Qt::AlignCenter};

    // Container defaults: Alpha embedded directly in QColor
    m_theme.container.minWidth = 220.0;
    m_theme.container.minHeight = 140.0;
    m_theme.container.normal.background = QColor(45, 45, 48, 45);
    m_theme.container.normal.border = QColor(106, 149, 255, 255);
    m_theme.container.normal.borderWidth = 2;
    m_theme.container.normal.borderStyle = Qt::DashLine;
    m_theme.container.normal.dashPattern = {6.0, 4.0};
    m_theme.container.normal.radius = 8;
    m_theme.container.normal.padding = 8;
    m_theme.container.normal.headerBackground = QColor(106, 149, 255, 35);
    m_theme.container.normal.headerHeightPadding = 12;
    m_theme.container.normal.title = {QColor("#ffffff"), 14, true, false, 5, 5, 5, 5, Qt::AlignLeft};

    m_theme.container.hover.background = QColor(53, 53, 58, 55);
    m_theme.container.hover.border = QColor(255, 255, 255, 255);
    m_theme.container.hover.borderWidth = 2;
    m_theme.container.hover.borderStyle = Qt::DashLine;
    m_theme.container.hover.dashPattern = {6.0, 4.0};
    m_theme.container.hover.radius = 8;
    m_theme.container.hover.padding = 8;
    m_theme.container.hover.headerBackground = QColor(255, 255, 255, 45);
    m_theme.container.hover.headerHeightPadding = 12;
    m_theme.container.hover.title = {QColor("#ffffff"), 14, true, false, 5, 5, 5, 5, Qt::AlignLeft};

    m_theme.container.selected.background = QColor(58, 53, 32, 60);
    m_theme.container.selected.border = QColor(255, 204, 0, 255);
    m_theme.container.selected.borderWidth = 3;
    m_theme.container.selected.borderStyle = Qt::DashLine;
    m_theme.container.selected.dashPattern = {6.0, 4.0};
    m_theme.container.selected.radius = 8;
    m_theme.container.selected.padding = 8;
    m_theme.container.selected.headerBackground = QColor(255, 204, 0, 50);
    m_theme.container.selected.headerHeightPadding = 12;
    m_theme.container.selected.title = {QColor("#ffcc00"), 14, true, false, 5, 5, 5, 5, Qt::AlignLeft};

    // Edge defaults
    m_theme.edge.preview.color = QColor(0, 180, 216);
    m_theme.edge.preview.width = 2;
    m_theme.edge.preview.style = Qt::DashLine;

    m_theme.edge.normal.lineColor = QColor("#7aa2f7");
    m_theme.edge.normal.lineWidth = 2;
    m_theme.edge.normal.arrow = {QColor("#7aa2f7"), QColor(122, 162, 247, 170), QColor("#000000"), 14, 10, 2, 1};
    m_theme.edge.normal.label = {QColor("#d0d0d0"), QColor(32, 32, 32, 0), QColor("#404040"), 1, 11, false, 0, 0, 4, 8};

    m_theme.edge.hover.lineColor = QColor("#ffffff");
    m_theme.edge.hover.lineWidth = 3;
    m_theme.edge.hover.arrow = {QColor("#ffffff"), QColor("#ffffff"), QColor("#000000"), 16, 12, 2, 1};
    m_theme.edge.hover.label = {QColor("#ffffff"), QColor("#2c2c2c"), QColor("#ffffff"), 1, 11, true, 6, 3, 4, 8};

    m_theme.edge.selected.lineColor = QColor("#ffcc00");
    m_theme.edge.selected.lineWidth = 4;
    m_theme.edge.selected.arrow = {QColor("#ffcc00"), QColor("#ffcc00"), QColor("#000000"), 18, 14, 3, 1};
    m_theme.edge.selected.label = {QColor("#ffcc00"), QColor("#3a3520"), QColor("#ffcc00"), 2, 11, true, 6, 3, 4, 8};

    // Port defaults
    m_theme.port.normal.inputColor = QColor("#4ec9b0");
    m_theme.port.normal.outputColor = QColor("#dcdcaa");
    m_theme.port.normal.hoverColor = QColor("#ffffff");
    m_theme.port.normal.radius = 6;

    // Selection defaults
    m_theme.selection.outline = QColor("#ffcc00");
    m_theme.selection.fill = QColor(255, 204, 0, 34);

    // Interaction defaults
    m_theme.interaction.hoverOutline = QColor("#ffffff");
    m_theme.interaction.invalidConnection = QColor("#ff4444");
    m_theme.interaction.dropTarget = QColor("#00ff88");
}

void GraphThemeManager::resetDefaults()
{
    initializeDefaults();
    emit themeChanged();
}

bool GraphThemeManager::load(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "Failed to open theme file:" << path;
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qDebug() << "Theme parse error:" << error.errorString();
        return false;
    }

    QJsonObject root = doc.object();
    m_theme.name = root.value("name").toString("Unnamed Theme");

    // View & Grid
    {
        QJsonObject viewObj = root.value("view").toObject();
        m_theme.view.background = loadColor(viewObj, "background", "#202020");

        QJsonObject gridObj = viewObj.value("grid").toObject();
        m_theme.view.grid.enabled = loadBool(gridObj, "enabled", true);

        QString minorKey = gridObj.contains("minorColor") ? "minorColor" : "color";
        m_theme.view.grid.minorColor = loadColor(gridObj, minorKey, "#2a2a2a");
        m_theme.view.grid.majorColor = loadColor(gridObj, "majorColor", "#353535");
        m_theme.view.grid.spacing = loadInt(gridObj, "spacing", 20);
        m_theme.view.grid.majorSpacing = loadInt(gridObj, "majorSpacing", 100);
        m_theme.view.grid.lineWidth = loadInt(gridObj, "lineWidth", 1);
    }

    if (root.contains("node"))
    {
        m_theme.node = loadComponentTheme(root.value("node").toObject(), false);
    }

    if (root.contains("container"))
    {
        m_theme.container = loadComponentTheme(root.value("container").toObject(), true);
    }

    // Edge Theme & Preview
    {
        QJsonObject edgeObj = root.value("edge").toObject();
        m_theme.edge.normal = loadEdgeState(edgeObj.value("normal").toObject());
        m_theme.edge.hover = loadEdgeState(edgeObj.value("hover").toObject());
        m_theme.edge.selected = loadEdgeState(edgeObj.value("selected").toObject());

        if (edgeObj.contains("preview"))
        {
            m_theme.edge.preview = loadPreviewLineTheme(edgeObj.value("preview").toObject());
        }
    }

    if (root.contains("port"))
    {
        QJsonObject portObj = root.value("port").toObject();
        m_theme.port.normal.inputColor = loadColor(portObj, "inputColor", "#4ec9b0");
        m_theme.port.normal.outputColor = loadColor(portObj, "outputColor", "#dcdcaa");
        m_theme.port.normal.hoverColor = loadColor(portObj, "hoverColor", "#ffffff");
        m_theme.port.normal.radius = loadInt(portObj, "radius", 6);
    }

    if (root.contains("selection"))
    {
        QJsonObject selObj = root.value("selection").toObject();
        m_theme.selection.outline = loadColor(selObj, "outline", "#ffcc00");
        m_theme.selection.fill = loadColor(selObj, "fill", "#ffcc0022");
    }

    if (root.contains("interaction"))
    {
        QJsonObject intObj = root.value("interaction").toObject();
        m_theme.interaction.hoverOutline = loadColor(intObj, "hoverOutline", "#ffffff");
        m_theme.interaction.invalidConnection = loadColor(intObj, "invalidConnection", "#ff4444");
        m_theme.interaction.dropTarget = loadColor(intObj, "dropTarget", "#00ff88");
    }

    emit themeChanged();
    return true;
}

bool GraphThemeManager::save(const QString& path) const
{
    QJsonObject root;
    root["name"] = m_theme.name;

    // View
    {
        QJsonObject viewObj;
        viewObj["background"] = m_theme.view.background.name(QColor::HexArgb);

        QJsonObject gridObj;
        gridObj["enabled"] = m_theme.view.grid.enabled;
        gridObj["minorColor"] = m_theme.view.grid.minorColor.name(QColor::HexArgb);
        gridObj["majorColor"] = m_theme.view.grid.majorColor.name(QColor::HexArgb);
        gridObj["spacing"] = m_theme.view.grid.spacing;
        gridObj["majorSpacing"] = m_theme.view.grid.majorSpacing;
        gridObj["lineWidth"] = m_theme.view.grid.lineWidth;
        viewObj["grid"] = gridObj;

        root["view"] = viewObj;
    }

    root["node"] = saveComponentTheme(m_theme.node);
    root["container"] = saveComponentTheme(m_theme.container);

    // Edge
    {
        QJsonObject edgeObj;
        edgeObj["normal"] = saveEdgeState(m_theme.edge.normal);
        edgeObj["hover"] = saveEdgeState(m_theme.edge.hover);
        edgeObj["selected"] = saveEdgeState(m_theme.edge.selected);
        edgeObj["preview"] = savePreviewLineTheme(m_theme.edge.preview);
        root["edge"] = edgeObj;
    }

    // Port
    {
        QJsonObject portObj;
        portObj["inputColor"] = m_theme.port.normal.inputColor.name(QColor::HexArgb);
        portObj["outputColor"] = m_theme.port.normal.outputColor.name(QColor::HexArgb);
        portObj["hoverColor"] = m_theme.port.normal.hoverColor.name(QColor::HexArgb);
        portObj["radius"] = m_theme.port.normal.radius;
        root["port"] = portObj;
    }

    // Selection
    {
        QJsonObject selObj;
        selObj["outline"] = m_theme.selection.outline.name(QColor::HexArgb);
        selObj["fill"] = m_theme.selection.fill.name(QColor::HexArgb);
        root["selection"] = selObj;
    }

    // Interaction
    {
        QJsonObject intObj;
        intObj["hoverOutline"] = m_theme.interaction.hoverOutline.name(QColor::HexArgb);
        intObj["invalidConnection"] = m_theme.interaction.invalidConnection.name(QColor::HexArgb);
        intObj["dropTarget"] = m_theme.interaction.dropTarget.name(QColor::HexArgb);
        root["interaction"] = intObj;
    }

    QJsonDocument doc(root);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

const GraphTheme& GraphThemeManager::theme() const
{
    return m_theme;
}

GraphTheme& GraphThemeManager::mutableTheme()
{
    return m_theme;
}

void GraphThemeManager::notifyThemeChanged()
{
    emit themeChanged();
}