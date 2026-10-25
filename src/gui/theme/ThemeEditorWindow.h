#pragma once

#include <QMainWindow>
#include <QColor>
#include <QString>
#include <QMap>
#include <memory>
#include <functional>

#include "GraphTheme.h"
#include "GraphThemeManager.h"
#include "ArchitectureModel.h"
#include "DbManagerSQLite.h"

class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QLineEdit;
class QLabel;
class QPushButton;
class QToolBar;
class QVBoxLayout;
class QAction;
class GraphView;
class QGraphicsScene;
class GraphNodeItem;
class GraphEdgeItem;
class QScrollArea;

struct StyleClipboard
{
    enum class Kind {
        None,
        ComponentTheme,
        ComponentState,
        EdgeTheme,
        EdgeState
    };

    Kind kind = Kind::None;
    QString sourceName;
    GraphComponentTheme componentTheme;
    GraphComponentState componentState;
    GraphEdgeTheme edgeTheme;
    GraphEdgeState edgeState;
};

class ThemeEditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ThemeEditorWindow(QWidget* parent = nullptr);
    ~ThemeEditorWindow() override;

    bool loadTheme(const QString& filePath);
    bool saveTheme();
    bool saveThemeAs();
    void resetToDefaultTheme();

public slots:
    // Style management
    void addNewStyle();
    void duplicateSelectedStyle();
    void removeSelectedStyle();
    void copySelectedStyle();
    void pasteSelectedStyle();

    // Tree & Live View
    void refreshTree();
    void refreshLiveView();
    void resetLiveViewScene();
    void addTestNodeToLiveView();
    void addTestEdgeToLiveView();

    void zoomIn();
    void zoomOut();
    void zoomFit();

private:
    enum class ItemCategory {
        General,
        View,
        Grid,
        Port,
        Selection,
        Interaction,
        NodeBase,
        NodeBaseState,
        NodeTypeRoot,
        NodeTypeState,
        ContainerBase,
        ContainerBaseState,
        ContainerTypeRoot,
        ContainerTypeState,
        EdgeBase,
        EdgeBaseState,
        EdgePreviewLine,
        EdgeTypeRoot,
        EdgeTypeState
    };

    struct TreeItemData {
        ItemCategory category;
        QString typeName;       // For type-specific styles (e.g. "pwr", "lan")
        int stateKind = 0;       // 0: Normal, 1: Hover, 2: Selected
        int stackIndex = -1;
    };

    struct InspectorPage {
        QWidget* container = nullptr;
        QWidget* content = nullptr;
        QVBoxLayout* layout = nullptr;
    };

    void setupUi();
    void setupMenusAndToolbars();
    void setupLiveView();
    void populateLiveScene();

    InspectorPage createInspectorPage();

    QWidget* createSection(
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
        int kind = 0,
        const QString& typeName = QString());

    void buildEdgeStateProperties(
        GraphEdgeState& state,
        QVBoxLayout* layout,
        int kind = 0,
        const QString& typeName = QString());

    void buildPreviewLineProperties(
        GraphPreviewLineTheme& preview,
        QVBoxLayout* layout);

    void buildPortStateProperties(
        GraphPortState& state,
        QVBoxLayout* layout);

    void buildViewProperties(QVBoxLayout* layout);
    void buildGridProperties(QVBoxLayout* layout);
    void buildSelectionProperties(QVBoxLayout* layout);
    void buildInteractionProperties(QVBoxLayout* layout);
    void buildGeneralProperties(QVBoxLayout* layout);

    void buildTypeOverviewProperties(
        QVBoxLayout* layout,
        bool isEdge,
        bool isContainer,
        const QString& typeName);

    void emitThemeChanged();
    void updateStatusDisplay(const QString& message = QString());
    void updateActionStates();

    // UI elements
    QTreeWidget* tree_{nullptr};
    QLineEdit* searchEdit_{nullptr};
    QStackedWidget* stack_{nullptr};

    // Live View elements
    QVBoxLayout* liveContainerLayout_{nullptr};
    QGraphicsScene* liveScene_{nullptr};
    GraphView* liveView_{nullptr};
    std::unique_ptr<DbManagerSQLite> liveDb_;
    std::unique_ptr<ArchitectureModel> liveModel_;
    QMap<QString, NodeId> liveNodeMap_;
    QList<EdgeId> liveEdgeIds_;

    // Actions & buttons
    QAction* actNewTheme_{nullptr};
    QAction* actOpenTheme_{nullptr};
    QAction* actSaveTheme_{nullptr};
    QAction* actSaveThemeAs_{nullptr};
    QAction* actResetDefaults_{nullptr};

    QAction* actAddStyle_{nullptr};
    QAction* actDuplicateStyle_{nullptr};
    QAction* actRemoveStyle_{nullptr};
    QAction* actCopyStyle_{nullptr};
    QAction* actPasteStyle_{nullptr};

    QPushButton* btnAddStyle_{nullptr};
    QPushButton* btnDuplicateStyle_{nullptr};
    QPushButton* btnRemoveStyle_{nullptr};
    QPushButton* btnCopyStyle_{nullptr};
    QPushButton* btnPasteStyle_{nullptr};

    QLabel* statusLabel_{nullptr};
    StyleClipboard clipboard_;
    bool isInternalUpdate_{false};
};
