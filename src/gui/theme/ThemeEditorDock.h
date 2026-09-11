#pragma once

#include <QDockWidget>
#include <QColor>

#include <functional>

class QPushButton;
class QVBoxLayout;
class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;

struct GraphNodeState;
struct GraphEdgeState;
struct GraphArrowState;
struct GraphTextStyle;
struct GraphEdgeLabelState;
struct GraphPortState;

class ThemeEditorDock : public QDockWidget
{
    Q_OBJECT

public:

    explicit ThemeEditorDock(
        QWidget* parent = nullptr);

private:

    struct InspectorPage
    {
        QWidget* container = nullptr;
        QWidget* content = nullptr;
        QVBoxLayout* layout = nullptr;
    };

    QTreeWidget* m_tree = nullptr;

    QStackedWidget* m_stack = nullptr;

    void populateTree();

    void connectTree();

    InspectorPage createInspectorPage();

    QWidget* createCollapsibleSection(
        const QString& title,
        QVBoxLayout*& contentLayout);

    QPushButton* makeColorButton(
        const QColor& initial,
        std::function<void(const QColor&)> onChanged);

    QWidget* createColorEditor(
        const QString& title,
        const QColor& initial,
        std::function<void(const QColor&)> onChanged);

    QWidget* createIntEditor(
        const QString& title,
        int value,
        int min,
        int max,
        std::function<void(int)> onChanged);

    QWidget* createBoolEditor(
        const QString& title,
        bool value,
        std::function<void(bool)> onChanged);

    void buildTextStyleSection(
        const QString& title,
        GraphTextStyle& style,
        QVBoxLayout* parentLayout);

    void buildArrowStateSection(
        const QString& title,
        GraphArrowState& state,
        QVBoxLayout* parentLayout);

    void buildEdgeLabelStyleSection(
        const QString& title,
        GraphEdgeLabelState& style,
        QVBoxLayout* parentLayout);

    void buildNodeStateProperties(
        GraphNodeState& state,
        QVBoxLayout* layout);

    void buildEdgeStateProperties(
        GraphEdgeState& state,
        QVBoxLayout* layout);

    void buildPortStateProperties(
        GraphPortState& state,
        QVBoxLayout* layout);

    void buildViewProperties(
        QVBoxLayout* layout);

    void buildGridProperties(
        QVBoxLayout* layout);

    void buildSelectionProperties(
        QVBoxLayout* layout);

    void buildInteractionProperties(
        QVBoxLayout* layout);

    void emitThemeChanged();

    public slots:
    void syncFromTheme();
};
