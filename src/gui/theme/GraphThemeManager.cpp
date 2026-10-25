#include "GraphThemeManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

GraphThemeManager* GraphThemeManager::s_instance = nullptr;

namespace
{

QColor parseHexColor(const QString& str, const QString& fallback = "#ffffffff")
{
    QString s = str.trimmed();
    if (s.startsWith('#'))
    {
        if (s.length() == 9)
        {
            // Format: #RRGGBBAA
            bool okR = false, okG = false, okB = false, okA = false;
            int r = s.mid(1, 2).toInt(&okR, 16);
            int g = s.mid(3, 2).toInt(&okG, 16);
            int b = s.mid(5, 2).toInt(&okB, 16);
            int a = s.mid(7, 2).toInt(&okA, 16);
            if (okR && okG && okB && okA)
                return QColor(r, g, b, a);
        }
        else if (s.length() == 7)
        {
            // Format: #RRGGBB (full opacity)
            bool okR = false, okG = false, okB = false;
            int r = s.mid(1, 2).toInt(&okR, 16);
            int g = s.mid(3, 2).toInt(&okG, 16);
            int b = s.mid(5, 2).toInt(&okB, 16);
            if (okR && okG && okB)
                return QColor(r, g, b, 255);
        }
    }

    QColor c(s);
    if (c.isValid())
        return c;
    return QColor(fallback);
}

QColor loadColor(const QJsonObject& obj, const QString& key, const QString& fallback = "#ffffffff")
{
    return parseHexColor(obj.value(key).toString(fallback), fallback);
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

QString saveColor(const QColor& color)
{
    return QString("#%1%2%3%4")
        .arg(color.red(), 2, 16, QChar('0'))
        .arg(color.green(), 2, 16, QChar('0'))
        .arg(color.blue(), 2, 16, QChar('0'))
        .arg(color.alpha(), 2, 16, QChar('0')).toLower();
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
    obj["color"] = saveColor(style.color);
    obj["size"] = style.size;
    obj["bold"] = style.bold;
    if (style.italic)
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
    if (obj.contains("body"))
    {
        state.body = loadTextStyle(obj.value("body").toObject());
    }
    else
    {
        state.body.color = QColor();
        state.body.size = 11;
        state.body.bold = false;
        state.body.italic = false;
        state.body.align = Qt::AlignCenter;
    }

    if (obj.contains("tertiary"))
    {
        state.tertiary = loadTextStyle(obj.value("tertiary").toObject());
    }
    else
    {
        state.tertiary = state.body;
        state.tertiary.size = std::max(7, (state.body.size > 0 ? state.body.size : 11) - 2);
        state.tertiary.italic = true;
        if (state.body.color.isValid())
        {
            QColor tc = state.body.color;
            tc.setAlpha(std::min(160, tc.alpha()));
            state.tertiary.color = tc;
        }
        else
        {
            state.tertiary.color = QColor();
        }
    }
    return state;
}

QJsonObject saveComponentState(const GraphComponentState& state, bool isContainer = false)
{
    QJsonObject obj;
    obj["background"] = saveColor(state.background);
    obj["border"] = saveColor(state.border);
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

    if (isContainer)
    {
        obj["headerBackground"] = saveColor(state.headerBackground);
        obj["headerHeightPadding"] = state.headerHeightPadding;
        obj["title"] = saveTextStyle(state.title);
        obj["body"] = saveTextStyle(state.body);
        obj["tertiary"] = saveTextStyle(state.tertiary);
    }
    else
    {
        obj["title"] = saveTextStyle(state.title);
        obj["body"] = saveTextStyle(state.body);
        obj["tertiary"] = saveTextStyle(state.tertiary);
    }
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

QJsonObject saveComponentTheme(const GraphComponentTheme& theme, bool isContainer = false)
{
    QJsonObject obj;
    obj["minWidth"] = theme.minWidth;
    obj["minHeight"] = theme.minHeight;
    obj["normal"] = saveComponentState(theme.normal, isContainer);
    obj["hover"] = saveComponentState(theme.hover, isContainer);
    obj["selected"] = saveComponentState(theme.selected, isContainer);
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
    obj["lineColor"] = saveColor(state.lineColor);
    obj["fillColor"] = saveColor(state.fillColor);
    obj["width"] = state.width;
    obj["height"] = state.height;
    obj["lineWidth"] = state.lineWidth;
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
    obj["textColor"] = saveColor(state.textColor);
    obj["backgroundColor"] = saveColor(state.backgroundColor);
    obj["borderColor"] = saveColor(state.borderColor);
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
    obj["lineColor"] = saveColor(state.lineColor);
    obj["lineWidth"] = state.lineWidth;
    if (state.lineStyle != Qt::SolidLine)
    {
        obj["lineStyle"] = savePenStyle(state.lineStyle);
    }

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
    if (obj.contains("lineColor"))
        preview.color = loadColor(obj, "lineColor", "#00b4d8");
    else
        preview.color = loadColor(obj, "color", "#00b4d8");

    if (obj.contains("lineWidth"))
        preview.width = loadInt(obj, "lineWidth", 2);
    else
        preview.width = loadInt(obj, "width", 2);

    if (obj.contains("lineStyle"))
        preview.style = loadPenStyle(obj, "lineStyle", Qt::DashLine);
    else
        preview.style = loadPenStyle(obj, "style", Qt::DashLine);

    if (obj.contains("dashPattern") && obj["dashPattern"].isArray())
    {
        preview.dashPattern.clear();
        for (const auto& v : obj["dashPattern"].toArray())
        {
            preview.dashPattern.append(v.toDouble());
        }
    }

    if (obj.contains("arrow") && obj["arrow"].isObject())
    {
        preview.arrow = loadArrowState(obj["arrow"].toObject());
    }
    else
    {
        preview.arrow.lineColor = preview.color;
        preview.arrow.fillColor = preview.color;
    }

    return preview;
}

QJsonObject savePreviewLineTheme(const GraphPreviewLineTheme& preview)
{
    QJsonObject obj;
    obj["color"] = saveColor(preview.color);
    obj["width"] = preview.width;
    obj["style"] = savePenStyle(preview.style);
    if (!preview.dashPattern.isEmpty())
    {
        QJsonArray arr;
        for (qreal v : preview.dashPattern)
            arr.append(v);
        obj["dashPattern"] = arr;
    }
    obj["arrow"] = saveArrowState(preview.arrow);
    return obj;
}

void mergeEdgeState(GraphEdgeState& state, const QJsonObject& obj)
{
    if (obj.contains("lineColor"))
        state.lineColor = loadColor(obj, "lineColor", state.lineColor.name(QColor::HexArgb));
    else if (obj.contains("color"))
        state.lineColor = loadColor(obj, "color", state.lineColor.name(QColor::HexArgb));

    if (obj.contains("lineWidth"))
        state.lineWidth = loadInt(obj, "lineWidth", state.lineWidth);
    else if (obj.contains("width"))
        state.lineWidth = loadInt(obj, "width", state.lineWidth);

    if (obj.contains("lineStyle"))
        state.lineStyle = loadPenStyle(obj, "lineStyle", state.lineStyle);
    else if (obj.contains("style"))
        state.lineStyle = loadPenStyle(obj, "style", state.lineStyle);

    if (obj.contains("dashed"))
    {
        state.dashed = loadBool(obj, "dashed", state.dashed);
        state.lineStyle = state.dashed ? Qt::DashLine : Qt::SolidLine;
    }

    if (obj.contains("dashPattern") && obj["dashPattern"].isArray())
    {
        state.dashPattern.clear();
        for (const auto& val : obj["dashPattern"].toArray())
            state.dashPattern.append(val.toDouble(4.0));
    }

    if (obj.contains("arrow") && obj["arrow"].isObject())
    {
        QJsonObject arrowObj = obj["arrow"].toObject();
        if (arrowObj.contains("lineColor"))
            state.arrow.lineColor = loadColor(arrowObj, "lineColor", state.arrow.lineColor.name(QColor::HexArgb));
        if (arrowObj.contains("fillColor"))
            state.arrow.fillColor = loadColor(arrowObj, "fillColor", state.arrow.fillColor.name(QColor::HexArgb));
        if (arrowObj.contains("borderColor"))
            state.arrow.borderColor = loadColor(arrowObj, "borderColor", state.arrow.borderColor.name(QColor::HexArgb));
        if (arrowObj.contains("width"))
            state.arrow.width = loadInt(arrowObj, "width", state.arrow.width);
        if (arrowObj.contains("height"))
            state.arrow.height = loadInt(arrowObj, "height", state.arrow.height);
        if (arrowObj.contains("lineWidth"))
            state.arrow.lineWidth = loadInt(arrowObj, "lineWidth", state.arrow.lineWidth);
        if (arrowObj.contains("borderWidth"))
            state.arrow.borderWidth = loadInt(arrowObj, "borderWidth", state.arrow.borderWidth);
    }
    else if (obj.contains("lineColor") || obj.contains("color"))
    {
        state.arrow.lineColor = state.lineColor;
        state.arrow.fillColor = state.lineColor;
    }

    if (obj.contains("label") && obj["label"].isObject())
    {
        QJsonObject labelObj = obj["label"].toObject();
        if (labelObj.contains("textColor"))
            state.label.textColor = loadColor(labelObj, "textColor", state.label.textColor.name(QColor::HexArgb));
        if (labelObj.contains("backgroundColor"))
            state.label.backgroundColor = loadColor(labelObj, "backgroundColor", state.label.backgroundColor.name(QColor::HexArgb));
        if (labelObj.contains("borderColor"))
            state.label.borderColor = loadColor(labelObj, "borderColor", state.label.borderColor.name(QColor::HexArgb));
        if (labelObj.contains("borderWidth"))
            state.label.borderWidth = loadInt(labelObj, "borderWidth", state.label.borderWidth);
        if (labelObj.contains("fontSize"))
            state.label.fontSize = loadInt(labelObj, "fontSize", state.label.fontSize);
        if (labelObj.contains("bold"))
            state.label.bold = loadBool(labelObj, "bold", state.label.bold);
        if (labelObj.contains("paddingX"))
            state.label.paddingX = loadInt(labelObj, "paddingX", state.label.paddingX);
        if (labelObj.contains("paddingY"))
            state.label.paddingY = loadInt(labelObj, "paddingY", state.label.paddingY);
        if (labelObj.contains("radius"))
            state.label.radius = loadInt(labelObj, "radius", state.label.radius);
        if (labelObj.contains("offset"))
            state.label.offset = loadInt(labelObj, "offset", state.label.offset);
    }
}

GraphEdgeTheme loadEdgeThemeOverride(const GraphEdgeTheme& baseTheme, const QJsonObject& obj)
{
    GraphEdgeTheme theme = baseTheme;

    if (obj.contains("lineColor") || obj.contains("color") || obj.contains("lineWidth") ||
        obj.contains("width") || obj.contains("lineStyle") || obj.contains("style") ||
        obj.contains("dashed") || obj.contains("dashPattern") || obj.contains("arrow") ||
        obj.contains("label"))
    {
        mergeEdgeState(theme.normal, obj);
        mergeEdgeState(theme.hover, obj);
        if (obj.contains("lineWidth") || obj.contains("width"))
            theme.selected.lineWidth = std::max(theme.normal.lineWidth + 1, theme.selected.lineWidth);
        if (obj.contains("lineStyle") || obj.contains("style") || obj.contains("dashed"))
            theme.selected.lineStyle = theme.normal.lineStyle;
    }

    if (obj.contains("normal") && obj["normal"].isObject())
    {
        mergeEdgeState(theme.normal, obj["normal"].toObject());
    }
    if (obj.contains("hover") && obj["hover"].isObject())
    {
        mergeEdgeState(theme.hover, obj["hover"].toObject());
    }
    if (obj.contains("selected") && obj["selected"].isObject())
    {
        mergeEdgeState(theme.selected, obj["selected"].toObject());
    }
    if (obj.contains("preview") && obj["preview"].isObject())
    {
        theme.preview = loadPreviewLineTheme(obj["preview"].toObject());
    }

    return theme;
}

void mergeComponentState(GraphComponentState& state, const QJsonObject& obj, bool isContainer = false)
{
    if (obj.contains("background"))
        state.background = loadColor(obj, "background", state.background.name(QColor::HexArgb));
    if (obj.contains("backgroundAlpha"))
        state.background.setAlpha(obj["backgroundAlpha"].toInt(state.background.alpha()));

    if (obj.contains("border"))
        state.border = loadColor(obj, "border", state.border.name(QColor::HexArgb));
    if (obj.contains("borderColor"))
        state.border = loadColor(obj, "borderColor", state.border.name(QColor::HexArgb));

    if (obj.contains("borderWidth"))
        state.borderWidth = loadInt(obj, "borderWidth", state.borderWidth);
    if (obj.contains("borderStyle"))
        state.borderStyle = loadPenStyle(obj, "borderStyle", state.borderStyle);

    if (obj.contains("dashPattern") && obj["dashPattern"].isArray())
    {
        state.dashPattern.clear();
        for (const auto& val : obj["dashPattern"].toArray())
            state.dashPattern.append(val.toDouble(4.0));
    }

    if (obj.contains("radius"))
        state.radius = loadInt(obj, "radius", state.radius);
    if (obj.contains("padding"))
        state.padding = loadInt(obj, "padding", state.padding);

    if (obj.contains("headerBackground"))
        state.headerBackground = loadColor(obj, "headerBackground", state.headerBackground.name(QColor::HexArgb));
    if (obj.contains("headerAlpha"))
        state.headerBackground.setAlpha(obj["headerAlpha"].toInt(state.headerBackground.alpha()));
    if (obj.contains("headerHeightPadding"))
        state.headerHeightPadding = loadInt(obj, "headerHeightPadding", state.headerHeightPadding);

    if (obj.contains("title") && obj["title"].isObject())
    {
        QJsonObject tObj = obj["title"].toObject();
        if (tObj.contains("color"))
            state.title.color = loadColor(tObj, "color", state.title.color.name(QColor::HexArgb));
        if (tObj.contains("size"))
            state.title.size = loadInt(tObj, "size", state.title.size);
        if (tObj.contains("bold"))
            state.title.bold = loadBool(tObj, "bold", state.title.bold);
        if (tObj.contains("italic"))
            state.title.italic = loadBool(tObj, "italic", state.title.italic);
        if (tObj.contains("align"))
            state.title.align = loadAlign(tObj, "align", state.title.align);
    }

    if (obj.contains("body") && obj["body"].isObject())
    {
        QJsonObject bObj = obj["body"].toObject();
        if (bObj.contains("color"))
            state.body.color = loadColor(bObj, "color", state.body.color.name(QColor::HexArgb));
        if (bObj.contains("size"))
            state.body.size = loadInt(bObj, "size", state.body.size);
        if (bObj.contains("bold"))
            state.body.bold = loadBool(bObj, "bold", state.body.bold);
        if (bObj.contains("italic"))
            state.body.italic = loadBool(bObj, "italic", state.body.italic);
        if (bObj.contains("align"))
            state.body.align = loadAlign(bObj, "align", state.body.align);
    }

    if (obj.contains("tertiary") && obj["tertiary"].isObject())
    {
        QJsonObject tertObj = obj["tertiary"].toObject();
        if (tertObj.contains("color"))
            state.tertiary.color = loadColor(tertObj, "color", state.tertiary.color.name(QColor::HexArgb));
        if (tertObj.contains("size"))
            state.tertiary.size = loadInt(tertObj, "size", state.tertiary.size);
        if (tertObj.contains("bold"))
            state.tertiary.bold = loadBool(tertObj, "bold", state.tertiary.bold);
        if (tertObj.contains("italic"))
            state.tertiary.italic = loadBool(tertObj, "italic", state.tertiary.italic);
        if (tertObj.contains("align"))
            state.tertiary.align = loadAlign(tertObj, "align", state.tertiary.align);
    }
}

GraphComponentTheme loadComponentThemeOverride(const GraphComponentTheme& baseTheme, const QJsonObject& obj, bool isContainer = false)
{
    GraphComponentTheme theme = baseTheme;

    if (obj.contains("minWidth"))
        theme.minWidth = loadDouble(obj, "minWidth", theme.minWidth);
    if (obj.contains("minHeight"))
        theme.minHeight = loadDouble(obj, "minHeight", theme.minHeight);

    if (obj.contains("background") || obj.contains("backgroundAlpha") ||
        obj.contains("border") || obj.contains("borderColor") ||
        obj.contains("borderWidth") || obj.contains("borderStyle") ||
        obj.contains("dashPattern") || obj.contains("radius") || obj.contains("padding") ||
        obj.contains("headerBackground") || obj.contains("headerAlpha") ||
        obj.contains("headerHeightPadding") || obj.contains("title") ||
        obj.contains("body") || obj.contains("tertiary"))
    {
        mergeComponentState(theme.normal, obj, isContainer);
        if (!obj.contains("hover"))
        {
            mergeComponentState(theme.hover, obj, isContainer);
            if (!obj.contains("border") && !obj.contains("borderColor"))
            {
                theme.hover.border = baseTheme.hover.border;
            }
        }
    }

    if (obj.contains("normal") && obj["normal"].isObject())
    {
        mergeComponentState(theme.normal, obj["normal"].toObject(), isContainer);
    }
    if (obj.contains("hover") && obj["hover"].isObject())
    {
        mergeComponentState(theme.hover, obj["hover"].toObject(), isContainer);
    }
    if (obj.contains("selected") && obj["selected"].isObject())
    {
        mergeComponentState(theme.selected, obj["selected"].toObject(), isContainer);
    }

    return theme;
}

} // anonymous namespace

GraphThemeManager::GraphThemeManager(QObject* parent)
    : QObject(parent)
{
    s_instance = this;
    initializeDefaults();
}

GraphThemeManager::~GraphThemeManager()
{
    if (s_instance == this)
    {
        s_instance = nullptr;
    }
}

GraphThemeManager* GraphThemeManager::instance()
{
    if (!s_instance)
    {
        s_instance = new GraphThemeManager(qApp);
    }
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
    m_theme.node.normal.tertiary = {QColor(160, 160, 160, 170), 9, false, true, 5, 5, 5, 5, Qt::AlignCenter};

    m_theme.node.hover.background = QColor(53, 53, 58, 255);
    m_theme.node.hover.border = QColor(255, 255, 255, 255);
    m_theme.node.hover.borderWidth = 3;
    m_theme.node.hover.borderStyle = Qt::SolidLine;
    m_theme.node.hover.radius = 8;
    m_theme.node.hover.padding = 8;
    m_theme.node.hover.title = {QColor("#ffffff"), 14, true, false, 5, 5, 5, 5, Qt::AlignCenter};
    m_theme.node.hover.body = {QColor("#ffffff"), 11, false, false, 5, 5, 5, 5, Qt::AlignCenter};
    m_theme.node.hover.tertiary = {QColor(208, 208, 208, 220), 9, false, true, 5, 5, 5, 5, Qt::AlignCenter};

    m_theme.node.selected.background = QColor(58, 53, 32, 255);
    m_theme.node.selected.border = QColor(255, 204, 0, 255);
    m_theme.node.selected.borderWidth = 4;
    m_theme.node.selected.borderStyle = Qt::SolidLine;
    m_theme.node.selected.radius = 8;
    m_theme.node.selected.padding = 8;
    m_theme.node.selected.title = {QColor("#ffcc00"), 14, true, false, 5, 5, 5, 5, Qt::AlignCenter};
    m_theme.node.selected.body = {QColor("#ffffff"), 11, false, false, 5, 5, 5, 5, Qt::AlignCenter};
    m_theme.node.selected.tertiary = {QColor(255, 204, 0, 180), 9, false, true, 5, 5, 5, 5, Qt::AlignCenter};

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
    m_theme.container.normal.body = {QColor("#d0d0d0"), 11, false, false, 5, 5, 5, 5, Qt::AlignCenter};
    m_theme.container.normal.tertiary = {QColor(160, 160, 160, 170), 9, false, true, 5, 5, 5, 5, Qt::AlignCenter};

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
    m_theme.container.hover.body = {QColor("#ffffff"), 11, false, false, 5, 5, 5, 5, Qt::AlignCenter};
    m_theme.container.hover.tertiary = {QColor(208, 208, 208, 220), 9, false, true, 5, 5, 5, 5, Qt::AlignCenter};

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
    m_theme.container.selected.body = {QColor("#ffcc00"), 11, false, false, 5, 5, 5, 5, Qt::AlignCenter};
    m_theme.container.selected.tertiary = {QColor(255, 204, 0, 180), 9, false, true, 5, 5, 5, 5, Qt::AlignCenter};

    // Edge defaults
    m_theme.edge.preview.color = QColor(0, 180, 216);
    m_theme.edge.preview.width = 2;
    m_theme.edge.preview.style = Qt::DashLine;
    m_theme.edge.preview.arrow = {QColor(0, 180, 216), QColor(0, 180, 216), QColor("#000000"), 14, 10, 2, 1};

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

    m_theme.edgeTypeThemes.clear();
    m_theme.nodeTypeThemes.clear();
    m_theme.containerTypeThemes.clear();
}

void GraphThemeManager::resetDefaults()
{
    m_currentPath.clear();
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
    m_currentPath = path;

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

    // Load type-specific overrides (edge#<type>, node#<type>, container#<type>)
    m_theme.edgeTypeThemes.clear();
    m_theme.nodeTypeThemes.clear();
    m_theme.containerTypeThemes.clear();

    for (auto it = root.begin(); it != root.end(); ++it)
    {
        const QString key = it.key();
        if (!it.value().isObject())
            continue;

        QJsonObject obj = it.value().toObject();

        if (key.startsWith("edge#", Qt::CaseInsensitive))
        {
            QString typeName = key.mid(5).trimmed().toLower();
            if (!typeName.isEmpty())
            {
                m_theme.edgeTypeThemes[typeName] = loadEdgeThemeOverride(m_theme.edge, obj);
            }
        }
        else if (key.startsWith("node#", Qt::CaseInsensitive))
        {
            QString typeName = key.mid(5).trimmed().toLower();
            if (!typeName.isEmpty())
            {
                m_theme.nodeTypeThemes[typeName] = loadComponentThemeOverride(m_theme.node, obj, false);
            }
        }
        else if (key.startsWith("container#", Qt::CaseInsensitive))
        {
            QString typeName = key.mid(10).trimmed().toLower();
            if (!typeName.isEmpty())
            {
                m_theme.containerTypeThemes[typeName] = loadComponentThemeOverride(m_theme.container, obj, true);
            }
        }
    }

    emit themeLoaded(path);
    emit themeChanged();
    return true;
}

bool GraphThemeManager::save()
{
    if (m_currentPath.isEmpty())
        return false;
    return save(m_currentPath);
}

bool GraphThemeManager::save(const QString& path)
{
    if (path.isEmpty())
        return false;

    if (m_theme.name.isEmpty()) {
        m_theme.name = QFileInfo(path).baseName();
    }

    QJsonObject root;
    root["name"] = m_theme.name;

    // View
    {
        QJsonObject viewObj;
        viewObj["background"] = saveColor(m_theme.view.background);

        QJsonObject gridObj;
        gridObj["enabled"] = m_theme.view.grid.enabled;
        gridObj["minorColor"] = saveColor(m_theme.view.grid.minorColor);
        gridObj["majorColor"] = saveColor(m_theme.view.grid.majorColor);
        gridObj["spacing"] = m_theme.view.grid.spacing;
        gridObj["majorSpacing"] = m_theme.view.grid.majorSpacing;
        gridObj["lineWidth"] = m_theme.view.grid.lineWidth;
        viewObj["grid"] = gridObj;

        root["view"] = viewObj;
    }

    root["node"] = saveComponentTheme(m_theme.node, false);
    root["container"] = saveComponentTheme(m_theme.container, true);

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
        portObj["inputColor"] = saveColor(m_theme.port.normal.inputColor);
        portObj["outputColor"] = saveColor(m_theme.port.normal.outputColor);
        portObj["hoverColor"] = saveColor(m_theme.port.normal.hoverColor);
        portObj["radius"] = m_theme.port.normal.radius;
        root["port"] = portObj;
    }

    // Selection
    {
        QJsonObject selObj;
        selObj["outline"] = saveColor(m_theme.selection.outline);
        selObj["fill"] = saveColor(m_theme.selection.fill);
        root["selection"] = selObj;
    }

    // Interaction
    {
        QJsonObject intObj;
        intObj["hoverOutline"] = saveColor(m_theme.interaction.hoverOutline);
        intObj["invalidConnection"] = saveColor(m_theme.interaction.invalidConnection);
        intObj["dropTarget"] = saveColor(m_theme.interaction.dropTarget);
        root["interaction"] = intObj;
    }

    // Save type-specific overrides (node#<type>, container#<type>, edge#<type>)
    for (auto it = m_theme.nodeTypeThemes.begin(); it != m_theme.nodeTypeThemes.end(); ++it)
    {
        root["node#" + it.key()] = saveComponentTheme(it.value(), false);
    }
    for (auto it = m_theme.containerTypeThemes.begin(); it != m_theme.containerTypeThemes.end(); ++it)
    {
        root["container#" + it.key()] = saveComponentTheme(it.value(), true);
    }
    for (auto it = m_theme.edgeTypeThemes.begin(); it != m_theme.edgeTypeThemes.end(); ++it)
    {
        QJsonObject edgeObj;
        edgeObj["normal"] = saveEdgeState(it.value().normal);
        edgeObj["hover"] = saveEdgeState(it.value().hover);
        edgeObj["selected"] = saveEdgeState(it.value().selected);
        edgeObj["preview"] = savePreviewLineTheme(it.value().preview);
        root["edge#" + it.key()] = edgeObj;
    }

    QJsonDocument doc(root);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(doc.toJson(QJsonDocument::Indented));
    m_currentPath = path;
    emit themeSaved(path);
    return true;
}

const QString& GraphThemeManager::currentThemePath() const
{
    return m_currentPath;
}

void GraphThemeManager::setCurrentThemePath(const QString& path)
{
    m_currentPath = path;
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