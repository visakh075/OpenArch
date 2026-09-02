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
#include <QToolButton>
#include <QColorDialog>
#include <QScrollArea>
#include <QFrame>

ThemeEditorDock::ThemeEditorDock(
    QWidget* parent)
    : QDockWidget(parent)
{
    setWindowTitle("Theme Editor");

    QWidget* root =
        new QWidget;

    auto* rootLayout =
        new QHBoxLayout(root);

    rootLayout->setContentsMargins(0,0,0,0);

    m_tree =
        new QTreeWidget;

    m_tree->setHeaderHidden(true);
    m_tree->setMinimumWidth(240);

    m_stack =
        new QStackedWidget;

    rootLayout->addWidget(m_tree);
    rootLayout->addWidget(m_stack, 1);

    setWidget(root);

    populateTree();
    connectTree();
    
    // Connect to the manager's change signal:
    connect(GraphThemeManager::instance(), &GraphThemeManager::themeChanged,
            this, &ThemeEditorDock::syncFromTheme);

    // Initial populate
    syncFromTheme();

}

ThemeEditorDock::InspectorPage
ThemeEditorDock::createInspectorPage()
{
    InspectorPage page;

    page.content =
        new QWidget;

    page.layout =
        new QVBoxLayout(page.content);

    page.layout->setAlignment(Qt::AlignTop);

    QScrollArea* scroll =
        new QScrollArea;

    scroll->setWidget(page.content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    page.container =
        new QWidget;

    auto* layout =
        new QVBoxLayout(page.container);

    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(scroll);

    return page;
}

void ThemeEditorDock::populateTree()
{
    auto& theme =
        GraphThemeManager::instance()
            ->mutableTheme();

    auto addPage =
        [&](QTreeWidgetItem* item,
            
            auto builder)
        {
            auto page =
                createInspectorPage();

            builder(page.layout);

            int index =
                m_stack->addWidget(
                    page.container);

            item->setData(
                0,
                Qt::UserRole,
                index);
        };

    auto* viewItem =
        new QTreeWidgetItem(
            QStringList() << "View");

    m_tree->addTopLevelItem(viewItem);

    addPage(
        viewItem,
        [&](QVBoxLayout* layout)
        {
            buildViewProperties(layout);
        });

    auto* gridItem =
        new QTreeWidgetItem(
            QStringList() << "Grid");

    m_tree->addTopLevelItem(gridItem);

    addPage(
        gridItem,
        [&](QVBoxLayout* layout)
        {
            buildGridProperties(layout);
        });

    auto* nodeRoot =
        new QTreeWidgetItem(
            QStringList() << "Node");

    m_tree->addTopLevelItem(nodeRoot);

    struct NodeEntry
    {
        QString name;
        GraphNodeState* state;
    };

    QVector<NodeEntry> nodeStates =
    {
        { "Normal", &theme.node.normal },
        { "Hover", &theme.node.hover },
        { "Selected", &theme.node.selected }
    };

    for (auto& entry : nodeStates)
    {
        auto* item =
            new QTreeWidgetItem(
                QStringList() << entry.name);

        nodeRoot->addChild(item);

        addPage(
            item,
            [&](QVBoxLayout* layout)
            {
                buildNodeStateProperties(
                    *entry.state,
                    layout);
            });
    }

    auto* edgeRoot =
        new QTreeWidgetItem(
            QStringList() << "Edge");

    m_tree->addTopLevelItem(edgeRoot);

    struct EdgeEntry
    {
        QString name;
        GraphEdgeState* state;
    };

    QVector<EdgeEntry> edgeStates =
    {
        { "Normal", &theme.edge.normal },
        { "Hover", &theme.edge.hover },
        { "Selected", &theme.edge.selected }
    };

    for (auto& entry : edgeStates)
    {
        auto* item =
            new QTreeWidgetItem(
                QStringList() << entry.name);

        edgeRoot->addChild(item);

        addPage(
            item,
            [&](QVBoxLayout* layout)
            {
                buildEdgeStateProperties(
                    *entry.state,
                    layout);
            });
    }

    auto* portRoot =
        new QTreeWidgetItem(
            QStringList() << "Port");

    m_tree->addTopLevelItem(portRoot);

    struct PortEntry
    {
        QString name;
        GraphPortState* state;
    };

    QVector<PortEntry> portStates =
    {
        { "Normal", &theme.port.normal },
        { "Hover", &theme.port.hover },
        { "Selected", &theme.port.selected }
    };

    for (auto& entry : portStates)
    {
        auto* item =
            new QTreeWidgetItem(
                QStringList() << entry.name);

        portRoot->addChild(item);

        addPage(
            item,
            [&](QVBoxLayout* layout)
            {
                buildPortStateProperties(
                    *entry.state,
                    layout);
            });
    }

    auto* selectionItem =
        new QTreeWidgetItem(
            QStringList() << "Selection");

    m_tree->addTopLevelItem(selectionItem);

    addPage(
        selectionItem,
        [&](QVBoxLayout* layout)
        {
            buildSelectionProperties(layout);
        });

    auto* interactionItem =
        new QTreeWidgetItem(
            QStringList() << "Interaction");

    m_tree->addTopLevelItem(interactionItem);

    addPage(
        interactionItem,
        [&](QVBoxLayout* layout)
        {
            buildInteractionProperties(layout);
        });

    m_tree->expandAll();
}

void ThemeEditorDock::connectTree()
{
    connect(m_tree,
            &QTreeWidget::itemSelectionChanged,
            this,
            [this]()
            {
                auto items =
                    m_tree->selectedItems();

                if (items.isEmpty())
                    return;

                int index =
                    items.first()
                        ->data(0, Qt::UserRole)
                        .toInt();

                if (index >= 0 &&
                    index < m_stack->count())
                {
                    m_stack->setCurrentIndex(index);
                }
            });
}

QWidget* ThemeEditorDock::createCollapsibleSection(
    const QString& title,
    QVBoxLayout*& contentLayout)
{
    QWidget* root =
        new QWidget;

    auto* rootLayout =
        new QVBoxLayout(root);

    QToolButton* button =
        new QToolButton;

    button->setText(title);
    button->setCheckable(true);
    button->setChecked(true);

    QWidget* content =
        new QWidget;

    contentLayout =
        new QVBoxLayout(content);

    connect(button,
            &QToolButton::toggled,
            this,
            [=](bool checked)
            {
                content->setVisible(checked);
            });

    rootLayout->addWidget(button);
    rootLayout->addWidget(content);

    return root;
}

QPushButton* ThemeEditorDock::makeColorButton(
    const QColor& initial,
    std::function<void(const QColor&)> onChanged)
{
    QPushButton* btn =
        new QPushButton;

    btn->setMinimumHeight(24);

    QColor currentColor =
        initial;

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
                    "min-width: 40px;"
                    "}")
                    .arg(rgba));
        };

    applyColor(currentColor);

    connect(btn,
            &QPushButton::clicked,
            this,
            [=]() mutable
            {
                QColorDialog dialog(
                    currentColor,
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

                currentColor =
                    color;

                applyColor(currentColor);

                onChanged(currentColor);
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

    auto* section =
        createCollapsibleSection(
            title,
            layout);

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

    parentLayout->addWidget(section);
}

void ThemeEditorDock::buildArrowStateSection(
    const QString& title,
    GraphArrowState& state,
    QVBoxLayout* parentLayout)
{
    QVBoxLayout* layout = nullptr;

    auto* section =
        createCollapsibleSection(
            title,
            layout);

    layout->addWidget(
        createColorEditor(
            "Line Color",
            state.lineColor,
            [&](const QColor& c)
            {
                state.lineColor = c;
                emitThemeChanged();
            }));

    parentLayout->addWidget(section);
}

void ThemeEditorDock::buildEdgeLabelStyleSection(
    const QString& title,
    GraphEdgeLabelState& style,
    QVBoxLayout* parentLayout)
{
    QVBoxLayout* layout = nullptr;

    auto* section =
        createCollapsibleSection(
            title,
            layout);

    layout->addWidget(
        createColorEditor(
            "Text Color",
            style.textColor,
            [&](const QColor& c)
            {
                style.textColor = c;
                emitThemeChanged();
            }));

    parentLayout->addWidget(section);
}

void ThemeEditorDock::buildNodeStateProperties(
    GraphNodeState& state,
    QVBoxLayout* layout)
{
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

    buildTextStyleSection(
        "Title Text",
        state.title,
        layout);

    buildTextStyleSection(
        "Body Text",
        state.body,
        layout);
}

void ThemeEditorDock::buildEdgeStateProperties(
    GraphEdgeState& state,
    QVBoxLayout* layout)
{
    layout->addWidget(
        createColorEditor(
            "Line Color",
            state.lineColor,
            [&](const QColor& c)
            {
                state.lineColor = c;
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
}

void ThemeEditorDock::buildPortStateProperties(
    GraphPortState& state,
    QVBoxLayout* layout)
{
    layout->addWidget(
        createColorEditor(
            "Input",
            state.inputColor,
            [&](const QColor& c)
            {
                state.inputColor = c;
                emitThemeChanged();
            }));
}

void ThemeEditorDock::buildViewProperties(
    QVBoxLayout* layout)
{
    auto& theme =
        GraphThemeManager::instance()
            ->mutableTheme();

    layout->addWidget(
        createColorEditor(
            "Background",
            theme.view.background,
            [&](const QColor& c)
            {
                theme.view.background = c;
                emitThemeChanged();
            }));
}

void ThemeEditorDock::buildGridProperties(
    QVBoxLayout* layout)
{
    auto& grid =
        GraphThemeManager::instance()
            ->mutableTheme()
            .view.grid;

    layout->addWidget(
        createBoolEditor(
            "Enabled",
            grid.enabled,
            [&](bool v)
            {
                grid.enabled = v;
                emitThemeChanged();
            }));
}

void ThemeEditorDock::buildSelectionProperties(
    QVBoxLayout* layout)
{
    auto& selection =
        GraphThemeManager::instance()
            ->mutableTheme()
            .selection;

    layout->addWidget(
        createColorEditor(
            "Outline",
            selection.outline,
            [&](const QColor& c)
            {
                selection.outline = c;
                emitThemeChanged();
            }));
}

void ThemeEditorDock::buildInteractionProperties(
    QVBoxLayout* layout)
{
    auto& interaction =
        GraphThemeManager::instance()
            ->mutableTheme()
            .interaction;

    layout->addWidget(
        createColorEditor(
            "Hover Outline",
            interaction.hoverOutline,
            [&](const QColor& c)
            {
                interaction.hoverOutline = c;
                emitThemeChanged();
            }));
}

void ThemeEditorDock::emitThemeChanged()
{
    GraphThemeManager::instance()
        ->notifyThemeChanged();
}

void ThemeEditorDock::syncFromTheme()
{
    // Block signals to avoid feedback loops
    const bool oldState = blockSignals(true);

    // Save current active page index in stack
    int currentIndex = m_stack->currentIndex();

    // Clear existing pages in stacked widget
    while (m_stack->count() > 0)
    {
        QWidget* w = m_stack->widget(0);
        m_stack->removeWidget(w);
        delete w;
    }

    // Clear and rebuild tree items and inspector pages
    m_tree->clear();
    populateTree();

    // Restore selected page
    if (currentIndex >= 0 && currentIndex < m_stack->count())
    {
        m_stack->setCurrentIndex(currentIndex);
    }

    blockSignals(oldState);
}