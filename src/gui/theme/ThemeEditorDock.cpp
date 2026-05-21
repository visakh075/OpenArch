
#include "ThemeEditorDock.h"

#include "theme/GraphThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QScrollArea>
#include <QGroupBox>
#include <QToolBox>

ThemeEditorDock::ThemeEditorDock(
    QWidget* parent)
    : QDockWidget(parent)
{
    setWindowTitle("Theme Editor");

    QWidget* root =
        new QWidget;

    QVBoxLayout* rootLayout =
        new QVBoxLayout(root);

    QToolBox* toolbox =
        new QToolBox;

    createToolBoxPage(toolbox, "View");
    createToolBoxPage(toolbox, "Grid");
    createToolBoxPage(toolbox, "Node");
    createToolBoxPage(toolbox, "Edge");
    createToolBoxPage(toolbox, "Port");
    createToolBoxPage(toolbox, "Selection");
    createToolBoxPage(toolbox, "Interaction");

    {
        QWidget* page =
            toolbox->widget(0);

        auto* layout =
            qobject_cast<QVBoxLayout*>(
                page->layout());

        buildViewSection(layout);
    }

    {
        QWidget* page =
            toolbox->widget(1);

        auto* layout =
            qobject_cast<QVBoxLayout*>(
                page->layout());

        buildGridSection(layout);
    }

    {
        QWidget* page =
            toolbox->widget(2);

        auto* layout =
            qobject_cast<QVBoxLayout*>(
                page->layout());

        buildNodeSection(layout);
    }

    {
        QWidget* page =
            toolbox->widget(3);

        auto* layout =
            qobject_cast<QVBoxLayout*>(
                page->layout());

        buildEdgeSection(layout);
    }

    {
        QWidget* page =
            toolbox->widget(4);

        auto* layout =
            qobject_cast<QVBoxLayout*>(
                page->layout());

        buildPortSection(layout);
    }

    {
        QWidget* page =
            toolbox->widget(5);

        auto* layout =
            qobject_cast<QVBoxLayout*>(
                page->layout());

        buildSelectionSection(layout);
    }

    {
        QWidget* page =
            toolbox->widget(6);

        auto* layout =
            qobject_cast<QVBoxLayout*>(
                page->layout());

        buildInteractionSection(layout);
    }

    rootLayout->addWidget(toolbox);

    QScrollArea* scroll =
        new QScrollArea;

    scroll->setWidget(root);
    scroll->setWidgetResizable(true);

    setWidget(scroll);
}

QWidget* ThemeEditorDock::createToolBoxPage(
    QToolBox* toolBox,
    const QString& title)
{
    QWidget* page =
        new QWidget;

    auto* layout =
        new QVBoxLayout(page);

    layout->setContentsMargins(4, 4, 4, 4);

    toolBox->addItem(page, title);

    return page;
}

void ThemeEditorDock::emitThemeChanged()
{
    GraphThemeManager::instance()
        ->notifyThemeChanged();
}

QGroupBox* ThemeEditorDock::createSection(
    const QString& title,
    QVBoxLayout*& outLayout)
{
    auto* group =
        new QGroupBox(title);

    outLayout =
        new QVBoxLayout(group);

    return group;
}

// QPushButton* ThemeEditorDock::makeColorButton(
//     const QColor& initial,
//     std::function<void(const QColor&)> onChanged)
// {
//     QPushButton* btn =
//         new QPushButton;

//     btn->setMinimumHeight(24);

//     auto applyColor =
//         [btn](const QColor& c)
//         {
//             btn->setStyleSheet(
//                 QString("background:%1;")
//                     .arg(c.name()));
//         };

//     applyColor(initial);

//     connect(btn,
//             &QPushButton::clicked,
//             this,
//             [=]() mutable
//             {
//                 QColor color =
//                     QColorDialog::getColor(
//                         initial,
//                         this,
//                         "Select Color");

//                 if (!color.isValid())
//                     return;

//                 applyColor(color);

//                 onChanged(color);
//             });

//     return btn;
// }
QPushButton* ThemeEditorDock::makeColorButton(
    const QColor& initial,
    std::function<void(const QColor&)> onChanged)
{
    QPushButton* btn =
        new QPushButton;

    btn->setMinimumHeight(24);

    auto applyColor =
        [btn](const QColor& c)
        {
            QString rgba =
                QString("rgba(%1,%2,%3,%4)")
                    .arg(c.red())
                    .arg(c.green())
                    .arg(c.blue())
                    .arg(c.alpha());

            btn->setStyleSheet(
                QString(
                    "QPushButton {"
                    "border: 1px solid #444;"
                    "background: %1;"
                    "}")
                    .arg(rgba));
        };

    applyColor(initial);

    connect(btn,
            &QPushButton::clicked,
            this,
            [=]() mutable
            {
                QColorDialog dialog(
                    initial,
                    this);

                dialog.setWindowTitle(
                    "Select Color");

                dialog.setOption(
                    QColorDialog::ShowAlphaChannel,
                    true);

                if (dialog.exec() != QDialog::Accepted)
                {
                    return;
                }

                QColor color =
                    dialog.selectedColor();

                if (!color.isValid())
                    return;

                applyColor(color);

                onChanged(color);
            });

    return btn;
}

QWidget* ThemeEditorDock::createColorEditor(
    const QString& title,
    const QColor& initial,
    std::function<void(const QColor&)> onChanged)
{
    QWidget* w =
        new QWidget;

    auto* layout =
        new QHBoxLayout(w);

    layout->addWidget(
        new QLabel(title));

    layout->addStretch();

    layout->addWidget(
        makeColorButton(
            initial,
            onChanged));

    return w;
}

QWidget* ThemeEditorDock::createIntEditor(
    const QString& title,
    int value,
    int min,
    int max,
    std::function<void(int)> onChanged)
{
    QWidget* w =
        new QWidget;

    auto* layout =
        new QHBoxLayout(w);

    auto* spin =
        new QSpinBox;

    spin->setRange(min, max);
    spin->setValue(value);

    connect(spin,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            onChanged);

    layout->addWidget(
        new QLabel(title));

    layout->addStretch();

    layout->addWidget(spin);

    return w;
}

QWidget* ThemeEditorDock::createBoolEditor(
    const QString& title,
    bool value,
    std::function<void(bool)> onChanged)
{
    QWidget* w =
        new QWidget;

    auto* layout =
        new QHBoxLayout(w);

    auto* check =
        new QCheckBox;

    check->setChecked(value);

    connect(check,
            &QCheckBox::toggled,
            this,
            onChanged);

    layout->addWidget(
        new QLabel(title));

    layout->addStretch();

    layout->addWidget(check);

    return w;
}

void ThemeEditorDock::buildTextStyleSection(
    const QString& title,
    GraphTextStyle& style,
    QVBoxLayout* parentLayout)
{
    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection(title, layout);

    layout->addWidget(
        createColorEditor(
            "Color",
            style.color,
            [&](const QColor& c)
            {
                style.color = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Size",
            style.size,
            6,
            72,
            [&](int v)
            {
                style.size = v;
                emitThemeChanged();
            }));

    layout->addWidget(
        createBoolEditor(
            "Bold",
            style.bold,
            [&](bool v)
            {
                style.bold = v;
                emitThemeChanged();
            }));

    layout->addWidget(
        createBoolEditor(
            "Italic",
            style.italic,
            [&](bool v)
            {
                style.italic = v;
                emitThemeChanged();
            }));

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildNodeStateSection(
    const QString& title,
    GraphNodeState& state,
    QVBoxLayout* parentLayout)
{
    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection(title, layout);

    layout->addWidget(
        createColorEditor(
            "Background",
            state.background,
            [&](const QColor& c)
            {
                state.background = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Border",
            state.border,
            [&](const QColor& c)
            {
                state.border = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Border Width",
            state.borderWidth,
            1,
            20,
            [&](int v)
            {
                state.borderWidth = v;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Radius",
            state.radius,
            0,
            100,
            [&](int v)
            {
                state.radius = v;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Padding",
            state.padding,
            0,
            100,
            [&](int v)
            {
                state.padding = v;
                emitThemeChanged();
            }));

    buildTextStyleSection(
        "Title Text",
        state.title,
        layout);

    buildTextStyleSection(
        "Body Text",
        state.body,
        layout);

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildNodeSection(
    QVBoxLayout* parentLayout)
{
    auto& theme =
        GraphThemeManager::instance()
            ->mutableTheme();

    buildNodeStateSection(
        "Normal",
        theme.node.normal,
        parentLayout);

    buildNodeStateSection(
        "Hover",
        theme.node.hover,
        parentLayout);

    buildNodeStateSection(
        "Selected",
        theme.node.selected,
        parentLayout);
}

void ThemeEditorDock::buildArrowStateSection(
    const QString& title,
    GraphArrowState& state,
    QVBoxLayout* parentLayout)
{
    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection(title, layout);

    layout->addWidget(
        createColorEditor(
            "Line Color",
            state.lineColor,
            [&](const QColor& c)
            {
                state.lineColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Fill Color",
            state.fillColor,
            [&](const QColor& c)
            {
                state.fillColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Border Color",
            state.borderColor,
            [&](const QColor& c)
            {
                state.borderColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Width",
            state.width,
            1,
            100,
            [&](int v)
            {
                state.width = v;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Height",
            state.height,
            1,
            100,
            [&](int v)
            {
                state.height = v;
                emitThemeChanged();
            }));

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildEdgeLabelStyleSection(
    const QString& title,
    GraphEdgeLabelState& style,
    QVBoxLayout* parentLayout)
{
    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection(title, layout);

    layout->addWidget(
        createColorEditor(
            "Text Color",
            style.textColor,
            [&](const QColor& c)
            {
                style.textColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Background",
            style.backgroundColor,
            [&](const QColor& c)
            {
                style.backgroundColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Border",
            style.borderColor,
            [&](const QColor& c)
            {
                style.borderColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Font Size",
            style.fontSize,
            6,
            72,
            [&](int v)
            {
                style.fontSize = v;
                emitThemeChanged();
            }));

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildEdgeStateSection(
    const QString& title,
    GraphEdgeState& state,
    QVBoxLayout* parentLayout)
{
    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection(title, layout);

    layout->addWidget(
        createColorEditor(
            "Line Color",
            state.lineColor,
            [&](const QColor& c)
            {
                state.lineColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Line Width",
            state.lineWidth,
            1,
            20,
            [&](int v)
            {
                state.lineWidth = v;
                emitThemeChanged();
            }));

    layout->addWidget(
        createBoolEditor(
            "Dashed",
            state.dashed,
            [&](bool v)
            {
                state.dashed = v;
                emitThemeChanged();
            }));

    buildArrowStateSection(
        "Arrow",
        state.arrow,
        layout);

    buildEdgeLabelStyleSection(
        "Label",
        state.label,
        layout);

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildEdgeSection(
    QVBoxLayout* parentLayout)
{
    auto& theme =
        GraphThemeManager::instance()
            ->mutableTheme();

    buildEdgeStateSection(
        "Normal",
        theme.edge.normal,
        parentLayout);

    buildEdgeStateSection(
        "Hover",
        theme.edge.hover,
        parentLayout);

    buildEdgeStateSection(
        "Selected",
        theme.edge.selected,
        parentLayout);
}
void ThemeEditorDock::buildViewSection(
    QVBoxLayout* parentLayout)
{
    auto& theme =
        GraphThemeManager::instance()
            ->mutableTheme();

    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection(
            "View",
            layout);

    layout->addWidget(
        createColorEditor(
            "Background",
            theme.view.background,
            [&](const QColor& c)
            {
                theme.view.background = c;

                emitThemeChanged();
            }));

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildGridSection(
    QVBoxLayout* parentLayout)
{
    auto& grid =
        GraphThemeManager::instance()
            ->mutableTheme()
            .view.grid;

    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection(
            "Grid",
            layout);

    layout->addWidget(
        createBoolEditor(
            "Enabled",
            grid.enabled,
            [&](bool v)
            {
                grid.enabled = v;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Minor Color",
            grid.minorColor,
            [&](const QColor& c)
            {
                grid.minorColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Major Color",
            grid.majorColor,
            [&](const QColor& c)
            {
                grid.majorColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Spacing",
            grid.spacing,
            1,
            500,
            [&](int v)
            {
                grid.spacing = v;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Major Spacing",
            grid.majorSpacing,
            1,
            1000,
            [&](int v)
            {
                grid.majorSpacing = v;
                emitThemeChanged();
            }));

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildPortSection(
    QVBoxLayout* parentLayout)
{
    auto& theme =
        GraphThemeManager::instance()
            ->mutableTheme();

    buildPortStateSection(
        "Normal",
        theme.port.normal,
        parentLayout);

    buildPortStateSection(
        "Hover",
        theme.port.hover,
        parentLayout);

    buildPortStateSection(
        "Selected",
        theme.port.selected,
        parentLayout);
}

void ThemeEditorDock::buildPortStateSection(
    const QString& title,
    GraphPortState& state,
    QVBoxLayout* parentLayout)
{
    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection(title, layout);

    layout->addWidget(
        createColorEditor(
            "Input Color",
            state.inputColor,
            [&](const QColor& c)
            {
                state.inputColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Output Color",
            state.outputColor,
            [&](const QColor& c)
            {
                state.outputColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Hover Color",
            state.hoverColor,
            [&](const QColor& c)
            {
                state.hoverColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Border Color",
            state.borderColor,
            [&](const QColor& c)
            {
                state.borderColor = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Radius",
            state.radius,
            1,
            50,
            [&](int v)
            {
                state.radius = v;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Border Width",
            state.borderWidth,
            1,
            20,
            [&](int v)
            {
                state.borderWidth = v;
                emitThemeChanged();
            }));

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildSelectionSection(
    QVBoxLayout* parentLayout)
{
    auto& selection =
        GraphThemeManager::instance()
            ->mutableTheme()
            .selection;

    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection(
            "Selection",
            layout);

    layout->addWidget(
        createColorEditor(
            "Outline",
            selection.outline,
            [&](const QColor& c)
            {
                selection.outline = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Fill",
            selection.fill,
            [&](const QColor& c)
            {
                selection.fill = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Border Width",
            selection.borderWidth,
            1,
            20,
            [&](int v)
            {
                selection.borderWidth = v;
                emitThemeChanged();
            }));

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildInteractionSection(
    QVBoxLayout* parentLayout)
{
    auto& interaction =
        GraphThemeManager::instance()
            ->mutableTheme()
            .interaction;

    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection(
            "Interaction",
            layout);

    layout->addWidget(
        createColorEditor(
            "Hover Outline",
            interaction.hoverOutline,
            [&](const QColor& c)
            {
                interaction.hoverOutline = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Invalid Connection",
            interaction.invalidConnection,
            [&](const QColor& c)
            {
                interaction.invalidConnection = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Drop Target",
            interaction.dropTarget,
            [&](const QColor& c)
            {
                interaction.dropTarget = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createColorEditor(
            "Snap Guide",
            interaction.snapGuide,
            [&](const QColor& c)
            {
                interaction.snapGuide = c;
                emitThemeChanged();
            }));

    layout->addWidget(
        createIntEditor(
            "Snap Guide Width",
            interaction.snapGuideWidth,
            1,
            20,
            [&](int v)
            {
                interaction.snapGuideWidth = v;
                emitThemeChanged();
            }));

    parentLayout->addWidget(group);
}