#include "ThemeEditorDock.h"

#include "GraphTheme.h"
#include "GraphThemeManager.h"

#include <QTreeWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QToolButton>
#include <QColorDialog>
#include <QScrollArea>
#include <QFrame>
#include <QVector>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

/*
 * =========================================================
 * PREVIEW WIDGETS (LIFETIME SAFE: NO REFERENCE MEMBERS)
 * =========================================================
 */

enum class ComponentStateKind { Normal, Hover, Selected };

class ComponentPreviewWidget : public QWidget
{
public:
    ComponentPreviewWidget(ComponentStateKind kind, bool isContainer, QWidget* parent = nullptr)
        : QWidget(parent), m_kind(kind), m_isContainer(isContainer)
    {
        setFixedHeight(110);
    }

    void refresh() { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        p.fillRect(rect(), QColor("#1a1a1a"));

        const auto& theme = GraphThemeManager::instance()->theme();
        const auto& group = m_isContainer ? theme.container : theme.node;
        const GraphComponentState* state = &group.normal;
        if (m_kind == ComponentStateKind::Hover) state = &group.hover;
        else if (m_kind == ComponentStateKind::Selected) state = &group.selected;

        QRectF card = rect().adjusted(15, 12, -15, -12);

        QPainterPath bodyPath;
        bodyPath.addRoundedRect(card, state->radius, state->radius);

        QPen pen(state->border, state->borderWidth, state->borderStyle);
        if (state->borderStyle == Qt::CustomDashLine && !state->dashPattern.isEmpty())
            pen.setDashPattern(state->dashPattern);

        p.setPen(pen);
        p.setBrush(state->background);
        p.drawPath(bodyPath);

        if (m_isContainer)
        {
            QFont titleFont;
            titleFont.setPointSize(state->title.size > 0 ? state->title.size : 12);
            titleFont.setBold(state->title.bold);
            titleFont.setItalic(state->title.italic);

            QFontMetrics fm(titleFont);
            qreal headerHeight = fm.height() + state->headerHeightPadding;

            QRectF headerRect(card.left(), card.top(), card.width(), headerHeight);
            QPainterPath hPath;
            hPath.addRoundedRect(headerRect, state->radius, state->radius);

            p.setPen(Qt::NoPen);
            p.setBrush(state->headerBackground);
            p.drawPath(hPath);

            p.setFont(titleFont);
            p.setPen(state->title.color);
            p.drawText(headerRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, "Container Title");
        }
        else
        {
            QFont titleFont;
            titleFont.setPointSize(state->title.size > 0 ? state->title.size : 12);
            titleFont.setBold(state->title.bold);
            titleFont.setItalic(state->title.italic);

            QFont bodyFont;
            bodyFont.setPointSize(state->body.size > 0 ? state->body.size : 10);
            bodyFont.setBold(state->body.bold);
            bodyFont.setItalic(state->body.italic);

            QFontMetrics titleFm(titleFont);
            qreal titleH = titleFm.height();

            QRectF titleRect(card.left(), card.top() + state->padding, card.width(), titleH);
            QRectF bodyRect(card.left(), titleRect.bottom() + 2, card.width(), card.bottom() - titleRect.bottom() - state->padding);

            p.setFont(titleFont);
            p.setPen(state->title.color);
            p.drawText(titleRect, state->title.align, "Node Title");

            p.setFont(bodyFont);
            p.setPen(state->body.color);
            p.drawText(bodyRect, state->body.align, "Type: service");
        }
    }

private:
    ComponentStateKind m_kind;
    bool m_isContainer;
};

class EdgePreviewWidget : public QWidget
{
public:
    EdgePreviewWidget(ComponentStateKind kind, QWidget* parent = nullptr)
        : QWidget(parent), m_kind(kind)
    {
        setFixedHeight(80);
    }

    void refresh() { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        p.fillRect(rect(), QColor("#1a1a1a"));

        const auto& theme = GraphThemeManager::instance()->theme();
        const GraphEdgeState* state = &theme.edge.normal;
        if (m_kind == ComponentStateKind::Hover) state = &theme.edge.hover;
        else if (m_kind == ComponentStateKind::Selected) state = &theme.edge.selected;

        qreal midY = height() / 2.0;
        QPointF start(20, midY);
        QPointF end(width() - 35, midY);

        QPen pen(state->lineColor, state->lineWidth);
        pen.setStyle(state->lineStyle);
        if (state->lineStyle == Qt::CustomDashLine && !state->dashPattern.isEmpty())
            pen.setDashPattern(state->dashPattern);

        p.setPen(pen);
        p.drawLine(start, end);

        // Arrow
        const auto& arrow = state->arrow;
        QPolygonF head;
        head << QPointF(width() - 20, midY)
             << QPointF(width() - 20 - arrow.width, midY - arrow.height / 2.0)
             << QPointF(width() - 20 - arrow.width, midY + arrow.height / 2.0);

        QPen arrowPen(arrow.lineColor, arrow.lineWidth);
        p.setPen(arrowPen);
        p.setBrush(arrow.fillColor);
        p.drawPolygon(head);

        // Label Badge anchored above line
        const auto& lbl = state->label;
        QFont font;
        font.setPointSize(lbl.fontSize > 0 ? lbl.fontSize : 10);
        font.setBold(lbl.bold);
        p.setFont(font);

        QString text = "sample: edge";
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(text);

        qreal badgeHalfHeight = (textRect.height() / 2.0) + lbl.paddingY;
        qreal verticalDistance = (state->lineWidth / 2.0) + lbl.offset + badgeHalfHeight;

        QRect badgeRect = textRect.adjusted(-lbl.paddingX, -lbl.paddingY, lbl.paddingX, lbl.paddingY);
        badgeRect.moveCenter(QPoint(width() / 2, static_cast<int>(midY - verticalDistance)));

        p.setPen(QPen(lbl.borderColor, lbl.borderWidth));
        p.setBrush(lbl.backgroundColor);
        p.drawRoundedRect(badgeRect, lbl.radius, lbl.radius);

        p.setPen(lbl.textColor);
        p.drawText(badgeRect, Qt::AlignCenter, text);
    }

private:
    ComponentStateKind m_kind;
};

class PreviewLinePreviewWidget : public QWidget
{
public:
    PreviewLinePreviewWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(60);
    }

    void refresh() { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        p.fillRect(rect(), QColor("#1a1a1a"));

        const auto& theme = GraphThemeManager::instance()->theme().edge.preview;

        qreal midY = height() / 2.0;
        QPen pen(theme.color, theme.width, theme.style, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawLine(QPointF(20, midY), QPointF(width() - 20, midY));
    }
};

/*
 * =========================================================
 * ENTRIES
 * =========================================================
 */

struct ComponentStateEntry
{
    QString name;
    ComponentStateKind kind;
    GraphComponentState* state{nullptr};

    ComponentStateEntry(const QString& n, ComponentStateKind k, GraphComponentState* s)
        : name(n), kind(k), state(s) {}
};

struct EdgeStateEntry
{
    QString name;
    ComponentStateKind kind;
    GraphEdgeState* state{nullptr};

    EdgeStateEntry(const QString& n, ComponentStateKind k, GraphEdgeState* s)
        : name(n), kind(k), state(s) {}
};

struct PortStateEntry
{
    QString name;
    GraphPortState* state{nullptr};

    PortStateEntry(const QString& n, GraphPortState* s)
        : name(n), state(s) {}
};

ThemeEditorDock::ThemeEditorDock(QWidget* parent)
    : QDockWidget(parent)
{
    setWindowTitle("Theme Editor");

    QWidget* root = new QWidget;
    auto* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_tree = new QTreeWidget;
    m_tree->setHeaderHidden(true);
    m_tree->setMinimumWidth(240);

    m_stack = new QStackedWidget;

    rootLayout->addWidget(m_tree);
    rootLayout->addWidget(m_stack, 1);

    setWidget(root);

    populateTree();
    connectTree();

    connect(GraphThemeManager::instance(), &GraphThemeManager::themeChanged,
            this, &ThemeEditorDock::syncFromTheme);
}

ThemeEditorDock::InspectorPage ThemeEditorDock::createInspectorPage()
{
    InspectorPage page;
    page.content = new QWidget;
    page.layout = new QVBoxLayout(page.content);
    page.layout->setAlignment(Qt::AlignTop);

    QScrollArea* scroll = new QScrollArea;
    scroll->setWidget(page.content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    page.container = new QWidget;
    auto* layout = new QVBoxLayout(page.container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    return page;
}

void ThemeEditorDock::populateTree()
{
    auto& theme = GraphThemeManager::instance()->mutableTheme();

    auto addPage = [&](QTreeWidgetItem* item, auto builder)
    {
        auto page = createInspectorPage();
        builder(page.layout);
        int index = m_stack->addWidget(page.container);
        item->setData(0, Qt::UserRole, index);
    };

    // 1. View
    auto* viewItem = new QTreeWidgetItem(QStringList() << "View");
    m_tree->addTopLevelItem(viewItem);
    addPage(viewItem, [&](QVBoxLayout* layout) {
        buildViewProperties(layout);
    });

    // 2. Grid
    auto* gridItem = new QTreeWidgetItem(QStringList() << "Grid");
    m_tree->addTopLevelItem(gridItem);
    addPage(gridItem, [&](QVBoxLayout* layout) {
        buildGridProperties(layout);
    });

    // 3. Node (Components)
    auto* nodeRoot = new QTreeWidgetItem(QStringList() << "Node");
    m_tree->addTopLevelItem(nodeRoot);

    QVector<ComponentStateEntry> nodeStates;
    nodeStates.append(ComponentStateEntry("Normal", ComponentStateKind::Normal, &theme.node.normal));
    nodeStates.append(ComponentStateEntry("Hover", ComponentStateKind::Hover, &theme.node.hover));
    nodeStates.append(ComponentStateEntry("Selected", ComponentStateKind::Selected, &theme.node.selected));

    for (auto& entry : nodeStates)
    {
        auto* item = new QTreeWidgetItem(QStringList() << entry.name);
        nodeRoot->addChild(item);
        addPage(item, [this, &entry](QVBoxLayout* layout) {
            buildComponentStateProperties(*entry.state, layout, false, static_cast<int>(entry.kind));
        });
    }

    // 4. Container (Components)
    auto* containerRoot = new QTreeWidgetItem(QStringList() << "Container");
    m_tree->addTopLevelItem(containerRoot);

    QVector<ComponentStateEntry> containerStates;
    containerStates.append(ComponentStateEntry("Normal", ComponentStateKind::Normal, &theme.container.normal));
    containerStates.append(ComponentStateEntry("Hover", ComponentStateKind::Hover, &theme.container.hover));
    containerStates.append(ComponentStateEntry("Selected", ComponentStateKind::Selected, &theme.container.selected));

    for (auto& entry : containerStates)
    {
        auto* item = new QTreeWidgetItem(QStringList() << entry.name);
        containerRoot->addChild(item);
        addPage(item, [this, &entry](QVBoxLayout* layout) {
            buildComponentStateProperties(*entry.state, layout, true, static_cast<int>(entry.kind));
        });
    }

    // 5. Edge
    auto* edgeRoot = new QTreeWidgetItem(QStringList() << "Edge");
    m_tree->addTopLevelItem(edgeRoot);

    QVector<EdgeStateEntry> edgeStates;
    edgeStates.append(EdgeStateEntry("Normal", ComponentStateKind::Normal, &theme.edge.normal));
    edgeStates.append(EdgeStateEntry("Hover", ComponentStateKind::Hover, &theme.edge.hover));
    edgeStates.append(EdgeStateEntry("Selected", ComponentStateKind::Selected, &theme.edge.selected));

    for (auto& entry : edgeStates)
    {
        auto* item = new QTreeWidgetItem(QStringList() << entry.name);
        edgeRoot->addChild(item);
        addPage(item, [this, &entry](QVBoxLayout* layout) {
            buildEdgeStateProperties(*entry.state, layout, static_cast<int>(entry.kind));
        });
    }

    // 5b. Edge Preview / Connection Line
    auto* previewItem = new QTreeWidgetItem(QStringList() << "Connecting Line");
    edgeRoot->addChild(previewItem);
    addPage(previewItem, [&](QVBoxLayout* layout) {
        buildPreviewLineProperties(theme.edge.preview, layout);
    });

    // 6. Port
    auto* portRoot = new QTreeWidgetItem(QStringList() << "Port");
    m_tree->addTopLevelItem(portRoot);

    QVector<PortStateEntry> portStates;
    portStates.append(PortStateEntry("Normal", &theme.port.normal));
    portStates.append(PortStateEntry("Hover", &theme.port.hover));
    portStates.append(PortStateEntry("Selected", &theme.port.selected));

    for (auto& entry : portStates)
    {
        auto* item = new QTreeWidgetItem(QStringList() << entry.name);
        portRoot->addChild(item);
        addPage(item, [&](QVBoxLayout* layout) {
            buildPortStateProperties(*entry.state, layout);
        });
    }

    // 7. Selection
    auto* selectionItem = new QTreeWidgetItem(QStringList() << "Selection");
    m_tree->addTopLevelItem(selectionItem);
    addPage(selectionItem, [&](QVBoxLayout* layout) {
        buildSelectionProperties(layout);
    });

    // 8. Interaction
    auto* interactionItem = new QTreeWidgetItem(QStringList() << "Interaction");
    m_tree->addTopLevelItem(interactionItem);
    addPage(interactionItem, [&](QVBoxLayout* layout) {
        buildInteractionProperties(layout);
    });

    m_tree->expandAll();
}

void ThemeEditorDock::connectTree()
{
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        auto items = m_tree->selectedItems();
        if (items.isEmpty()) return;

        int index = items.first()->data(0, Qt::UserRole).toInt();
        if (index >= 0 && index < m_stack->count())
        {
            m_stack->setCurrentIndex(index);
        }
    });
}

QWidget* ThemeEditorDock::createCollapsibleSection(const QString& title, QVBoxLayout*& contentLayout)
{
    QWidget* root = new QWidget;
    auto* rootLayout = new QVBoxLayout(root);

    QToolButton* button = new QToolButton;
    button->setText(title);
    button->setCheckable(true);
    button->setChecked(true);

    QWidget* content = new QWidget;
    contentLayout = new QVBoxLayout(content);

    connect(button, &QToolButton::toggled, this, [=](bool checked) {
        content->setVisible(checked);
    });

    rootLayout->addWidget(button);
    rootLayout->addWidget(content);

    return root;
}

QPushButton* ThemeEditorDock::makeColorButton(const QColor& initial, std::function<void(const QColor&)> onChanged)
{
    QPushButton* btn = new QPushButton;
    btn->setMinimumHeight(24);

    QColor currentColor = initial;

    auto applyColor = [btn](const QColor& c) {
        QString rgba = QString("rgba(%1,%2,%3,%4)")
            .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());

        btn->setStyleSheet(QString(
            "QPushButton {"
            "border: 1px solid #444;"
            "background: %1;"
            "min-width: 40px;"
            "}").arg(rgba));
    };

    applyColor(currentColor);

    connect(btn, &QPushButton::clicked, this, [=]() mutable {
        QColorDialog dialog(currentColor, this);
        dialog.setWindowTitle("Select Color");
        dialog.setOption(QColorDialog::ShowAlphaChannel, true);

        if (dialog.exec() != QDialog::Accepted) return;

        QColor color = dialog.selectedColor();
        if (!color.isValid()) return;

        currentColor = color;
        applyColor(currentColor);
        onChanged(currentColor);
    });

    return btn;
}

QWidget* ThemeEditorDock::createColorEditor(const QString& title, const QColor& initial, std::function<void(const QColor&)> onChanged)
{
    QWidget* w = new QWidget;
    auto* layout = new QHBoxLayout(w);
    layout->addWidget(new QLabel(title));
    layout->addStretch();
    layout->addWidget(makeColorButton(initial, onChanged));
    return w;
}

QWidget* ThemeEditorDock::createIntEditor(const QString& title, int value, int min, int max, std::function<void(int)> onChanged)
{
    QWidget* w = new QWidget;
    auto* layout = new QHBoxLayout(w);

    auto* spin = new QSpinBox;
    spin->setRange(min, max);
    spin->setValue(value);

    connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, onChanged);

    layout->addWidget(new QLabel(title));
    layout->addStretch();
    layout->addWidget(spin);

    return w;
}

QWidget* ThemeEditorDock::createBoolEditor(const QString& title, bool value, std::function<void(bool)> onChanged)
{
    QWidget* w = new QWidget;
    auto* layout = new QHBoxLayout(w);

    auto* check = new QCheckBox;
    check->setChecked(value);

    connect(check, &QCheckBox::toggled, this, onChanged);

    layout->addWidget(new QLabel(title));
    layout->addStretch();
    layout->addWidget(check);

    return w;
}

QWidget* ThemeEditorDock::createPenStyleEditor(const QString& title, Qt::PenStyle value, std::function<void(Qt::PenStyle)> onChanged)
{
    QWidget* w = new QWidget;
    auto* layout = new QHBoxLayout(w);

    auto* combo = new QComboBox;
    combo->addItem("Solid", static_cast<int>(Qt::SolidLine));
    combo->addItem("Dashed", static_cast<int>(Qt::DashLine));
    combo->addItem("Dotted", static_cast<int>(Qt::DotLine));
    combo->addItem("Dash-Dot", static_cast<int>(Qt::DashDotLine));
    combo->addItem("None", static_cast<int>(Qt::NoPen));

    int idx = combo->findData(static_cast<int>(value));
    if (idx >= 0) combo->setCurrentIndex(idx);

    connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [combo, onChanged](int) {
        auto style = static_cast<Qt::PenStyle>(combo->currentData().toInt());
        onChanged(style);
    });

    layout->addWidget(new QLabel(title));
    layout->addStretch();
    layout->addWidget(combo);

    return w;
}

void ThemeEditorDock::buildTextStyleSection(const QString& title, GraphTextStyle& style, QVBoxLayout* parentLayout, std::function<void()> onUpdate)
{
    QVBoxLayout* layout = nullptr;
    auto* section = createCollapsibleSection(title, layout);

    layout->addWidget(createColorEditor("Color", style.color, [&style, onUpdate, this](const QColor& c) {
        style.color = c;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Size", style.size, 6, 72, [&style, onUpdate, this](int v) {
        style.size = v;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createBoolEditor("Bold", style.bold, [&style, onUpdate, this](bool v) {
        style.bold = v;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createBoolEditor("Italic", style.italic, [&style, onUpdate, this](bool v) {
        style.italic = v;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    parentLayout->addWidget(section);
}

void ThemeEditorDock::buildArrowStateSection(const QString& title, GraphArrowState& state, QVBoxLayout* parentLayout, std::function<void()> onUpdate)
{
    QVBoxLayout* layout = nullptr;
    auto* section = createCollapsibleSection(title, layout);

    layout->addWidget(createColorEditor("Line Color", state.lineColor, [&state, onUpdate, this](const QColor& c) {
        state.lineColor = c;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createColorEditor("Fill Color", state.fillColor, [&state, onUpdate, this](const QColor& c) {
        state.fillColor = c;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Width", state.width, 4, 40, [&state, onUpdate, this](int v) {
        state.width = v;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Height", state.height, 4, 40, [&state, onUpdate, this](int v) {
        state.height = v;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    parentLayout->addWidget(section);
}

void ThemeEditorDock::buildEdgeLabelStyleSection(const QString& title, GraphEdgeLabelState& style, QVBoxLayout* parentLayout, std::function<void()> onUpdate)
{
    QVBoxLayout* layout = nullptr;
    auto* section = createCollapsibleSection(title, layout);

    layout->addWidget(createColorEditor("Text Color", style.textColor, [&style, onUpdate, this](const QColor& c) {
        style.textColor = c;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createColorEditor("Background", style.backgroundColor, [&style, onUpdate, this](const QColor& c) {
        style.backgroundColor = c;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createColorEditor("Border Color", style.borderColor, [&style, onUpdate, this](const QColor& c) {
        style.borderColor = c;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Border Width", style.borderWidth, 0, 10, [&style, onUpdate, this](int v) {
        style.borderWidth = v;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Font Size", style.fontSize, 6, 32, [&style, onUpdate, this](int v) {
        style.fontSize = v;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createBoolEditor("Bold", style.bold, [&style, onUpdate, this](bool v) {
        style.bold = v;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    // --- Label Offset & Box Spacing ---
    layout->addWidget(createIntEditor("Line Offset", style.offset, 0, 100, [&style, onUpdate, this](int v) {
        style.offset = v;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Padding X", style.paddingX, 0, 50, [&style, onUpdate, this](int v) {
        style.paddingX = v;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Padding Y", style.paddingY, 0, 50, [&style, onUpdate, this](int v) {
        style.paddingY = v;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Corner Radius", style.radius, 0, 30, [&style, onUpdate, this](int v) {
        style.radius = v;
        if (onUpdate) onUpdate();
        emitThemeChanged();
    }));

    parentLayout->addWidget(section);
}

void ThemeEditorDock::buildComponentStateProperties(GraphComponentState& state, QVBoxLayout* layout, bool isContainer, int kindInt)
{
    auto kind = static_cast<ComponentStateKind>(kindInt);
    auto* preview = new ComponentPreviewWidget(kind, isContainer);
    layout->addWidget(new QLabel(isContainer ? "<b>Live Container Preview:</b>" : "<b>Live Node Preview:</b>"));
    layout->addWidget(preview);

    auto updatePreview = [preview]() { preview->refresh(); };

    layout->addWidget(createColorEditor("Background", state.background, [&state, updatePreview, this](const QColor& c) {
        state.background = c;
        updatePreview();
        emitThemeChanged();
    }));

    layout->addWidget(createColorEditor("Border Color", state.border, [&state, updatePreview, this](const QColor& c) {
        state.border = c;
        updatePreview();
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Border Width", state.borderWidth, 0, 20, [&state, updatePreview, this](int v) {
        state.borderWidth = v;
        updatePreview();
        emitThemeChanged();
    }));

    layout->addWidget(createPenStyleEditor("Border Style", state.borderStyle, [&state, updatePreview, this](Qt::PenStyle s) {
        state.borderStyle = s;
        if (s != Qt::CustomDashLine)
        {
            state.dashPattern.clear();
        }
        updatePreview();
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Corner Radius", state.radius, 0, 50, [&state, updatePreview, this](int v) {
        state.radius = v;
        updatePreview();
        emitThemeChanged();
    }));

    if (isContainer)
    {
        layout->addWidget(createColorEditor("Header Color", state.headerBackground, [&state, updatePreview, this](const QColor& c) {
            state.headerBackground = c;
            updatePreview();
            emitThemeChanged();
        }));
    }

    buildTextStyleSection("Title Text", state.title, layout, updatePreview);

    if (!isContainer)
    {
        buildTextStyleSection("Body Text", state.body, layout, updatePreview);
    }
}

void ThemeEditorDock::buildEdgeStateProperties(GraphEdgeState& state, QVBoxLayout* layout, int kindInt)
{
    auto kind = static_cast<ComponentStateKind>(kindInt);
    auto* preview = new EdgePreviewWidget(kind);
    layout->addWidget(new QLabel("<b>Live Edge Preview:</b>"));
    layout->addWidget(preview);

    auto updatePreview = [preview]() { preview->refresh(); };

    layout->addWidget(createColorEditor("Line Color", state.lineColor, [&state, updatePreview, this](const QColor& c) {
        state.lineColor = c;
        updatePreview();
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Line Width", state.lineWidth, 1, 20, [&state, updatePreview, this](int v) {
        state.lineWidth = v;
        updatePreview();
        emitThemeChanged();
    }));

    layout->addWidget(createPenStyleEditor("Line Style", state.lineStyle, [&state, updatePreview, this](Qt::PenStyle s) {
        state.lineStyle = s;
        state.dashed = (s == Qt::DashLine);
        if (s != Qt::CustomDashLine)
        {
            state.dashPattern.clear();
        }
        updatePreview();
        emitThemeChanged();
    }));

    buildArrowStateSection("Arrow", state.arrow, layout, updatePreview);
    buildEdgeLabelStyleSection("Label", state.label, layout, updatePreview);
}

void ThemeEditorDock::buildPreviewLineProperties(GraphPreviewLineTheme& preview, QVBoxLayout* layout)
{
    auto* previewWidget = new PreviewLinePreviewWidget();
    layout->addWidget(new QLabel("<b>Live Connecting Line Preview:</b>"));
    layout->addWidget(previewWidget);

    auto updatePreview = [previewWidget]() { previewWidget->refresh(); };

    layout->addWidget(createColorEditor("Line Color", preview.color, [&preview, updatePreview, this](const QColor& c) {
        preview.color = c;
        updatePreview();
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Line Width", preview.width, 1, 10, [&preview, updatePreview, this](int v) {
        preview.width = v;
        updatePreview();
        emitThemeChanged();
    }));

    layout->addWidget(createPenStyleEditor("Line Style", preview.style, [&preview, updatePreview, this](Qt::PenStyle s) {
        preview.style = s;
        updatePreview();
        emitThemeChanged();
    }));
}

void ThemeEditorDock::buildPortStateProperties(GraphPortState& state, QVBoxLayout* layout)
{
    layout->addWidget(createColorEditor("Input", state.inputColor, [&state, this](const QColor& c) {
        state.inputColor = c;
        emitThemeChanged();
    }));

    layout->addWidget(createColorEditor("Output", state.outputColor, [&state, this](const QColor& c) {
        state.outputColor = c;
        emitThemeChanged();
    }));

    layout->addWidget(createColorEditor("Hover", state.hoverColor, [&state, this](const QColor& c) {
        state.hoverColor = c;
        emitThemeChanged();
    }));
}

void ThemeEditorDock::buildViewProperties(QVBoxLayout* layout)
{
    auto& theme = GraphThemeManager::instance()->mutableTheme();

    layout->addWidget(createColorEditor("Background", theme.view.background, [&theme, this](const QColor& c) {
        theme.view.background = c;
        emitThemeChanged();
    }));
}

void ThemeEditorDock::buildGridProperties(QVBoxLayout* layout)
{
    auto& grid = GraphThemeManager::instance()->mutableTheme().view.grid;

    layout->addWidget(createBoolEditor("Enabled", grid.enabled, [&grid, this](bool v) {
        grid.enabled = v;
        emitThemeChanged();
    }));

    layout->addWidget(createColorEditor("Minor Color", grid.minorColor, [&grid, this](const QColor& c) {
        grid.minorColor = c;
        emitThemeChanged();
    }));

    layout->addWidget(createColorEditor("Major Color", grid.majorColor, [&grid, this](const QColor& c) {
        grid.majorColor = c;
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Cell Spacing", grid.spacing, 5, 200, [&grid, this](int v) {
        grid.spacing = v;
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Major Spacing", grid.majorSpacing, 10, 1000, [&grid, this](int v) {
        grid.majorSpacing = v;
        emitThemeChanged();
    }));

    layout->addWidget(createIntEditor("Line Width", grid.lineWidth, 1, 10, [&grid, this](int v) {
        grid.lineWidth = v;
        emitThemeChanged();
    }));
}

void ThemeEditorDock::buildSelectionProperties(QVBoxLayout* layout)
{
    auto& selection = GraphThemeManager::instance()->mutableTheme().selection;

    layout->addWidget(createColorEditor("Outline", selection.outline, [&selection, this](const QColor& c) {
        selection.outline = c;
        emitThemeChanged();
    }));

    layout->addWidget(createColorEditor("Fill", selection.fill, [&selection, this](const QColor& c) {
        selection.fill = c;
        emitThemeChanged();
    }));
}

void ThemeEditorDock::buildInteractionProperties(QVBoxLayout* layout)
{
    auto& interaction = GraphThemeManager::instance()->mutableTheme().interaction;

    layout->addWidget(createColorEditor("Hover Outline", interaction.hoverOutline, [&interaction, this](const QColor& c) {
        interaction.hoverOutline = c;
        emitThemeChanged();
    }));

    layout->addWidget(createColorEditor("Invalid Connection", interaction.invalidConnection, [&interaction, this](const QColor& c) {
        interaction.invalidConnection = c;
        emitThemeChanged();
    }));

    layout->addWidget(createColorEditor("Drop Target", interaction.dropTarget, [&interaction, this](const QColor& c) {
        interaction.dropTarget = c;
        emitThemeChanged();
    }));
}

void ThemeEditorDock::emitThemeChanged()
{
    if (m_isInternalUpdate)
        return;

    m_isInternalUpdate = true;
    GraphThemeManager::instance()->notifyThemeChanged();
    m_isInternalUpdate = false;
}

void ThemeEditorDock::syncFromTheme()
{
    if (m_isInternalUpdate)
        return;

    int currentIndex = m_stack->currentIndex();

    while (m_stack->count() > 0)
    {
        QWidget* w = m_stack->widget(0);
        m_stack->removeWidget(w);
        w->deleteLater();
    }

    m_tree->clear();
    populateTree();

    if (currentIndex >= 0 && currentIndex < m_stack->count())
    {
        m_stack->setCurrentIndex(currentIndex);
    }
}