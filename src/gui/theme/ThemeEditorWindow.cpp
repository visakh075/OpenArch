#include "ThemeEditorWindow.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QToolButton>
#include <QColorDialog>
#include <QScrollArea>
#include <QFrame>
#include <QVector>
#include <QPainter>
#include <QPainterPath>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileInfo>
#include <QDir>
#include <cmath>

#include "GraphView.h"
#include "GraphModules/GraphNodeItem.h"
#include "GraphModules/GraphEdgeItem.h"

/*
 * =========================================================
 * PREVIEW WIDGETS WITH TYPE-SPECIFIC STYLING
 * =========================================================
 */

enum class ComponentStateKind { Normal, Hover, Selected };

class StandaloneComponentPreview : public QWidget
{
public:
    StandaloneComponentPreview(ComponentStateKind kind, bool isContainer, const QString& typeName = QString(), QWidget* parent = nullptr)
        : QWidget(parent), m_kind(kind), m_isContainer(isContainer), m_typeName(typeName)
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
        const auto& group = m_isContainer
            ? theme.containerThemeForType(m_typeName)
            : theme.nodeThemeForType(m_typeName);

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
            QString tStr = m_typeName.isEmpty() ? "Container (Base)" : QString("Container [%1]").arg(m_typeName);
            p.drawText(headerRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, tStr);

            QRectF bodyArea(card.left(), headerRect.bottom(), card.width(), card.bottom() - headerHeight);
            QFont bodyFont;
            bodyFont.setPointSize(state->body.size > 0 ? state->body.size : 11);
            bodyFont.setBold(state->body.bold);
            bodyFont.setItalic(state->body.italic);
            p.setFont(bodyFont);
            p.setPen(state->body.color);
            p.drawText(bodyArea.adjusted(0, 4, 0, -18), state->body.align, "Component Group");

            QFont tertFont;
            tertFont.setPointSize(state->tertiary.size > 0 ? state->tertiary.size : 9);
            tertFont.setBold(state->tertiary.bold);
            tertFont.setItalic(state->tertiary.italic);
            p.setFont(tertFont);
            p.setPen(state->tertiary.color);
            p.drawText(bodyArea.adjusted(0, 22, 0, -2), state->tertiary.align, "(3 components)");
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
            QString nStr = m_typeName.isEmpty() ? "Node (Base)" : QString("Node [%1]").arg(m_typeName);
            p.drawText(titleRect, state->title.align, nStr);

            p.setFont(bodyFont);
            p.setPen(state->body.color);
            p.drawText(bodyRect, state->body.align, QString("Type: %1").arg(m_typeName.isEmpty() ? "default" : m_typeName));
        }
    }

private:
    ComponentStateKind m_kind;
    bool m_isContainer;
    QString m_typeName;
};

class StandaloneEdgePreview : public QWidget
{
public:
    StandaloneEdgePreview(ComponentStateKind kind, const QString& typeName = QString(), QWidget* parent = nullptr)
        : QWidget(parent), m_kind(kind), m_typeName(typeName)
    {
        setFixedHeight(90);
    }

    void refresh() { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        p.fillRect(rect(), QColor("#1a1a1a"));

        const auto& theme = GraphThemeManager::instance()->theme();
        const auto& group = theme.edgeThemeForType(m_typeName);

        const GraphEdgeState* state = &group.normal;
        if (m_kind == ComponentStateKind::Hover) state = &group.hover;
        else if (m_kind == ComponentStateKind::Selected) state = &group.selected;

        QPointF start(30, height() / 2.0);
        QPointF end(width() - 40, height() / 2.0);

        QPen linePen(state->lineColor, state->lineWidth, state->lineStyle);
        if (state->lineStyle == Qt::CustomDashLine && !state->dashPattern.isEmpty())
            linePen.setDashPattern(state->dashPattern);

        p.setPen(linePen);
        p.drawLine(start, end);

        // Draw Arrow
        qreal arrowW = state->arrow.width > 0 ? state->arrow.width : 14;
        qreal arrowH = state->arrow.height > 0 ? state->arrow.height : 10;
        QPainterPath arrowPath;
        arrowPath.moveTo(end);
        arrowPath.lineTo(end.x() - arrowW, end.y() - arrowH / 2.0);
        arrowPath.lineTo(end.x() - arrowW, end.y() + arrowH / 2.0);
        arrowPath.closeSubpath();

        p.setPen(QPen(state->arrow.lineColor, state->arrow.lineWidth > 0 ? state->arrow.lineWidth : 2));
        p.setBrush(state->arrow.fillColor);
        p.drawPath(arrowPath);

        // Draw Label
        QString labelText = m_typeName.isEmpty() ? "edge (base)" : QString("edge#%1").arg(m_typeName);
        QFont labelFont;
        labelFont.setPointSize(state->label.fontSize > 0 ? state->label.fontSize : 10);
        labelFont.setBold(state->label.bold);
        QFontMetrics fm(labelFont);
        QRect labelTextRect = fm.boundingRect(labelText);

        qreal boxW = labelTextRect.width() + state->label.paddingX * 2 + 10;
        qreal boxH = labelTextRect.height() + state->label.paddingY * 2 + 6;
        QRectF labelBox(width() / 2.0 - boxW / 2.0, (height() / 2.0) - boxH - state->label.offset, boxW, boxH);

        QPainterPath boxPath;
        boxPath.addRoundedRect(labelBox, state->label.radius, state->label.radius);
        p.setPen(QPen(state->label.borderColor, state->label.borderWidth));
        p.setBrush(state->label.backgroundColor.alpha() > 0 ? state->label.backgroundColor : QColor("#222222"));
        p.drawPath(boxPath);

        p.setFont(labelFont);
        p.setPen(state->label.textColor);
        p.drawText(labelBox, Qt::AlignCenter, labelText);
    }

private:
    ComponentStateKind m_kind;
    QString m_typeName;
};

/*
 * =========================================================
 * ADD STYLE DIALOG
 * =========================================================
 */

class AddStyleDialog : public QDialog
{
public:
    AddStyleDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Add Named / Type Style");
        setMinimumWidth(360);

        auto* layout = new QVBoxLayout(this);
        auto* form = new QFormLayout;

        targetCombo_ = new QComboBox(this);
        targetCombo_->addItem("Node Style (node#<type>)", 0);
        targetCombo_->addItem("Container Style (container#<type>)", 1);
        targetCombo_->addItem("Edge Style (edge#<type>)", 2);

        nameEdit_ = new QLineEdit(this);
        nameEdit_->setPlaceholderText("e.g. pwr, lan, db, sensor, cloud");

        templateCombo_ = new QComboBox(this);
        updateTemplates();

        connect(targetCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &AddStyleDialog::updateTemplates);

        form->addRow("Style Target:", targetCombo_);
        form->addRow("Type / Name:", nameEdit_);
        form->addRow("Copy From Template:", templateCombo_);
        layout->addLayout(form);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            QString name = nameEdit_->text().trimmed().toLower();
            if (name.isEmpty()) {
                QMessageBox::warning(this, "Invalid Name", "Style type name cannot be empty.");
                return;
            }
            if (name.contains('#') || name.contains(' ')) {
                QMessageBox::warning(this, "Invalid Name", "Style type name should not contain spaces or '#' symbols.");
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    int targetType() const { return targetCombo_->currentData().toInt(); }
    QString styleName() const { return nameEdit_->text().trimmed().toLower(); }
    QString templateChoice() const { return templateCombo_->currentText(); }

private:
    void updateTemplates()
    {
        templateCombo_->clear();
        templateCombo_->addItem("Base Style (Default)");

        const auto& theme = GraphThemeManager::instance()->theme();
        int t = targetCombo_->currentData().toInt();
        if (t == 0) {
            for (const auto& k : theme.nodeTypeThemes.keys())
                templateCombo_->addItem(QString("node#%1").arg(k));
        } else if (t == 1) {
            for (const auto& k : theme.containerTypeThemes.keys())
                templateCombo_->addItem(QString("container#%1").arg(k));
        } else {
            for (const auto& k : theme.edgeTypeThemes.keys())
                templateCombo_->addItem(QString("edge#%1").arg(k));
        }
    }

    QComboBox* targetCombo_;
    QLineEdit* nameEdit_;
    QComboBox* templateCombo_;
};

/*
 * =========================================================
 * THEME EDITOR WINDOW IMPLEMENTATION
 * =========================================================
 */

ThemeEditorWindow::ThemeEditorWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("OpenArch Theme Editor");
    resize(1280, 820);

    setupUi();
    setupMenusAndToolbars();
    setupLiveView();

    refreshTree();
    populateLiveScene();

    connect(GraphThemeManager::instance(), &GraphThemeManager::themeChanged,
            this, &ThemeEditorWindow::refreshLiveView);

    connect(GraphThemeManager::instance(), &GraphThemeManager::themeSaved,
            this, [this](const QString& path) {
        updateStatusDisplay(QString("Theme saved to %1").arg(QFileInfo(path).fileName()));
    });

    connect(GraphThemeManager::instance(), &GraphThemeManager::themeLoaded,
            this, [this](const QString& path) {
        updateStatusDisplay(QString("Loaded %1").arg(QFileInfo(path).fileName()));
        refreshTree();
        populateLiveScene();
    });

    updateStatusDisplay();
}

ThemeEditorWindow::~ThemeEditorWindow() = default;

void ThemeEditorWindow::setupUi()
{
    auto* central = new QWidget(this);
    auto* centralLayout = new QHBoxLayout(central);
    centralLayout->setContentsMargins(4, 4, 4, 4);
    centralLayout->setSpacing(4);

    auto* mainSplitter = new QSplitter(Qt::Horizontal, central);

    // ==========================================
    // Left Panel: Styles Tree & Management
    // ==========================================
    auto* leftContainer = new QWidget(mainSplitter);
    auto* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(2, 2, 2, 2);
    leftLayout->setSpacing(4);

    searchEdit_ = new QLineEdit(leftContainer);
    searchEdit_->setPlaceholderText("Filter styles...");
    searchEdit_->setClearButtonEnabled(true);

    tree_ = new QTreeWidget(leftContainer);
    tree_->setHeaderHidden(true);
    tree_->setMinimumWidth(220);

    // Tree action buttons
    auto* btnRow1 = new QHBoxLayout;
    btnRow1->setSpacing(4);
    btnAddStyle_ = new QPushButton("+ Add Style", leftContainer);
    btnAddStyle_->setToolTip("Add a new type-specific or named style (e.g. edge#pwr, node#lan)");
    btnDuplicateStyle_ = new QPushButton("Duplicate", leftContainer);
    btnDuplicateStyle_->setToolTip("Duplicate currently selected style");
    btnRemoveStyle_ = new QPushButton("Remove", leftContainer);
    btnRemoveStyle_->setToolTip("Remove selected type style");
    btnRow1->addWidget(btnAddStyle_);
    btnRow1->addWidget(btnDuplicateStyle_);
    btnRow1->addWidget(btnRemoveStyle_);

    auto* btnRow2 = new QHBoxLayout;
    btnRow2->setSpacing(4);
    btnCopyStyle_ = new QPushButton("Copy Style", leftContainer);
    btnCopyStyle_->setToolTip("Copy properties of selected style");
    btnPasteStyle_ = new QPushButton("Paste Style", leftContainer);
    btnPasteStyle_->setToolTip("Paste copied properties into selected style");
    btnPasteStyle_->setEnabled(false);
    btnRow2->addWidget(btnCopyStyle_);
    btnRow2->addWidget(btnPasteStyle_);

    leftLayout->addWidget(searchEdit_);
    leftLayout->addWidget(tree_, 1);
    leftLayout->addLayout(btnRow1);
    leftLayout->addLayout(btnRow2);

    connect(btnAddStyle_, &QPushButton::clicked, this, &ThemeEditorWindow::addNewStyle);
    connect(btnDuplicateStyle_, &QPushButton::clicked, this, &ThemeEditorWindow::duplicateSelectedStyle);
    connect(btnRemoveStyle_, &QPushButton::clicked, this, &ThemeEditorWindow::removeSelectedStyle);
    connect(btnCopyStyle_, &QPushButton::clicked, this, &ThemeEditorWindow::copySelectedStyle);
    connect(btnPasteStyle_, &QPushButton::clicked, this, &ThemeEditorWindow::pasteSelectedStyle);

    // ==========================================
    // Center Panel: Property Inspector
    // ==========================================
    stack_ = new QStackedWidget(mainSplitter);
    stack_->setMinimumWidth(320);

    // ==========================================
    // Right Panel: Live View Canvas
    // ==========================================
    auto* rightContainer = new QWidget(mainSplitter);
    liveContainerLayout_ = new QVBoxLayout(rightContainer);
    liveContainerLayout_->setContentsMargins(2, 2, 2, 2);
    liveContainerLayout_->setSpacing(4);

    // Live View Header Toolbar
    auto* liveHeader = new QWidget(rightContainer);
    auto* liveHeaderLayout = new QHBoxLayout(liveHeader);
    liveHeaderLayout->setContentsMargins(4, 2, 4, 2);
    liveHeaderLayout->setSpacing(6);

    auto* liveTitle = new QLabel("<b>Interactive Live View</b>", liveHeader);
    liveHeaderLayout->addWidget(liveTitle);
    liveHeaderLayout->addStretch();

    auto* btnZoomIn = new QPushButton("+", liveHeader);
    btnZoomIn->setFixedSize(26, 24);
    btnZoomIn->setToolTip("Zoom In");
    auto* btnZoomOut = new QPushButton("-", liveHeader);
    btnZoomOut->setFixedSize(26, 24);
    btnZoomOut->setToolTip("Zoom Out");
    auto* btnZoomFit = new QPushButton("Fit", liveHeader);
    btnZoomFit->setFixedHeight(24);
    btnZoomFit->setToolTip("Fit View");

    auto* btnAddTestNode = new QPushButton("+ Node", liveHeader);
    btnAddTestNode->setFixedHeight(24);
    btnAddTestNode->setToolTip("Add a test node to canvas with selected style type");

    auto* btnResetScene = new QPushButton("Reset Scene", liveHeader);
    btnResetScene->setFixedHeight(24);
    btnResetScene->setToolTip("Reset Live View sample diagram");

    liveHeaderLayout->addWidget(btnZoomIn);
    liveHeaderLayout->addWidget(btnZoomOut);
    liveHeaderLayout->addWidget(btnZoomFit);
    liveHeaderLayout->addWidget(btnAddTestNode);
    liveHeaderLayout->addWidget(btnResetScene);

    connect(btnZoomIn, &QPushButton::clicked, this, &ThemeEditorWindow::zoomIn);
    connect(btnZoomOut, &QPushButton::clicked, this, &ThemeEditorWindow::zoomOut);
    connect(btnZoomFit, &QPushButton::clicked, this, &ThemeEditorWindow::zoomFit);
    connect(btnAddTestNode, &QPushButton::clicked, this, &ThemeEditorWindow::addTestNodeToLiveView);
    connect(btnResetScene, &QPushButton::clicked, this, &ThemeEditorWindow::resetLiveViewScene);

    liveContainerLayout_->addWidget(liveHeader);

    mainSplitter->addWidget(leftContainer);
    mainSplitter->addWidget(stack_);
    mainSplitter->addWidget(rightContainer);

    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 2);
    mainSplitter->setStretchFactor(2, 3);

    centralLayout->addWidget(mainSplitter);
    setCentralWidget(central);

    // Status bar
    statusLabel_ = new QLabel("Ready");
    statusBar()->addWidget(statusLabel_, 1);

    // Tree connections
    connect(tree_, &QTreeWidget::itemSelectionChanged, this, [this]() {
        auto items = tree_->selectedItems();
        if (items.isEmpty()) return;

        int index = items.first()->data(0, Qt::UserRole).toInt();
        if (index >= 0 && index < stack_->count()) {
            stack_->setCurrentIndex(index);
        }
        updateActionStates();
    });

    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        QString query = text.trimmed();
        for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
            auto* top = tree_->topLevelItem(i);
            bool topMatches = top->text(0).contains(query, Qt::CaseInsensitive);
            bool anyChildMatches = false;
            for (int j = 0; j < top->childCount(); ++j) {
                auto* child = top->child(j);
                bool childMatches = query.isEmpty() || child->text(0).contains(query, Qt::CaseInsensitive);
                child->setHidden(!query.isEmpty() && !childMatches && !topMatches);
                if (childMatches) anyChildMatches = true;
            }
            top->setHidden(!query.isEmpty() && !topMatches && !anyChildMatches);
            if (!query.isEmpty() && (topMatches || anyChildMatches)) {
                top->setExpanded(true);
            }
        }
    });
}

void ThemeEditorWindow::setupMenusAndToolbars()
{
    // ==========================================
    // File Menu
    // ==========================================
    auto* fileMenu = menuBar()->addMenu("&File");

    actNewTheme_ = fileMenu->addAction("&New Theme (Reset)", this, &ThemeEditorWindow::resetToDefaultTheme);
    actOpenTheme_ = fileMenu->addAction("&Open Theme...", this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Open Theme JSON", "src/gui/theme/themes", "Theme Files (*.json)");
        if (!path.isEmpty()) loadTheme(path);
    });
    fileMenu->addSeparator();
    actSaveTheme_ = fileMenu->addAction("&Save Theme", this, &ThemeEditorWindow::saveTheme);
    actSaveThemeAs_ = fileMenu->addAction("Save Theme &As...", this, &ThemeEditorWindow::saveThemeAs);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close);

    // ==========================================
    // Edit Menu
    // ==========================================
    auto* editMenu = menuBar()->addMenu("&Edit");
    actAddStyle_ = editMenu->addAction("&Add Style...", this, &ThemeEditorWindow::addNewStyle);
    actAddStyle_->setShortcut(QKeySequence::New);
    actAddStyle_->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    actDuplicateStyle_ = editMenu->addAction("&Duplicate Style...", this, &ThemeEditorWindow::duplicateSelectedStyle);
    actDuplicateStyle_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
    actDuplicateStyle_->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    actRemoveStyle_ = editMenu->addAction("&Remove Style", this, &ThemeEditorWindow::removeSelectedStyle);
    actRemoveStyle_->setShortcut(QKeySequence::Delete);
    actRemoveStyle_->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    editMenu->addSeparator();
    actCopyStyle_ = editMenu->addAction("&Copy Style Properties", this, &ThemeEditorWindow::copySelectedStyle);
    actCopyStyle_->setShortcut(QKeySequence::Copy);
    actCopyStyle_->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    actPasteStyle_ = editMenu->addAction("&Paste Style Properties", this, &ThemeEditorWindow::pasteSelectedStyle);
    actPasteStyle_->setShortcut(QKeySequence::Paste);
    actPasteStyle_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    actPasteStyle_->setEnabled(false);

    // ==========================================
    // Presets Menu
    // ==========================================
    auto* presetsMenu = menuBar()->addMenu("&Presets");
    presetsMenu->addAction("Load Dark Theme", this, [this]() {
        loadTheme("src/gui/theme/themes/Dark.json");
    });
    presetsMenu->addAction("Load Light Theme", this, [this]() {
        loadTheme("src/gui/theme/themes/Light.json");
    });

    // ==========================================
    // View Menu
    // ==========================================
    auto* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("Zoom In", this, &ThemeEditorWindow::zoomIn, QKeySequence::ZoomIn);
    viewMenu->addAction("Zoom Out", this, &ThemeEditorWindow::zoomOut, QKeySequence::ZoomOut);
    viewMenu->addAction("Fit to View", this, &ThemeEditorWindow::zoomFit);
    viewMenu->addSeparator();
    viewMenu->addAction("Reset Live View Scene", this, &ThemeEditorWindow::resetLiveViewScene);

    // ==========================================
    // Toolbar
    // ==========================================
    auto* toolbar = addToolBar("Theme Actions");
    toolbar->addAction(actOpenTheme_);
    toolbar->addAction(actSaveTheme_);
    toolbar->addAction(actSaveThemeAs_);
    toolbar->addSeparator();
    toolbar->addAction(actAddStyle_);
    toolbar->addAction(actDuplicateStyle_);
    toolbar->addAction(actRemoveStyle_);
    toolbar->addSeparator();
    toolbar->addAction(actCopyStyle_);
    toolbar->addAction(actPasteStyle_);
}

void ThemeEditorWindow::setupLiveView()
{
    liveDb_ = std::make_unique<DbManagerSQLite>();
    liveDb_->open(":memory:");
    liveModel_ = std::make_unique<ArchitectureModel>(*liveDb_);

    liveScene_ = new QGraphicsScene(this);
    liveView_ = new GraphView(this);
    liveView_->setScene(liveScene_);
    liveView_->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    liveView_->setRenderHint(QPainter::Antialiasing);
    liveView_->setInteractive(true);
    liveView_->setDragMode(QGraphicsView::RubberBandDrag);

    // Connect item selection from live view to select corresponding style in tree
    connect(liveScene_, &QGraphicsScene::selectionChanged, this, [this]() {
        auto items = liveScene_->selectedItems();
        if (items.isEmpty()) return;

        if (auto* node = dynamic_cast<GraphNodeItem*>(items.first())) {
            QString type = node->displayType().toLower();
            bool isCont = node->isContainer();
            // Search in tree for node/container type
            for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
                auto* top = tree_->topLevelItem(i);
                if ((isCont && top->text(0) == "Containers") || (!isCont && top->text(0) == "Nodes")) {
                    for (int j = 0; j < top->childCount(); ++j) {
                        auto* child = top->child(j);
                        if (child->text(0).contains(type, Qt::CaseInsensitive) ||
                            (type == "default" && child->text(0).contains("Base", Qt::CaseInsensitive))) {
                            tree_->setCurrentItem(child);
                            return;
                        }
                    }
                }
            }
        } else if (auto* edge = dynamic_cast<GraphEdgeItem*>(items.first())) {
            QString type = edge->edgeType().toLower();
            for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
                auto* top = tree_->topLevelItem(i);
                if (top->text(0) == "Edges") {
                    for (int j = 0; j < top->childCount(); ++j) {
                        auto* child = top->child(j);
                        if (child->text(0).contains(type, Qt::CaseInsensitive) ||
                            (type.isEmpty() && child->text(0).contains("Base", Qt::CaseInsensitive))) {
                            tree_->setCurrentItem(child);
                            return;
                        }
                    }
                }
            }
        }
    });

    // Add liveView_ to liveContainerLayout_
    if (liveContainerLayout_) {
        liveContainerLayout_->addWidget(liveView_, 1);
    }
}

void ThemeEditorWindow::populateLiveScene()
{
    if (!liveScene_ || !liveModel_) return;

    liveScene_->clear();
    liveNodeMap_.clear();
    liveEdgeIds_.clear();

    const auto& theme = GraphThemeManager::instance()->theme();

    // 1. Container: Rack A
    NodeData rackData;
    rackData.name = "Rack Subsystem";
    rackData.type = theme.containerTypeThemes.isEmpty() ? "container" : theme.containerTypeThemes.firstKey().toStdString();
    rackData.metadata = R"({"_width": 330, "_height": 250, "_dim_lock": true})";
    NodeId rackId = 0;
    liveModel_->addNode(rackData, rackId);
    auto* rackItem = new GraphNodeItem(liveModel_.get(), rackId);
    liveScene_->addItem(rackItem);
    rackItem->setPos(40, 40);
    rackItem->setContainerSizing(GraphNodeItem::ContainerSizing::Manual);
    rackItem->setManualContainerSize(330, 250);
    rackItem->refreshGeometry();
    liveNodeMap_["rack"] = rackId;

    // 2. Node inside container: Controller (Base)
    NodeData ctlData;
    ctlData.name = "Core Controller";
    ctlData.type = "default";
    ctlData.parentId = rackId;
    NodeId ctlId = 0;
    liveModel_->addNode(ctlData, ctlId);
    auto* ctlItem = new GraphNodeItem(liveModel_.get(), ctlId);
    liveScene_->addItem(ctlItem);
    ctlItem->setPos(70, 90);
    ctlItem->refreshGeometry();
    liveNodeMap_["controller"] = ctlId;

    // 3. Node inside container: Power Unit (Type: pwr)
    NodeData pwrData;
    pwrData.name = "Power Distribution";
    pwrData.type = "pwr";
    pwrData.parentId = rackId;
    NodeId pwrId = 0;
    liveModel_->addNode(pwrData, pwrId);
    auto* pwrItem = new GraphNodeItem(liveModel_.get(), pwrId);
    liveScene_->addItem(pwrItem);
    pwrItem->setPos(210, 90);
    pwrItem->refreshGeometry();
    liveNodeMap_["power"] = pwrId;

    // 4. Outside node: Gateway (Type: lan)
    NodeData lanData;
    lanData.name = "Network Switch";
    lanData.type = "lan";
    NodeId lanId = 0;
    liveModel_->addNode(lanData, lanId);
    auto* lanItem = new GraphNodeItem(liveModel_.get(), lanId);
    liveScene_->addItem(lanItem);
    lanItem->setPos(430, 90);
    lanItem->refreshGeometry();
    liveNodeMap_["lan"] = lanId;

    // 5. Outside node: Database
    NodeData dbData;
    dbData.name = "Data Storage";
    dbData.type = "db";
    NodeId dbId = 0;
    liveModel_->addNode(dbData, dbId);
    auto* dbItem = new GraphNodeItem(liveModel_.get(), dbId);
    liveScene_->addItem(dbItem);
    dbItem->setPos(430, 230);
    dbItem->refreshGeometry();
    liveNodeMap_["db"] = dbId;

    // 6. Connect Edges
    // Power Line (type: "pwr")
    EdgeData e1Data;
    e1Data.srcNode = pwrId;
    e1Data.dstNode = lanId;
    e1Data.edgeType = "pwr";
    EdgeId e1Id = 0;
    if (liveModel_->addEdge(e1Data, e1Id).ok) {
        auto* e1 = new GraphEdgeItem(liveModel_.get(), e1Id, pwrItem, lanItem);
        liveScene_->addItem(e1);
        e1->refreshLayout();
        liveEdgeIds_.append(e1Id);
    }

    // Network Line (type: "lan")
    EdgeData e2Data;
    e2Data.srcNode = ctlId;
    e2Data.dstNode = lanId;
    e2Data.edgeType = "lan";
    EdgeId e2Id = 0;
    if (liveModel_->addEdge(e2Data, e2Id).ok) {
        auto* e2 = new GraphEdgeItem(liveModel_.get(), e2Id, ctlItem, lanItem);
        liveScene_->addItem(e2);
        e2->refreshLayout();
        liveEdgeIds_.append(e2Id);
    }

    // DB Sync Line (type: "default")
    EdgeData e3Data;
    e3Data.srcNode = lanId;
    e3Data.dstNode = dbId;
    e3Data.edgeType = "default";
    EdgeId e3Id = 0;
    if (liveModel_->addEdge(e3Data, e3Id).ok) {
        auto* e3 = new GraphEdgeItem(liveModel_.get(), e3Id, lanItem, dbItem);
        liveScene_->addItem(e3);
        e3->refreshLayout();
        liveEdgeIds_.append(e3Id);
    }

    // Place extra sample node for any remaining custom node types
    int extraY = 320;
    for (const auto& customType : theme.nodeTypeThemes.keys()) {
        if (customType == "pwr" || customType == "lan" || customType == "db") continue;
        NodeData extraData;
        extraData.name = QString("Sample [%1]").arg(customType).toStdString();
        extraData.type = customType.toStdString();
        NodeId extraId = 0;
        if (liveModel_->addNode(extraData, extraId).ok) {
            auto* extraItem = new GraphNodeItem(liveModel_.get(), extraId);
            liveScene_->addItem(extraItem);
            extraItem->setPos(40, extraY);
            extraItem->refreshGeometry();
            extraY += 70;
        }
    }

    refreshLiveView();
}

void ThemeEditorWindow::refreshLiveView()
{
    if (!liveScene_) return;

    for (QGraphicsItem* item : liveScene_->items()) {
        if (auto* node = dynamic_cast<GraphNodeItem*>(item)) {
            node->onThemeChanged();
        } else if (auto* edge = dynamic_cast<GraphEdgeItem*>(item)) {
            edge->onThemeChanged();
        }
    }

    liveScene_->invalidate(QRectF(), QGraphicsScene::AllLayers);
    if (liveView_) liveView_->viewport()->update();
}

void ThemeEditorWindow::resetLiveViewScene()
{
    populateLiveScene();
    zoomFit();
}

void ThemeEditorWindow::addTestNodeToLiveView()
{
    // Get currently selected type from tree if available
    QString initialType = "default";
    auto items = tree_->selectedItems();
    if (!items.isEmpty()) {
        QString text = items.first()->text(0);
        if (text.contains('#')) {
            initialType = text.section('#', 1);
        }
    }

    bool ok = false;
    QString type = QInputDialog::getText(this, "Add Test Node", "Enter Node Type:", QLineEdit::Normal, initialType, &ok);
    if (!ok || type.trimmed().isEmpty()) return;

    NodeData d;
    d.name = QString("Test [%1]").arg(type.trimmed()).toStdString();
    d.type = type.trimmed().toStdString();
    NodeId nid = 0;
    if (liveModel_->addNode(d, nid).ok) {
        auto* item = new GraphNodeItem(liveModel_.get(), nid);
        liveScene_->addItem(item);
        item->setPos(240, 200);
        item->refreshGeometry();
        refreshLiveView();
        updateStatusDisplay(QString("Added test node '%1'").arg(QString::fromStdString(d.name)));
    }
}

void ThemeEditorWindow::addTestEdgeToLiveView()
{
    // Connect first two nodes found with selected type
    auto nodes = liveScene_->items();
    GraphNodeItem* n1 = nullptr;
    GraphNodeItem* n2 = nullptr;
    for (auto* item : nodes) {
        if (auto* gn = dynamic_cast<GraphNodeItem*>(item)) {
            if (!n1) n1 = gn;
            else if (!n2 && gn != n1) { n2 = gn; break; }
        }
    }

    if (!n1 || !n2) {
        QMessageBox::information(this, "Add Edge", "Need at least two nodes on canvas to connect.");
        return;
    }

    QString edgeType = "pwr";
    bool ok = false;
    edgeType = QInputDialog::getText(this, "Add Test Edge", "Enter Edge Type:", QLineEdit::Normal, edgeType, &ok);
    if (!ok) return;

    EdgeData ed;
    ed.srcNode = n1->nodeId();
    ed.dstNode = n2->nodeId();
    ed.edgeType = edgeType.trimmed().toStdString();
    EdgeId eid = 0;
    if (liveModel_->addEdge(ed, eid).ok) {
        auto* edgeItem = new GraphEdgeItem(liveModel_.get(), eid, n1, n2);
        liveScene_->addItem(edgeItem);
        edgeItem->refreshLayout();
        refreshLiveView();
        updateStatusDisplay(QString("Added test edge of type '%1'").arg(QString::fromStdString(ed.edgeType)));
    }
}


void ThemeEditorWindow::zoomIn()
{
    if (liveView_) liveView_->scale(1.2, 1.2);
}

void ThemeEditorWindow::zoomOut()
{
    if (liveView_) liveView_->scale(1.0 / 1.2, 1.0 / 1.2);
}

void ThemeEditorWindow::zoomFit()
{
    if (liveView_ && liveScene_) {
        liveView_->fitInView(liveScene_->itemsBoundingRect().adjusted(-30, -30, 30, 30), Qt::KeepAspectRatio);
    }
}

/*
 * =========================================================
 * TREE & INSPECTOR PAGES POPULATION
 * =========================================================
 */

ThemeEditorWindow::InspectorPage ThemeEditorWindow::createInspectorPage()
{
    InspectorPage page;
    page.content = new QWidget;
    page.layout = new QVBoxLayout(page.content);
    page.layout->setAlignment(Qt::AlignTop);
    page.layout->setContentsMargins(10, 10, 10, 10);
    page.layout->setSpacing(8);

    auto* scroll = new QScrollArea;
    scroll->setWidget(page.content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    page.container = new QWidget;
    auto* layout = new QVBoxLayout(page.container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    return page;
}

QWidget* ThemeEditorWindow::createSection(const QString& title, QVBoxLayout*& contentLayout)
{
    auto* box = new QGroupBox(title);
    box->setStyleSheet(
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 1px solid #3c3c3c;"
        "    border-radius: 6px;"
        "    margin-top: 10px;"
        "    padding-top: 14px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    subcontrol-position: top left;"
        "    left: 8px;"
        "    padding: 0 4px;"
        "    color: #d0d0d0;"
        "}"
    );
    contentLayout = new QVBoxLayout(box);
    contentLayout->setContentsMargins(8, 10, 8, 8);
    contentLayout->setSpacing(4);
    return box;
}

QPushButton* ThemeEditorWindow::makeColorButton(const QColor& initial, std::function<void(const QColor&)> onChanged)
{
    auto* btn = new QPushButton;
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

QWidget* ThemeEditorWindow::createColorEditor(const QString& title, const QColor& initial, std::function<void(const QColor&)> onChanged)
{
    auto* w = new QWidget;
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->addWidget(new QLabel(title));
    layout->addStretch();
    layout->addWidget(makeColorButton(initial, onChanged));
    return w;
}

QWidget* ThemeEditorWindow::createIntEditor(const QString& title, int value, int min, int max, std::function<void(int)> onChanged)
{
    auto* w = new QWidget;
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 2, 0, 2);

    auto* spin = new QSpinBox;
    spin->setRange(min, max);
    spin->setValue(value);

    connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, onChanged);

    layout->addWidget(new QLabel(title));
    layout->addStretch();
    layout->addWidget(spin);
    return w;
}

QWidget* ThemeEditorWindow::createBoolEditor(const QString& title, bool value, std::function<void(bool)> onChanged)
{
    auto* w = new QWidget;
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 2, 0, 2);

    auto* check = new QCheckBox;
    check->setChecked(value);

    connect(check, &QCheckBox::toggled, this, onChanged);

    layout->addWidget(new QLabel(title));
    layout->addStretch();
    layout->addWidget(check);
    return w;
}

QWidget* ThemeEditorWindow::createPenStyleEditor(const QString& title, Qt::PenStyle value, std::function<void(Qt::PenStyle)> onChanged)
{
    auto* w = new QWidget;
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 2, 0, 2);

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

void ThemeEditorWindow::buildTextStyleSection(const QString& title, GraphTextStyle& style, QVBoxLayout* parentLayout, std::function<void()> onUpdate)
{
    QVBoxLayout* layout = nullptr;
    auto* section = createSection(title, layout);

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

void ThemeEditorWindow::buildArrowStateSection(const QString& title, GraphArrowState& state, QVBoxLayout* parentLayout, std::function<void()> onUpdate)
{
    QVBoxLayout* layout = nullptr;
    auto* section = createSection(title, layout);

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

void ThemeEditorWindow::buildEdgeLabelStyleSection(const QString& title, GraphEdgeLabelState& style, QVBoxLayout* parentLayout, std::function<void()> onUpdate)
{
    QVBoxLayout* layout = nullptr;
    auto* section = createSection(title, layout);

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

void ThemeEditorWindow::buildComponentStateProperties(GraphComponentState& state, QVBoxLayout* layout, bool isContainer, int kindInt, const QString& typeName)
{
    auto kind = static_cast<ComponentStateKind>(kindInt);
    auto* preview = new StandaloneComponentPreview(kind, isContainer, typeName);
    layout->addWidget(new QLabel(isContainer ? "<b>Component Preview:</b>" : "<b>Node Preview:</b>"));
    layout->addWidget(preview);

    auto updatePreview = [preview]() { preview->refresh(); };

    QVBoxLayout* appLayout = nullptr;
    auto* appBox = createSection("Shape & Border", appLayout);

    appLayout->addWidget(createColorEditor("Background", state.background, [&state, updatePreview, this](const QColor& c) {
        state.background = c;
        updatePreview();
        emitThemeChanged();
    }));

    appLayout->addWidget(createColorEditor("Border Color", state.border, [&state, updatePreview, this](const QColor& c) {
        state.border = c;
        updatePreview();
        emitThemeChanged();
    }));

    appLayout->addWidget(createIntEditor("Border Width", state.borderWidth, 0, 20, [&state, updatePreview, this](int v) {
        state.borderWidth = v;
        updatePreview();
        emitThemeChanged();
    }));

    appLayout->addWidget(createPenStyleEditor("Border Style", state.borderStyle, [&state, updatePreview, this](Qt::PenStyle s) {
        state.borderStyle = s;
        if (s != Qt::CustomDashLine) state.dashPattern.clear();
        updatePreview();
        emitThemeChanged();
    }));

    appLayout->addWidget(createIntEditor("Corner Radius", state.radius, 0, 50, [&state, updatePreview, this](int v) {
        state.radius = v;
        updatePreview();
        emitThemeChanged();
    }));

    if (isContainer) {
        appLayout->addWidget(createColorEditor("Header Color", state.headerBackground, [&state, updatePreview, this](const QColor& c) {
            state.headerBackground = c;
            updatePreview();
            emitThemeChanged();
        }));
    }

    layout->addWidget(appBox);

    buildTextStyleSection("Title Text", state.title, layout, updatePreview);
    buildTextStyleSection("Body Text", state.body, layout, updatePreview);
    buildTextStyleSection("Tertiary Text", state.tertiary, layout, updatePreview);
}

void ThemeEditorWindow::buildEdgeStateProperties(GraphEdgeState& state, QVBoxLayout* layout, int kindInt, const QString& typeName)
{
    auto kind = static_cast<ComponentStateKind>(kindInt);
    auto* preview = new StandaloneEdgePreview(kind, typeName);
    layout->addWidget(new QLabel("<b>Edge Preview:</b>"));
    layout->addWidget(preview);

    auto updatePreview = [preview]() { preview->refresh(); };

    QVBoxLayout* lineLayout = nullptr;
    auto* lineBox = createSection("Line Styling", lineLayout);

    lineLayout->addWidget(createColorEditor("Line Color", state.lineColor, [&state, updatePreview, this](const QColor& c) {
        state.lineColor = c;
        updatePreview();
        emitThemeChanged();
    }));

    lineLayout->addWidget(createIntEditor("Line Width", state.lineWidth, 1, 20, [&state, updatePreview, this](int v) {
        state.lineWidth = v;
        updatePreview();
        emitThemeChanged();
    }));

    lineLayout->addWidget(createPenStyleEditor("Line Style", state.lineStyle, [&state, updatePreview, this](Qt::PenStyle s) {
        state.lineStyle = s;
        state.dashed = (s == Qt::DashLine);
        if (s != Qt::CustomDashLine) state.dashPattern.clear();
        updatePreview();
        emitThemeChanged();
    }));

    layout->addWidget(lineBox);

    buildArrowStateSection("Arrow", state.arrow, layout, updatePreview);
    buildEdgeLabelStyleSection("Label", state.label, layout, updatePreview);
}

void ThemeEditorWindow::buildPreviewLineProperties(GraphPreviewLineTheme& preview, QVBoxLayout* layout)
{
    auto updatePreview = [this]() { emitThemeChanged(); };

    QVBoxLayout* lineLayout = nullptr;
    auto* lineBox = createSection("Connecting Line Styling", lineLayout);

    lineLayout->addWidget(createColorEditor("Line Color", preview.color, [&preview, updatePreview, this](const QColor& c) {
        preview.color = c;
        updatePreview();
    }));

    lineLayout->addWidget(createIntEditor("Line Width", preview.width, 1, 10, [&preview, updatePreview, this](int v) {
        preview.width = v;
        updatePreview();
    }));

    lineLayout->addWidget(createPenStyleEditor("Line Style", preview.style, [&preview, updatePreview, this](Qt::PenStyle s) {
        preview.style = s;
        updatePreview();
    }));

    layout->addWidget(lineBox);
}

void ThemeEditorWindow::buildPortStateProperties(GraphPortState& state, QVBoxLayout* layout)
{
    QVBoxLayout* portLayout = nullptr;
    auto* portBox = createSection("Port Colors", portLayout);

    portLayout->addWidget(createColorEditor("Input", state.inputColor, [&state, this](const QColor& c) {
        state.inputColor = c;
        emitThemeChanged();
    }));

    portLayout->addWidget(createColorEditor("Output", state.outputColor, [&state, this](const QColor& c) {
        state.outputColor = c;
        emitThemeChanged();
    }));

    portLayout->addWidget(createColorEditor("Hover", state.hoverColor, [&state, this](const QColor& c) {
        state.hoverColor = c;
        emitThemeChanged();
    }));

    layout->addWidget(portBox);
}

void ThemeEditorWindow::buildViewProperties(QVBoxLayout* layout)
{
    auto& theme = GraphThemeManager::instance()->mutableTheme();

    QVBoxLayout* viewLayout = nullptr;
    auto* viewBox = createSection("Canvas Background", viewLayout);

    viewLayout->addWidget(createColorEditor("Background", theme.view.background, [&theme, this](const QColor& c) {
        theme.view.background = c;
        emitThemeChanged();
    }));

    layout->addWidget(viewBox);
}

void ThemeEditorWindow::buildGridProperties(QVBoxLayout* layout)
{
    auto& grid = GraphThemeManager::instance()->mutableTheme().view.grid;

    QVBoxLayout* gridLayout = nullptr;
    auto* gridBox = createSection("Grid Settings", gridLayout);

    gridLayout->addWidget(createBoolEditor("Enabled", grid.enabled, [&grid, this](bool v) {
        grid.enabled = v;
        emitThemeChanged();
    }));

    gridLayout->addWidget(createColorEditor("Minor Color", grid.minorColor, [&grid, this](const QColor& c) {
        grid.minorColor = c;
        emitThemeChanged();
    }));

    gridLayout->addWidget(createColorEditor("Major Color", grid.majorColor, [&grid, this](const QColor& c) {
        grid.majorColor = c;
        emitThemeChanged();
    }));

    gridLayout->addWidget(createIntEditor("Cell Spacing", grid.spacing, 5, 200, [&grid, this](int v) {
        grid.spacing = v;
        emitThemeChanged();
    }));

    gridLayout->addWidget(createIntEditor("Major Spacing", grid.majorSpacing, 10, 1000, [&grid, this](int v) {
        grid.majorSpacing = v;
        emitThemeChanged();
    }));

    gridLayout->addWidget(createIntEditor("Line Width", grid.lineWidth, 1, 10, [&grid, this](int v) {
        grid.lineWidth = v;
        emitThemeChanged();
    }));

    layout->addWidget(gridBox);
}

void ThemeEditorWindow::buildSelectionProperties(QVBoxLayout* layout)
{
    auto& selection = GraphThemeManager::instance()->mutableTheme().selection;

    QVBoxLayout* selLayout = nullptr;
    auto* selBox = createSection("Selection Box", selLayout);

    selLayout->addWidget(createColorEditor("Outline", selection.outline, [&selection, this](const QColor& c) {
        selection.outline = c;
        emitThemeChanged();
    }));

    selLayout->addWidget(createColorEditor("Fill", selection.fill, [&selection, this](const QColor& c) {
        selection.fill = c;
        emitThemeChanged();
    }));

    layout->addWidget(selBox);
}

void ThemeEditorWindow::buildInteractionProperties(QVBoxLayout* layout)
{
    auto& interaction = GraphThemeManager::instance()->mutableTheme().interaction;

    QVBoxLayout* intLayout = nullptr;
    auto* intBox = createSection("Interaction Cues", intLayout);

    intLayout->addWidget(createColorEditor("Hover Outline", interaction.hoverOutline, [&interaction, this](const QColor& c) {
        interaction.hoverOutline = c;
        emitThemeChanged();
    }));

    intLayout->addWidget(createColorEditor("Invalid Connection", interaction.invalidConnection, [&interaction, this](const QColor& c) {
        interaction.invalidConnection = c;
        emitThemeChanged();
    }));

    intLayout->addWidget(createColorEditor("Drop Target", interaction.dropTarget, [&interaction, this](const QColor& c) {
        interaction.dropTarget = c;
        emitThemeChanged();
    }));

    layout->addWidget(intBox);
}

void ThemeEditorWindow::buildGeneralProperties(QVBoxLayout* layout)
{
    QVBoxLayout* secLayout = nullptr;
    auto* sec = createSection("Theme Details", secLayout);

    auto* nameRow = new QWidget;
    auto* nameLayout = new QHBoxLayout(nameRow);
    nameLayout->setContentsMargins(0, 0, 0, 0);
    nameLayout->addWidget(new QLabel("Theme Name:"));
    auto* nameEdit = new QLineEdit;
    nameEdit->setText(GraphThemeManager::instance()->theme().name);
    connect(nameEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        GraphThemeManager::instance()->mutableTheme().name = text;
        updateStatusDisplay("Theme name modified");
    });
    nameLayout->addWidget(nameEdit, 1);
    secLayout->addWidget(nameRow);

    auto* fileRow = new QWidget;
    auto* fileLayout = new QHBoxLayout(fileRow);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    fileLayout->addWidget(new QLabel("Current File:"));
    auto* fileEdit = new QLineEdit;
    QString curPath = GraphThemeManager::instance()->currentThemePath();
    fileEdit->setText(curPath.isEmpty() ? "(Unsaved / Custom Theme)" : curPath);
    fileEdit->setReadOnly(true);
    fileLayout->addWidget(fileEdit, 1);
    secLayout->addWidget(fileRow);

    layout->addWidget(sec);

    QVBoxLayout* actLayout = nullptr;
    auto* actSec = createSection("Quick Actions", actLayout);

    auto* row1 = new QHBoxLayout;
    auto* btnSave = new QPushButton("Save Theme");
    auto* btnSaveAs = new QPushButton("Save Theme As...");
    auto* btnReset = new QPushButton("Reset to Defaults");

    connect(btnSave, &QPushButton::clicked, this, &ThemeEditorWindow::saveTheme);
    connect(btnSaveAs, &QPushButton::clicked, this, &ThemeEditorWindow::saveThemeAs);
    connect(btnReset, &QPushButton::clicked, this, &ThemeEditorWindow::resetToDefaultTheme);

    row1->addWidget(btnSave);
    row1->addWidget(btnSaveAs);
    row1->addWidget(btnReset);
    actLayout->addLayout(row1);

    layout->addWidget(actSec);
}

void ThemeEditorWindow::buildTypeOverviewProperties(QVBoxLayout* layout, bool isEdge, bool isContainer, const QString& typeName)
{
    QString labelStr = isEdge ? QString("Edge Style: edge#%1").arg(typeName)
                              : (isContainer ? QString("Container Style: container#%1").arg(typeName)
                                             : QString("Node Style: node#%1").arg(typeName));

    QVBoxLayout* infoLayout = nullptr;
    auto* infoBox = createSection("Style Information", infoLayout);

    infoLayout->addWidget(new QLabel(QString("<b>%1</b>").arg(labelStr)));
    infoLayout->addWidget(new QLabel(QString("Target: <i>%1</i>").arg(isEdge ? "Edge" : (isContainer ? "Container" : "Node"))));
    infoLayout->addWidget(new QLabel(QString("Type Identifier: <b>%1</b>").arg(typeName)));

    auto* btnRow = new QHBoxLayout;
    auto* btnDup = new QPushButton("Duplicate This Style");
    auto* btnDel = new QPushButton("Delete This Style");
    btnRow->addWidget(btnDup);
    btnRow->addWidget(btnDel);
    infoLayout->addLayout(btnRow);

    connect(btnDup, &QPushButton::clicked, this, &ThemeEditorWindow::duplicateSelectedStyle);
    connect(btnDel, &QPushButton::clicked, this, &ThemeEditorWindow::removeSelectedStyle);

    layout->addWidget(infoBox);

    // Previews for Normal, Hover, Selected
    QVBoxLayout* prevLayout = nullptr;
    auto* prevBox = createSection("State Previews", prevLayout);

    if (isEdge) {
        prevLayout->addWidget(new QLabel("Normal State:"));
        prevLayout->addWidget(new StandaloneEdgePreview(ComponentStateKind::Normal, typeName));
        prevLayout->addWidget(new QLabel("Hover State:"));
        prevLayout->addWidget(new StandaloneEdgePreview(ComponentStateKind::Hover, typeName));
        prevLayout->addWidget(new QLabel("Selected State:"));
        prevLayout->addWidget(new StandaloneEdgePreview(ComponentStateKind::Selected, typeName));
    } else {
        prevLayout->addWidget(new QLabel("Normal State:"));
        prevLayout->addWidget(new StandaloneComponentPreview(ComponentStateKind::Normal, isContainer, typeName));
        prevLayout->addWidget(new QLabel("Hover State:"));
        prevLayout->addWidget(new StandaloneComponentPreview(ComponentStateKind::Hover, isContainer, typeName));
        prevLayout->addWidget(new QLabel("Selected State:"));
        prevLayout->addWidget(new StandaloneComponentPreview(ComponentStateKind::Selected, isContainer, typeName));
    }

    layout->addWidget(prevBox);
}

void ThemeEditorWindow::refreshTree()
{
    isInternalUpdate_ = true;

    // Clear old inspector stack pages
    while (stack_->count() > 0) {
        auto* w = stack_->widget(0);
        stack_->removeWidget(w);
        delete w;
    }

    tree_->clear();

    auto& theme = GraphThemeManager::instance()->mutableTheme();

    auto addPage = [&](QTreeWidgetItem* item, auto builder)
    {
        auto page = createInspectorPage();
        builder(page.layout);
        int index = stack_->addWidget(page.container);
        item->setData(0, Qt::UserRole, index);
    };

    // 0. General
    auto* genItem = new QTreeWidgetItem(QStringList() << "General");
    tree_->addTopLevelItem(genItem);
    addPage(genItem, [this](QVBoxLayout* l) { buildGeneralProperties(l); });

    // 1. View
    auto* viewItem = new QTreeWidgetItem(QStringList() << "View & Canvas");
    tree_->addTopLevelItem(viewItem);
    addPage(viewItem, [this](QVBoxLayout* l) { buildViewProperties(l); });

    // 2. Grid
    auto* gridItem = new QTreeWidgetItem(QStringList() << "Grid");
    tree_->addTopLevelItem(gridItem);
    addPage(gridItem, [this](QVBoxLayout* l) { buildGridProperties(l); });

    // 3. Nodes
    auto* nodeRoot = new QTreeWidgetItem(QStringList() << "Nodes");
    tree_->addTopLevelItem(nodeRoot);

    // 3a. Node Base
    auto* nodeBase = new QTreeWidgetItem(QStringList() << "Base Style (Default)");
    nodeRoot->addChild(nodeBase);
    addPage(nodeBase, [this, &theme](QVBoxLayout* l) {
        buildTypeOverviewProperties(l, false, false, QString());
    });

    auto* nNorm = new QTreeWidgetItem(QStringList() << "Normal");
    nodeBase->addChild(nNorm);
    addPage(nNorm, [this, &theme](QVBoxLayout* l) {
        buildComponentStateProperties(theme.node.normal, l, false, 0);
    });

    auto* nHov = new QTreeWidgetItem(QStringList() << "Hover");
    nodeBase->addChild(nHov);
    addPage(nHov, [this, &theme](QVBoxLayout* l) {
        buildComponentStateProperties(theme.node.hover, l, false, 1);
    });

    auto* nSel = new QTreeWidgetItem(QStringList() << "Selected");
    nodeBase->addChild(nSel);
    addPage(nSel, [this, &theme](QVBoxLayout* l) {
        buildComponentStateProperties(theme.node.selected, l, false, 2);
    });

    // 3b. Custom Node Types
    for (const auto& typeKey : theme.nodeTypeThemes.keys()) {
        auto* typeItem = new QTreeWidgetItem(QStringList() << QString("node#%1").arg(typeKey));
        nodeRoot->addChild(typeItem);
        addPage(typeItem, [this, typeKey](QVBoxLayout* l) {
            buildTypeOverviewProperties(l, false, false, typeKey);
        });

        auto& compTheme = theme.nodeTypeThemes[typeKey];
        auto* stNorm = new QTreeWidgetItem(QStringList() << "Normal");
        typeItem->addChild(stNorm);
        addPage(stNorm, [this, &compTheme, typeKey](QVBoxLayout* l) {
            buildComponentStateProperties(compTheme.normal, l, false, 0, typeKey);
        });

        auto* stHov = new QTreeWidgetItem(QStringList() << "Hover");
        typeItem->addChild(stHov);
        addPage(stHov, [this, &compTheme, typeKey](QVBoxLayout* l) {
            buildComponentStateProperties(compTheme.hover, l, false, 1, typeKey);
        });

        auto* stSel = new QTreeWidgetItem(QStringList() << "Selected");
        typeItem->addChild(stSel);
        addPage(stSel, [this, &compTheme, typeKey](QVBoxLayout* l) {
            buildComponentStateProperties(compTheme.selected, l, false, 2, typeKey);
        });
    }

    // 4. Containers
    auto* contRoot = new QTreeWidgetItem(QStringList() << "Containers");
    tree_->addTopLevelItem(contRoot);

    auto* contBase = new QTreeWidgetItem(QStringList() << "Base Style (Default)");
    contRoot->addChild(contBase);
    addPage(contBase, [this](QVBoxLayout* l) {
        buildTypeOverviewProperties(l, false, true, QString());
    });

    auto* cNorm = new QTreeWidgetItem(QStringList() << "Normal");
    contBase->addChild(cNorm);
    addPage(cNorm, [this, &theme](QVBoxLayout* l) {
        buildComponentStateProperties(theme.container.normal, l, true, 0);
    });

    auto* cHov = new QTreeWidgetItem(QStringList() << "Hover");
    contBase->addChild(cHov);
    addPage(cHov, [this, &theme](QVBoxLayout* l) {
        buildComponentStateProperties(theme.container.hover, l, true, 1);
    });

    auto* cSel = new QTreeWidgetItem(QStringList() << "Selected");
    contBase->addChild(cSel);
    addPage(cSel, [this, &theme](QVBoxLayout* l) {
        buildComponentStateProperties(theme.container.selected, l, true, 2);
    });

    // 4b. Custom Container Types
    for (const auto& typeKey : theme.containerTypeThemes.keys()) {
        auto* typeItem = new QTreeWidgetItem(QStringList() << QString("container#%1").arg(typeKey));
        contRoot->addChild(typeItem);
        addPage(typeItem, [this, typeKey](QVBoxLayout* l) {
            buildTypeOverviewProperties(l, false, true, typeKey);
        });

        auto& compTheme = theme.containerTypeThemes[typeKey];
        auto* stNorm = new QTreeWidgetItem(QStringList() << "Normal");
        typeItem->addChild(stNorm);
        addPage(stNorm, [this, &compTheme, typeKey](QVBoxLayout* l) {
            buildComponentStateProperties(compTheme.normal, l, true, 0, typeKey);
        });

        auto* stHov = new QTreeWidgetItem(QStringList() << "Hover");
        typeItem->addChild(stHov);
        addPage(stHov, [this, &compTheme, typeKey](QVBoxLayout* l) {
            buildComponentStateProperties(compTheme.hover, l, true, 1, typeKey);
        });

        auto* stSel = new QTreeWidgetItem(QStringList() << "Selected");
        typeItem->addChild(stSel);
        addPage(stSel, [this, &compTheme, typeKey](QVBoxLayout* l) {
            buildComponentStateProperties(compTheme.selected, l, true, 2, typeKey);
        });
    }

    // 5. Edges
    auto* edgeRoot = new QTreeWidgetItem(QStringList() << "Edges");
    tree_->addTopLevelItem(edgeRoot);

    auto* edgeBase = new QTreeWidgetItem(QStringList() << "Base Style (Default)");
    edgeRoot->addChild(edgeBase);
    addPage(edgeBase, [this](QVBoxLayout* l) {
        buildTypeOverviewProperties(l, true, false, QString());
    });

    auto* eNorm = new QTreeWidgetItem(QStringList() << "Normal");
    edgeBase->addChild(eNorm);
    addPage(eNorm, [this, &theme](QVBoxLayout* l) {
        buildEdgeStateProperties(theme.edge.normal, l, 0);
    });

    auto* eHov = new QTreeWidgetItem(QStringList() << "Hover");
    edgeBase->addChild(eHov);
    addPage(eHov, [this, &theme](QVBoxLayout* l) {
        buildEdgeStateProperties(theme.edge.hover, l, 1);
    });

    auto* eSel = new QTreeWidgetItem(QStringList() << "Selected");
    edgeBase->addChild(eSel);
    addPage(eSel, [this, &theme](QVBoxLayout* l) {
        buildEdgeStateProperties(theme.edge.selected, l, 2);
    });

    auto* prevItem = new QTreeWidgetItem(QStringList() << "Connecting Line (Preview)");
    edgeBase->addChild(prevItem);
    addPage(prevItem, [this, &theme](QVBoxLayout* l) {
        buildPreviewLineProperties(theme.edge.preview, l);
    });

    // 5b. Custom Edge Types
    for (const auto& typeKey : theme.edgeTypeThemes.keys()) {
        auto* typeItem = new QTreeWidgetItem(QStringList() << QString("edge#%1").arg(typeKey));
        edgeRoot->addChild(typeItem);
        addPage(typeItem, [this, typeKey](QVBoxLayout* l) {
            buildTypeOverviewProperties(l, true, false, typeKey);
        });

        auto& edgeTheme = theme.edgeTypeThemes[typeKey];
        auto* stNorm = new QTreeWidgetItem(QStringList() << "Normal");
        typeItem->addChild(stNorm);
        addPage(stNorm, [this, &edgeTheme, typeKey](QVBoxLayout* l) {
            buildEdgeStateProperties(edgeTheme.normal, l, 0, typeKey);
        });

        auto* stHov = new QTreeWidgetItem(QStringList() << "Hover");
        typeItem->addChild(stHov);
        addPage(stHov, [this, &edgeTheme, typeKey](QVBoxLayout* l) {
            buildEdgeStateProperties(edgeTheme.hover, l, 1, typeKey);
        });

        auto* stSel = new QTreeWidgetItem(QStringList() << "Selected");
        typeItem->addChild(stSel);
        addPage(stSel, [this, &edgeTheme, typeKey](QVBoxLayout* l) {
            buildEdgeStateProperties(edgeTheme.selected, l, 2, typeKey);
        });
    }

    // 6. Ports
    auto* portRoot = new QTreeWidgetItem(QStringList() << "Ports");
    tree_->addTopLevelItem(portRoot);
    auto* pNorm = new QTreeWidgetItem(QStringList() << "Normal & Hover Colors");
    portRoot->addChild(pNorm);
    addPage(pNorm, [this, &theme](QVBoxLayout* l) {
        buildPortStateProperties(theme.port.normal, l);
    });

    // 7. Selection
    auto* selItem = new QTreeWidgetItem(QStringList() << "Selection Box");
    tree_->addTopLevelItem(selItem);
    addPage(selItem, [this](QVBoxLayout* l) { buildSelectionProperties(l); });

    // 8. Interaction
    auto* intItem = new QTreeWidgetItem(QStringList() << "Interaction Cues");
    tree_->addTopLevelItem(intItem);
    addPage(intItem, [this](QVBoxLayout* l) { buildInteractionProperties(l); });

    tree_->expandAll();

    if (tree_->topLevelItemCount() > 0) {
        tree_->setCurrentItem(tree_->topLevelItem(0));
    }

    isInternalUpdate_ = false;
    updateActionStates();
}

void ThemeEditorWindow::updateActionStates()
{
    auto items = tree_->selectedItems();
    if (items.isEmpty()) {
        btnRemoveStyle_->setEnabled(false);
        actRemoveStyle_->setEnabled(false);
        btnDuplicateStyle_->setEnabled(false);
        actDuplicateStyle_->setEnabled(false);
        btnCopyStyle_->setEnabled(false);
        actCopyStyle_->setEnabled(false);
        return;
    }

    QString text = items.first()->text(0);
    bool isCustomType = text.contains('#');

    btnRemoveStyle_->setEnabled(isCustomType);
    actRemoveStyle_->setEnabled(isCustomType);

    btnDuplicateStyle_->setEnabled(true);
    actDuplicateStyle_->setEnabled(true);

    btnCopyStyle_->setEnabled(true);
    actCopyStyle_->setEnabled(true);

    btnPasteStyle_->setEnabled(clipboard_.kind != StyleClipboard::Kind::None);
    actPasteStyle_->setEnabled(clipboard_.kind != StyleClipboard::Kind::None);
}

void ThemeEditorWindow::emitThemeChanged()
{
    GraphThemeManager::instance()->themeChanged();
    refreshLiveView();
}

void ThemeEditorWindow::updateStatusDisplay(const QString& message)
{
    if (!statusLabel_) return;
    if (!message.isEmpty()) {
        statusLabel_->setText(message);
        return;
    }

    const auto& theme = GraphThemeManager::instance()->theme();
    int customCount = theme.nodeTypeThemes.size() + theme.containerTypeThemes.size() + theme.edgeTypeThemes.size();
    statusLabel_->setText(QString("Theme: %1 | Custom Type Styles: %2")
        .arg(theme.name.isEmpty() ? "Unnamed Theme" : theme.name)
        .arg(customCount));
}

/*
 * =========================================================
 * ADD, DUPLICATE, REMOVE, COPY, PASTE IMPLEMENTATIONS
 * =========================================================
 */

void ThemeEditorWindow::addNewStyle()
{
    AddStyleDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) return;

    int target = dialog.targetType();
    QString name = dialog.styleName();
    QString templateChoice = dialog.templateChoice();

    auto& theme = GraphThemeManager::instance()->mutableTheme();

    if (target == 0) { // Node
        if (theme.nodeTypeThemes.contains(name)) {
            QMessageBox::warning(this, "Style Exists", QString("Node style 'node#%1' already exists.").arg(name));
            return;
        }

        GraphComponentTheme newTheme = theme.node;
        if (templateChoice.startsWith("node#")) {
            QString srcKey = templateChoice.section('#', 1);
            if (theme.nodeTypeThemes.contains(srcKey))
                newTheme = theme.nodeTypeThemes[srcKey];
        }
        theme.nodeTypeThemes[name] = newTheme;
    } else if (target == 1) { // Container
        if (theme.containerTypeThemes.contains(name)) {
            QMessageBox::warning(this, "Style Exists", QString("Container style 'container#%1' already exists.").arg(name));
            return;
        }

        GraphComponentTheme newTheme = theme.container;
        if (templateChoice.startsWith("container#")) {
            QString srcKey = templateChoice.section('#', 1);
            if (theme.containerTypeThemes.contains(srcKey))
                newTheme = theme.containerTypeThemes[srcKey];
        }
        theme.containerTypeThemes[name] = newTheme;
    } else { // Edge
        if (theme.edgeTypeThemes.contains(name)) {
            QMessageBox::warning(this, "Style Exists", QString("Edge style 'edge#%1' already exists.").arg(name));
            return;
        }

        GraphEdgeTheme newTheme = theme.edge;
        if (templateChoice.startsWith("edge#")) {
            QString srcKey = templateChoice.section('#', 1);
            if (theme.edgeTypeThemes.contains(srcKey))
                newTheme = theme.edgeTypeThemes[srcKey];
        }
        theme.edgeTypeThemes[name] = newTheme;
    }

    emitThemeChanged();
    refreshTree();
    populateLiveScene();

    updateStatusDisplay(QString("Added style for type '%1'").arg(name));
}

void ThemeEditorWindow::duplicateSelectedStyle()
{
    auto items = tree_->selectedItems();
    if (items.isEmpty()) return;

    QString text = items.first()->text(0);
    auto* parent = items.first()->parent();
    QString parentText = parent ? parent->text(0) : "";

    auto& theme = GraphThemeManager::instance()->mutableTheme();

    bool isEdge = text.startsWith("edge#") || parentText.startsWith("edge#") || text.contains("Edge");
    bool isContainer = text.startsWith("container#") || parentText.startsWith("container#") || text.contains("Container");

    QString currentType = text.contains('#') ? text.section('#', 1) : (parentText.contains('#') ? parentText.section('#', 1) : "custom");
    QString defaultNewName = QString("%1_copy").arg(currentType);

    bool ok = false;
    QString newName = QInputDialog::getText(this, "Duplicate Style", "New Style Type Name:", QLineEdit::Normal, defaultNewName, &ok);
    if (!ok || newName.trimmed().isEmpty()) return;

    newName = newName.trimmed().toLower();

    if (isEdge) {
        if (theme.edgeTypeThemes.contains(newName)) {
            QMessageBox::warning(this, "Style Exists", QString("Edge style 'edge#%1' already exists.").arg(newName));
            return;
        }
        GraphEdgeTheme cloned = theme.edgeThemeForType(currentType);
        theme.edgeTypeThemes[newName] = cloned;
    } else if (isContainer) {
        if (theme.containerTypeThemes.contains(newName)) {
            QMessageBox::warning(this, "Style Exists", QString("Container style 'container#%1' already exists.").arg(newName));
            return;
        }
        GraphComponentTheme cloned = theme.containerThemeForType(currentType);
        theme.containerTypeThemes[newName] = cloned;
    } else {
        if (theme.nodeTypeThemes.contains(newName)) {
            QMessageBox::warning(this, "Style Exists", QString("Node style 'node#%1' already exists.").arg(newName));
            return;
        }
        GraphComponentTheme cloned = theme.nodeThemeForType(currentType);
        theme.nodeTypeThemes[newName] = cloned;
    }

    emitThemeChanged();
    refreshTree();
    populateLiveScene();

    updateStatusDisplay(QString("Duplicated style to '%1'").arg(newName));
}

void ThemeEditorWindow::removeSelectedStyle()
{
    auto items = tree_->selectedItems();
    if (items.isEmpty()) return;

    QString text = items.first()->text(0);
    if (!text.contains('#')) {
        QMessageBox::information(this, "Remove Style", "Base styles cannot be removed. You can reset them to default instead.");
        return;
    }

    auto reply = QMessageBox::question(this, "Confirm Removal",
        QString("Are you sure you want to remove style '%1'?").arg(text),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QString prefix = text.section('#', 0, 0);
    QString typeName = text.section('#', 1);

    auto& theme = GraphThemeManager::instance()->mutableTheme();

    if (prefix == "node") {
        theme.nodeTypeThemes.remove(typeName);
    } else if (prefix == "container") {
        theme.containerTypeThemes.remove(typeName);
    } else if (prefix == "edge") {
        theme.edgeTypeThemes.remove(typeName);
    }

    emitThemeChanged();
    refreshTree();
    populateLiveScene();

    updateStatusDisplay(QString("Removed style '%1'").arg(text));
}

void ThemeEditorWindow::copySelectedStyle()
{
    auto items = tree_->selectedItems();
    if (items.isEmpty()) return;

    auto* item = items.first();
    QString text = item->text(0);
    auto* parent = item->parent();
    QString parentText = parent ? parent->text(0) : "";

    const auto& theme = GraphThemeManager::instance()->theme();

    if (text.startsWith("node#")) {
        QString type = text.section('#', 1);
        clipboard_.kind = StyleClipboard::Kind::ComponentTheme;
        clipboard_.sourceName = text;
        clipboard_.componentTheme = theme.nodeThemeForType(type);
    } else if (text.startsWith("container#")) {
        QString type = text.section('#', 1);
        clipboard_.kind = StyleClipboard::Kind::ComponentTheme;
        clipboard_.sourceName = text;
        clipboard_.componentTheme = theme.containerThemeForType(type);
    } else if (text.startsWith("edge#")) {
        QString type = text.section('#', 1);
        clipboard_.kind = StyleClipboard::Kind::EdgeTheme;
        clipboard_.sourceName = text;
        clipboard_.edgeTheme = theme.edgeThemeForType(type);
    } else if (parentText.contains("Node") || (parent && parent->parent() && parent->parent()->text(0) == "Nodes")) {
        QString type = parentText.contains('#') ? parentText.section('#', 1) : "";
        clipboard_.kind = StyleClipboard::Kind::ComponentState;
        clipboard_.sourceName = QString("%1 -> %2").arg(parentText.isEmpty() ? "Node" : parentText, text);
        clipboard_.componentState = theme.nodeStateFor(type, text == "Selected", text == "Hover");
    } else if (parentText.contains("Container") || (parent && parent->parent() && parent->parent()->text(0) == "Containers")) {
        QString type = parentText.contains('#') ? parentText.section('#', 1) : "";
        clipboard_.kind = StyleClipboard::Kind::ComponentState;
        clipboard_.sourceName = QString("%1 -> %2").arg(parentText.isEmpty() ? "Container" : parentText, text);
        clipboard_.componentState = theme.containerStateFor(type, text == "Selected", text == "Hover");
    } else if (parentText.contains("Edge") || (parent && parent->parent() && parent->parent()->text(0) == "Edges")) {
        QString type = parentText.contains('#') ? parentText.section('#', 1) : "";
        clipboard_.kind = StyleClipboard::Kind::EdgeState;
        clipboard_.sourceName = QString("%1 -> %2").arg(parentText.isEmpty() ? "Edge" : parentText, text);
        clipboard_.edgeState = theme.edgeStateFor(type, text == "Selected", text == "Hover");
    }

    updateActionStates();
    updateStatusDisplay(QString("Copied style: %1").arg(clipboard_.sourceName));
}

void ThemeEditorWindow::pasteSelectedStyle()
{
    if (clipboard_.kind == StyleClipboard::Kind::None) return;

    auto items = tree_->selectedItems();
    if (items.isEmpty()) return;

    auto* item = items.first();
    QString text = item->text(0);
    auto* parent = item->parent();
    QString parentText = parent ? parent->text(0) : "";

    auto& theme = GraphThemeManager::instance()->mutableTheme();

    if (clipboard_.kind == StyleClipboard::Kind::ComponentTheme) {
        if (text.startsWith("node#")) {
            theme.nodeTypeThemes[text.section('#', 1)] = clipboard_.componentTheme;
        } else if (text.startsWith("container#")) {
            theme.containerTypeThemes[text.section('#', 1)] = clipboard_.componentTheme;
        } else if (text == "Base Style (Default)" && parent && parent->text(0) == "Nodes") {
            theme.node = clipboard_.componentTheme;
        } else if (text == "Base Style (Default)" && parent && parent->text(0) == "Containers") {
            theme.container = clipboard_.componentTheme;
        } else {
            QMessageBox::warning(this, "Paste Style", "Incompatible destination. Copy a component theme to paste onto a node or container.");
            return;
        }
    } else if (clipboard_.kind == StyleClipboard::Kind::EdgeTheme) {
        if (text.startsWith("edge#")) {
            theme.edgeTypeThemes[text.section('#', 1)] = clipboard_.edgeTheme;
        } else if (text == "Base Style (Default)" && parent && parent->text(0) == "Edges") {
            theme.edge = clipboard_.edgeTheme;
        } else {
            QMessageBox::warning(this, "Paste Style", "Incompatible destination. Copy an edge theme to paste onto an edge.");
            return;
        }
    } else if (clipboard_.kind == StyleClipboard::Kind::ComponentState) {
        auto applyToState = [&](GraphComponentState& target) {
            target = clipboard_.componentState;
        };

        if (text == "Normal") {
            if (parentText.startsWith("node#")) applyToState(theme.nodeTypeThemes[parentText.section('#', 1)].normal);
            else if (parentText.startsWith("container#")) applyToState(theme.containerTypeThemes[parentText.section('#', 1)].normal);
            else if (parent && parent->parent() && parent->parent()->text(0) == "Nodes") applyToState(theme.node.normal);
            else if (parent && parent->parent() && parent->parent()->text(0) == "Containers") applyToState(theme.container.normal);
        } else if (text == "Hover") {
            if (parentText.startsWith("node#")) applyToState(theme.nodeTypeThemes[parentText.section('#', 1)].hover);
            else if (parentText.startsWith("container#")) applyToState(theme.containerTypeThemes[parentText.section('#', 1)].hover);
            else if (parent && parent->parent() && parent->parent()->text(0) == "Nodes") applyToState(theme.node.hover);
            else if (parent && parent->parent() && parent->parent()->text(0) == "Containers") applyToState(theme.container.hover);
        } else if (text == "Selected") {
            if (parentText.startsWith("node#")) applyToState(theme.nodeTypeThemes[parentText.section('#', 1)].selected);
            else if (parentText.startsWith("container#")) applyToState(theme.containerTypeThemes[parentText.section('#', 1)].selected);
            else if (parent && parent->parent() && parent->parent()->text(0) == "Nodes") applyToState(theme.node.selected);
            else if (parent && parent->parent() && parent->parent()->text(0) == "Containers") applyToState(theme.container.selected);
        }
    } else if (clipboard_.kind == StyleClipboard::Kind::EdgeState) {
        auto applyToEdgeState = [&](GraphEdgeState& target) {
            target = clipboard_.edgeState;
        };

        if (text == "Normal") {
            if (parentText.startsWith("edge#")) applyToEdgeState(theme.edgeTypeThemes[parentText.section('#', 1)].normal);
            else if (parent && parent->parent() && parent->parent()->text(0) == "Edges") applyToEdgeState(theme.edge.normal);
        } else if (text == "Hover") {
            if (parentText.startsWith("edge#")) applyToEdgeState(theme.edgeTypeThemes[parentText.section('#', 1)].hover);
            else if (parent && parent->parent() && parent->parent()->text(0) == "Edges") applyToEdgeState(theme.edge.hover);
        } else if (text == "Selected") {
            if (parentText.startsWith("edge#")) applyToEdgeState(theme.edgeTypeThemes[parentText.section('#', 1)].selected);
            else if (parent && parent->parent() && parent->parent()->text(0) == "Edges") applyToEdgeState(theme.edge.selected);
        }
    }

    emitThemeChanged();
    refreshTree();
    updateStatusDisplay(QString("Pasted style from %1 to %2").arg(clipboard_.sourceName, text));
}

/*
 * =========================================================
 * FILE OPERATIONS
 * =========================================================
 */

bool ThemeEditorWindow::loadTheme(const QString& filePath)
{
    if (GraphThemeManager::instance()->load(filePath)) {
        refreshTree();
        populateLiveScene();
        updateStatusDisplay(QString("Loaded %1").arg(QFileInfo(filePath).fileName()));
        return true;
    }
    QMessageBox::warning(this, "Load Theme Failed", QString("Could not load theme from %1").arg(filePath));
    return false;
}

bool ThemeEditorWindow::saveTheme()
{
    if (GraphThemeManager::instance()->save()) {
        updateStatusDisplay("Theme saved successfully.");
        return true;
    }
    return saveThemeAs();
}

bool ThemeEditorWindow::saveThemeAs()
{
    QString path = QFileDialog::getSaveFileName(this, "Save Theme As", "src/gui/theme/themes", "Theme Files (*.json)");
    if (path.isEmpty()) return false;

    if (!path.endsWith(".json", Qt::CaseInsensitive))
        path += ".json";

    if (GraphThemeManager::instance()->save(path)) {
        updateStatusDisplay(QString("Saved theme as %1").arg(QFileInfo(path).fileName()));
        return true;
    }
    QMessageBox::warning(this, "Save Failed", "Could not save theme file.");
    return false;
}

void ThemeEditorWindow::resetToDefaultTheme()
{
    auto reply = QMessageBox::question(this, "Reset Theme",
        "Reset all theme settings and custom styles to default?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        GraphThemeManager::instance()->resetDefaults();
        refreshTree();
        populateLiveScene();
        updateStatusDisplay("Reset theme to defaults.");
    }
}
