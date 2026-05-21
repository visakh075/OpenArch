#pragma once

#include <QDockWidget>
#include <QColor>
#include <QString>
#include <QWidget>

#include <functional>

class QPushButton;
class QVBoxLayout;
class QGroupBox;
class QToolBox;

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

    /*
     * ROOT BUILDERS
     */

    void buildViewSection(
        QVBoxLayout* parentLayout);

    void buildGridSection(
        QVBoxLayout* parentLayout);

    void buildNodeSection(
        QVBoxLayout* parentLayout);

    void buildEdgeSection(
        QVBoxLayout* parentLayout);

    void buildPortSection(
        QVBoxLayout* parentLayout);

    void buildSelectionSection(
        QVBoxLayout* parentLayout);

    void buildInteractionSection(
        QVBoxLayout* parentLayout);

    /*
     * STATE BUILDERS
     */

    void buildNodeStateSection(
        const QString& title,
        GraphNodeState& state,
        QVBoxLayout* parentLayout);

    void buildEdgeStateSection(
        const QString& title,
        GraphEdgeState& state,
        QVBoxLayout* parentLayout);

    void buildArrowStateSection(
        const QString& title,
        GraphArrowState& state,
        QVBoxLayout* parentLayout);

    void buildPortStateSection(
        const QString& title,
        GraphPortState& state,
        QVBoxLayout* parentLayout);

    /*
     * STYLE BUILDERS
     */

    void buildTextStyleSection(
        const QString& title,
        GraphTextStyle& style,
        QVBoxLayout* parentLayout);

    void buildEdgeLabelStyleSection(
        const QString& title,
        GraphEdgeLabelState& style,
        QVBoxLayout* parentLayout);

    /*
     * HELPERS
     */

    QGroupBox* createSection(
        const QString& title,
        QVBoxLayout*& outLayout);

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

    QPushButton* makeColorButton(
        const QColor& initial,
        std::function<void(const QColor&)> onChanged);

    /*
     * UTILITIES
     */

    QWidget* createToolBoxPage(
        QToolBox* toolBox,
        const QString& title);

    void emitThemeChanged();
};
