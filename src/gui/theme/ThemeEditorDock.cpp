#include "ThemeEditorDock.h"

#include "theme/GraphThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QScrollArea>
#include <QGroupBox>
#include <QFrame>

ThemeEditorDock::ThemeEditorDock(
    QWidget* parent)
    : QDockWidget(parent)
{
    setWindowTitle("Theme Editor");

    QWidget* root =
        new QWidget;

    QVBoxLayout* layout =
        new QVBoxLayout(root);

    buildSceneSection(layout);
    buildGridSection(layout);
    buildNodeSection(layout);
    buildEdgeSection(layout);
    buildArrowSection(layout);
    // buildTextSection(layout);

    /*
     * SAVE
     */

    QPushButton* saveBtn =
        new QPushButton("Save Theme");

    connect(saveBtn,
            &QPushButton::clicked,
            this,
            []()
            {
                GraphThemeManager::instance()
                    ->save("dark.json");
            });

    layout->addWidget(saveBtn);
    layout->addStretch();

    QScrollArea* scroll =
        new QScrollArea;

    scroll->setWidget(root);
    scroll->setWidgetResizable(true);

    setWidget(scroll);
}

void ThemeEditorDock::buildSceneSection(
    QVBoxLayout* parentLayout)
{
    const auto& theme =
        GraphThemeManager::instance()
            ->theme();

    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection("Scene", layout);

    /*
     * Replace these fields with your actual scene theme fields.
     */

    layout->addWidget(
        createColorEditor(
            "Background",
            theme.view.background,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.view.background = c;
            }));

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildGridSection(
    QVBoxLayout* parentLayout)
{
    const auto& theme =
        GraphThemeManager::instance()
            ->theme();

    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection("Grid", layout);

    layout->addWidget(
        createColorEditor(
            "Minor Grid",
            theme.view.grid.minorColor,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.view.grid.minorColor = c;
            }));

    layout->addWidget(
        createColorEditor(
            "Major Grid",
            theme.view.grid.majorColor,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.view.grid.majorColor = c;
            }));

    layout->addWidget(
        createIntEditor(
            "Grid Size",
            theme.view.grid.spacing,
            4,
            200,
            [](int v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.view.grid.spacing = v;
            }));

    layout->addWidget(
        createBoolEditor(
            "Show Grid",
            theme.view.grid.enabled,
            [](bool v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.view.grid.enabled = v;
            }));

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildNodeSection(
    QVBoxLayout* parentLayout)
{
    const auto& theme =
        GraphThemeManager::instance()
            ->theme();

    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection("Nodes", layout);

    /*
     * NORMAL
     */

    QVBoxLayout* normalLayout = nullptr;

    auto* normalGroup =
        createSection("Normal", normalLayout);

    normalLayout->addWidget(
        createColorEditor(
            "Background",
            theme.node.normal.background,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.node.normal.background = c;
            }));

    normalLayout->addWidget(
        createColorEditor(
            "Border",
            theme.node.normal.border,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.node.normal.border = c;
            }));

    normalLayout->addWidget(
        createIntEditor(
            "Border Width",
            theme.node.normal.borderWidth,
            0,
            20,
            [](int v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.node.normal.borderWidth = v;
            }));

    normalLayout->addWidget(
        createIntEditor(
            "Corner Radius",
            theme.node.normal.radius,
            0,
            64,
            [](int v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.node.normal.radius = v;
            }));

    normalLayout->addWidget(
        createIntEditor(
            "Padding",
            theme.node.normal.padding,
            0,
            64,
            [](int v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.node.normal.padding = v;
            }));

    layout->addWidget(normalGroup);

    /*
     * HOVER
     */

    QVBoxLayout* hoverLayout = nullptr;

    auto* hoverGroup =
        createSection("Hover", hoverLayout);

    hoverLayout->addWidget(
        createColorEditor(
            "Background",
            theme.node.hover.background,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.node.hover.background = c;
            }));

    hoverLayout->addWidget(
        createColorEditor(
            "Border",
            theme.node.hover.border,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.node.hover.border = c;
            }));

    hoverLayout->addWidget(
        createIntEditor(
            "Border Width",
            theme.node.hover.borderWidth,
            0,
            20,
            [](int v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.node.hover.borderWidth = v;
            }));

    layout->addWidget(hoverGroup);

    /*
     * SELECTED
     */

    QVBoxLayout* selectedLayout = nullptr;

    auto* selectedGroup =
        createSection("Selected", selectedLayout);

    selectedLayout->addWidget(
        createColorEditor(
            "Background",
            theme.node.selected.background,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.node.selected.background = c;
            }));

    selectedLayout->addWidget(
        createColorEditor(
            "Border",
            theme.node.selected.border,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.node.selected.border = c;
            }));

    selectedLayout->addWidget(
        createIntEditor(
            "Border Width",
            theme.node.selected.borderWidth,
            0,
            20,
            [](int v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.node.selected.borderWidth = v;
            }));

    layout->addWidget(selectedGroup);

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildEdgeSection(
    QVBoxLayout* parentLayout)
{
    const auto& theme =
        GraphThemeManager::instance()
            ->theme();

    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection("Edges", layout);

    /*
     * NORMAL
     */

    QVBoxLayout* normalLayout = nullptr;

    auto* normalGroup =
        createSection("Normal", normalLayout);

    normalLayout->addWidget(
        createColorEditor(
            "Line Color",
            theme.edge.normal.lineColor,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.edge.normal.lineColor = c;
            }));

    normalLayout->addWidget(
        createIntEditor(
            "Line Width",
            theme.edge.normal.lineWidth,
            1,
            20,
            [](int v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.edge.normal.lineWidth = v;
            }));

    normalLayout->addWidget(
        createBoolEditor(
            "Dashed",
            theme.edge.normal.dashed,
            [](bool v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.edge.normal.dashed = v;
            }));

    layout->addWidget(normalGroup);

    /*
     * HOVER
     */

    QVBoxLayout* hoverLayout = nullptr;

    auto* hoverGroup =
        createSection("Hover", hoverLayout);

    hoverLayout->addWidget(
        createColorEditor(
            "Line Color",
            theme.edge.hover.lineColor,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.edge.hover.lineColor = c;
            }));

    hoverLayout->addWidget(
        createIntEditor(
            "Line Width",
            theme.edge.hover.lineWidth,
            1,
            20,
            [](int v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.edge.hover.lineWidth = v;
            }));

    layout->addWidget(hoverGroup);

    /*
     * SELECTED
     */

    QVBoxLayout* selectedLayout = nullptr;

    auto* selectedGroup =
        createSection("Selected", selectedLayout);

    selectedLayout->addWidget(
        createColorEditor(
            "Line Color",
            theme.edge.selected.lineColor,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.edge.selected.lineColor = c;
            }));

    selectedLayout->addWidget(
        createIntEditor(
            "Line Width",
            theme.edge.selected.lineWidth,
            1,
            20,
            [](int v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.edge.selected.lineWidth = v;
            }));

    layout->addWidget(selectedGroup);

    parentLayout->addWidget(group);
}

void ThemeEditorDock::buildArrowSection(
    QVBoxLayout* parentLayout)
{
    const auto& theme =
        GraphThemeManager::instance()
            ->theme();

    QVBoxLayout* layout = nullptr;

    auto* group =
        createSection("Arrows", layout);

    layout->addWidget(
        createColorEditor(
            "Fill",
            theme.edge.normal.arrow.fillColor,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.edge.normal.arrow.fillColor = c;
            }));

    layout->addWidget(
        createColorEditor(
            "Border",
            theme.edge.normal.arrow.borderColor,
            [](const QColor& c)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.edge.normal.arrow.borderColor = c;
            }));

    layout->addWidget(
        createIntEditor(
            "Border Width",
            theme.edge.normal.arrow.borderWidth,
            0,
            20,
            [](int v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.edge.normal.arrow.borderWidth = v;
            }));

    layout->addWidget(
        createIntEditor(
            "Width",
            theme.edge.normal.arrow.width,
            1,
            128,
            [](int v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.edge.normal.arrow.width = v;
            }));

    layout->addWidget(
        createIntEditor(
            "Height",
            theme.edge.normal.arrow.height,
            1,
            128,
            [](int v)
            {
                auto& theme =
                    GraphThemeManager::instance()
                        ->mutableTheme();

                theme.edge.normal.arrow.height = v;
            }));

    parentLayout->addWidget(group);
}

// void ThemeEditorDock::buildTextSection(
//     QVBoxLayout* parentLayout)
// {
//     const auto& theme =
//         GraphThemeManager::instance()
//             ->theme();

//     QVBoxLayout* layout = nullptr;

//     auto* group =
//         createSection("Text", layout);

//     layout->addWidget(
//         createColorEditor(
//             "Node Text",
//             theme.text.nodeColor,
//             [](const QColor& c)
//             {
//                 auto& theme =
//                     GraphThemeManager::instance()
//                         ->mutableTheme();

//                 theme.text.nodeColor = c;
//             }));

//     layout->addWidget(
//         createColorEditor(
//             "Edge Text",
//             theme.text.edgeColor,
//             [](const QColor& c)
//             {
//                 auto& theme =
//                     GraphThemeManager::instance()
//                         ->mutableTheme();

//                 theme.text.edgeColor = c;
//             }));

//     layout->addWidget(
//         createIntEditor(
//             "Font Size",
//             theme.text.fontSize,
//             6,
//             64,
//             [](int v)
//             {
//                 auto& theme =
//                     GraphThemeManager::instance()
//                         ->mutableTheme();

//                 theme.text.fontSize = v;
//             }));

//     parentLayout->addWidget(group);
// }

QGroupBox* ThemeEditorDock::createSection(
    const QString& title,
    QVBoxLayout*& outLayout)
{
    auto* group =
        new QGroupBox(title);

    outLayout =
        new QVBoxLayout(group);

    outLayout->setSpacing(6);

    return group;
}

QWidget* ThemeEditorDock::createColorEditor(
    const QString& title,
    QColor initial,
    std::function<void(const QColor&)> onChanged)
{
    QWidget* w =
        new QWidget;

    QHBoxLayout* layout =
        new QHBoxLayout(w);

    QLabel* label =
        new QLabel(title);

    QPushButton* button =
        makeColorButton(initial,
                        [=](const QColor& c)
                        {
                            updateTheme(
                                [&]()
                                {
                                    onChanged(c);
                                });
                        });

    layout->addWidget(label);
    layout->addStretch();
    layout->addWidget(button);

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

    QHBoxLayout* layout =
        new QHBoxLayout(w);

    QLabel* label =
        new QLabel(title);

    QSpinBox* spin =
        new QSpinBox;

    spin->setRange(min, max);
    spin->setValue(value);

    connect(
        spin,
        QOverload<int>::of(
            &QSpinBox::valueChanged),
        this,
        [=](int v)
        {
            updateTheme(
                [&]()
                {
                    onChanged(v);
                });
        });

    layout->addWidget(label);
    layout->addStretch();
    layout->addWidget(spin);

    return w;
}

QWidget* ThemeEditorDock::createDoubleEditor(
    const QString& title,
    double value,
    double min,
    double max,
    double step,
    std::function<void(double)> onChanged)
{
    QWidget* w =
        new QWidget;

    QHBoxLayout* layout =
        new QHBoxLayout(w);

    QLabel* label =
        new QLabel(title);

    QDoubleSpinBox* spin =
        new QDoubleSpinBox;

    spin->setRange(min, max);
    spin->setSingleStep(step);
    spin->setValue(value);

    connect(
        spin,
        QOverload<double>::of(
            &QDoubleSpinBox::valueChanged),
        this,
        [=](double v)
        {
            updateTheme(
                [&]()
                {
                    onChanged(v);
                });
        });

    layout->addWidget(label);
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

    QHBoxLayout* layout =
        new QHBoxLayout(w);

    QCheckBox* check =
        new QCheckBox(title);

    check->setChecked(value);

    connect(check,
            &QCheckBox::toggled,
            this,
            [=](bool v)
            {
                updateTheme(
                    [&]()
                    {
                        onChanged(v);
                    });
            });

    layout->addWidget(check);
    layout->addStretch();

    return w;
}

QPushButton* ThemeEditorDock::makeColorButton(
    QColor initial,
    std::function<void(const QColor&)> onChanged)
{
    QPushButton* button =
        new QPushButton;

    button->setFixedSize(48, 24);

    auto updateButton =
        [button](QColor c)
        {
            button->setStyleSheet(
                QString(
                    "border:1px solid #666;"
                    "border-radius:4px;"
                    "background:%1;")
                        .arg(
                            c.name(
                                QColor::HexArgb)));
        };

    updateButton(initial);

    connect(
        button,
        &QPushButton::clicked,
        button,
        [=]() mutable
        {
            QColor c =
                QColorDialog::getColor(
                    initial,
                    button,
                    "Select Color",
                    QColorDialog::ShowAlphaChannel);

            if (!c.isValid())
                return;

            updateButton(c);
            onChanged(c);
        });

    return button;
}