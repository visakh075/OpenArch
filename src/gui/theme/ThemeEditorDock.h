#pragma once

#include <QDockWidget>
#include <QColor>
#include <functional>

#include "GraphTheme.h"

class QPushButton;
class QVBoxLayout;
class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QComboBox;
class QLineEdit;
class QLabel;

class ThemeEditorDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit ThemeEditorDock(QWidget* parent = nullptr);

public slots:
    void syncFromTheme();
    void saveTheme();
    void saveThemeAs();

private:
    struct InspectorPage
    {
        QWidget* container = nullptr;
        QWidget* content = nullptr;
        QVBoxLayout* layout = nullptr;
    };

    QTreeWidget* m_tree = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QStackedWidget* m_stack = nullptr;
    bool m_isInternalUpdate = false;

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

    QWidget* createPenStyleEditor(
        const QString& title,
        Qt::PenStyle value,
        std::function<void(Qt::PenStyle)> onChanged);

    void buildTextStyleSection(
        const QString& title,
        GraphTextStyle& style,
        QVBoxLayout* parentLayout,
        std::function<void()> onUpdate = nullptr);

    void buildArrowStateSection(
        const QString& title,
        GraphArrowState& state,
        QVBoxLayout* parentLayout,
        std::function<void()> onUpdate = nullptr);

    void buildEdgeLabelStyleSection(
        const QString& title,
        GraphEdgeLabelState& style,
        QVBoxLayout* parentLayout,
        std::function<void()> onUpdate = nullptr);

    void buildComponentStateProperties(
        GraphComponentState& state,
        QVBoxLayout* layout,
        bool isContainer,
        int kind = 0);

    void buildEdgeStateProperties(
        GraphEdgeState& state,
        QVBoxLayout* layout,
        int kind = 0);

    void buildPreviewLineProperties(
        GraphPreviewLineTheme& preview,
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

    void buildGeneralProperties(
        QVBoxLayout* layout);

    void emitThemeChanged();
    void updateStatusDisplay(const QString& message = QString());

private:
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_saveAsBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
};