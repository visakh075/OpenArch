#include "ThemeEditorDock.h"

#include "theme/GraphThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QColorDialog>
#include <QScrollArea>
#include <QGroupBox>

ThemeEditorDock::ThemeEditorDock(
    QWidget* parent)
    : QDockWidget(parent)
{
    setWindowTitle("Theme Editor");

    QWidget* root =
        new QWidget;

    QVBoxLayout* layout =
        new QVBoxLayout(root);

    const auto& theme =
        GraphThemeManager::instance()
            ->theme();

    /*
     * NODE
     */

    {
        QGroupBox* group =
            new QGroupBox("Node");

        QVBoxLayout* gLayout =
            new QVBoxLayout(group);

        gLayout->addWidget(
            createColorEditor(
                "Background",
                theme.node.normal.background,
                [](const QColor& c)
                {
                    auto& theme =
                        GraphThemeManager::instance()
                            ->mutableTheme();

                    theme.node.normal.background = c;

                    GraphThemeManager::instance()
                        ->notifyThemeChanged();
                }));

        gLayout->addWidget(
            createColorEditor(
                "Border",
                theme.node.normal.border,
                [](const QColor& c)
                {
                    auto& theme =
                        GraphThemeManager::instance()
                            ->mutableTheme();

                    theme.node.normal.border = c;

                    GraphThemeManager::instance()
                        ->notifyThemeChanged();
                }));

        gLayout->addWidget(
            createIntEditor(
                "Radius",
                theme.node.normal.radius,
                0,
                50,
                [](int v)
                {
                    auto& theme =
                        GraphThemeManager::instance()
                            ->mutableTheme();

                    theme.node.normal.radius = v;

                    GraphThemeManager::instance()
                        ->notifyThemeChanged();
                }));

        layout->addWidget(group);
    }

    /*
     * EDGE
     */

    {
        QGroupBox* group =
            new QGroupBox("Edge");

        QVBoxLayout* gLayout =
            new QVBoxLayout(group);

        gLayout->addWidget(
            createColorEditor(
                "Line Color",
                theme.edge.normal.lineColor,
                [](const QColor& c)
                {
                    auto& theme =
                        GraphThemeManager::instance()
                            ->mutableTheme();

                    theme.edge.normal.lineColor = c;

                    GraphThemeManager::instance()
                        ->notifyThemeChanged();
                }));

        gLayout->addWidget(
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

                    GraphThemeManager::instance()
                        ->notifyThemeChanged();
                }));

        layout->addWidget(group);
    }

    /*
     * ARROW
     */

    {
        QGroupBox* group =
            new QGroupBox("Arrow");

        QVBoxLayout* gLayout =
            new QVBoxLayout(group);

        gLayout->addWidget(
            createIntEditor(
                "Width",
                theme.edge.normal.arrow.width,
                1,
                100,
                [](int v)
                {
                    auto& theme =
                        GraphThemeManager::instance()
                            ->mutableTheme();

                    theme.edge.normal.arrow.width = v;

                    GraphThemeManager::instance()
                        ->notifyThemeChanged();
                }));

        gLayout->addWidget(
            createIntEditor(
                "Height",
                theme.edge.normal.arrow.height,
                1,
                100,
                [](int v)
                {
                    auto& theme =
                        GraphThemeManager::instance()
                            ->mutableTheme();

                    theme.edge.normal.arrow.height = v;

                    GraphThemeManager::instance()
                        ->notifyThemeChanged();
                }));

        layout->addWidget(group);
    }

    /*
     * SAVE BUTTON
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
                        onChanged);

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
        onChanged);

    layout->addWidget(label);
    layout->addStretch();
    layout->addWidget(spin);

    return w;
}

QPushButton* ThemeEditorDock::makeColorButton(
    QColor initial,
    std::function<void(const QColor&)> onChanged)
{
    QPushButton* button =
        new QPushButton;

    button->setFixedSize(40, 20);

    auto updateButton =
        [button](QColor c)
        {
            button->setStyleSheet(
                QString(
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