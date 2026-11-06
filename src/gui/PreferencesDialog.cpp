#include "PreferencesDialog.h"
#include "config/ApplicationConfig.h"
#include "theme/ApplicationThemeManager.h"
#include "ShortcutConfigDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QFontDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDialogButtonBox>
#include <QFrame>
#include <QTabWidget>
#include <QFileInfo>

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Preferences");
    setModal(true);
    setMinimumWidth(560);
    resize(580, 560);

    setupUi();
    loadCurrentValues();
}

void PreferencesDialog::setupUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(14, 14, 14, 14);
    outerLayout->setSpacing(12);

    tabWidget_ = new QTabWidget(this);

    appearanceTab_ = createAppearanceTab();
    generalTab_ = createGeneralTab();
    shortcutsTab_ = createShortcutsTab();

    tabWidget_->addTab(appearanceTab_, "Appearance");
    tabWidget_->addTab(generalTab_, "General");
    tabWidget_->addTab(shortcutsTab_, "Shortcuts");

    outerLayout->addWidget(tabWidget_);

    // Dialog Action Buttons
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
        this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &PreferencesDialog::onApply);

    outerLayout->addWidget(buttonBox);

    // Event Connections
    connect(themeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PreferencesDialog::onThemeSelectionChanged);
    connect(loadCustomThemeBtn_, &QPushButton::clicked, this, &PreferencesDialog::onLoadCustomThemeClicked);
    connect(openCustomFolderBtn_, &QPushButton::clicked, this, &PreferencesDialog::onOpenCustomFolderClicked);

    connect(fontComboBox_, &QFontComboBox::currentFontChanged, this, &PreferencesDialog::onFontSelectionChanged);
    connect(fontSizeSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged), this, &PreferencesDialog::onFontSelectionChanged);
    connect(chooseFontBtn_, &QPushButton::clicked, this, &PreferencesDialog::onChooseFontClicked);
    connect(resetFontBtn_, &QPushButton::clicked, this, &PreferencesDialog::onResetFontClicked);

    connect(autoSaveCheck_, &QCheckBox::toggled, autoSaveDelayCombo_, &QComboBox::setEnabled);
    connect(openShortcutsBtn_, &QPushButton::clicked, this, &PreferencesDialog::onConfigureShortcutsClicked);
}

QWidget* PreferencesDialog::createAppearanceTab()
{
    auto* tab = new QWidget(this);
    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(10, 12, 10, 10);
    tabLayout->setSpacing(12);

    // 1. Application UI Theme Group
    auto* themeGroup = new QGroupBox("Application UI Theme", tab);
    auto* themeLayout = new QVBoxLayout(themeGroup);
    themeLayout->setSpacing(10);

    auto* themeForm = new QFormLayout();
    themeForm->setSpacing(8);

    themeCombo_ = new QComboBox(themeGroup);
    themeForm->addRow("Active Theme:", themeCombo_);
    themeLayout->addLayout(themeForm);

    themeColorSwatches_ = new QWidget(themeGroup);
    auto* swatchesLayout = new QHBoxLayout(themeColorSwatches_);
    swatchesLayout->setContentsMargins(0, 0, 0, 0);
    swatchesLayout->setSpacing(6);
    themeLayout->addWidget(themeColorSwatches_);

    themeInfoLabel_ = new QLabel(themeGroup);
    themeInfoLabel_->setStyleSheet("color: gray;");
    themeLayout->addWidget(themeInfoLabel_);

    auto* themeBtnsLayout = new QHBoxLayout();
    loadCustomThemeBtn_ = new QPushButton("Load Custom Theme File...", themeGroup);
    openCustomFolderBtn_ = new QPushButton("Open Custom Themes Folder...", themeGroup);
    themeBtnsLayout->addWidget(loadCustomThemeBtn_);
    themeBtnsLayout->addWidget(openCustomFolderBtn_);
    themeBtnsLayout->addStretch();
    themeLayout->addLayout(themeBtnsLayout);

    tabLayout->addWidget(themeGroup);

    // 2. Application Font & Typography Group
    auto* fontGroup = new QGroupBox("Application Font & Typography", tab);
    auto* fontLayout = new QVBoxLayout(fontGroup);
    fontLayout->setSpacing(10);

    auto* fontForm = new QFormLayout();
    fontForm->setSpacing(8);

    fontComboBox_ = new QFontComboBox(fontGroup);
    fontForm->addRow("Font Family:", fontComboBox_);

    auto* sizeLayout = new QHBoxLayout();
    fontSizeSpinBox_ = new QSpinBox(fontGroup);
    fontSizeSpinBox_->setRange(7, 36);
    fontSizeSpinBox_->setSuffix(" pt");
    sizeLayout->addWidget(fontSizeSpinBox_);

    chooseFontBtn_ = new QPushButton("Browse System Fonts...", fontGroup);
    sizeLayout->addWidget(chooseFontBtn_);
    sizeLayout->addStretch();
    fontForm->addRow("Font Size:", sizeLayout);

    fontLayout->addLayout(fontForm);

    // Font preview panel
    auto* previewBox = new QFrame(fontGroup);
    previewBox->setFrameShape(QFrame::StyledPanel);
    previewBox->setFrameShadow(QFrame::Sunken);
    previewBox->setMinimumHeight(60);
    auto* previewInnerLayout = new QVBoxLayout(previewBox);
    previewInnerLayout->setContentsMargins(10, 6, 10, 6);

    fontPreviewLabel_ = new QLabel("OpenArch: The quick brown fox jumps over the lazy dog 1234567890", previewBox);
    fontPreviewLabel_->setWordWrap(true);
    fontPreviewLabel_->setAlignment(Qt::AlignCenter);
    previewInnerLayout->addWidget(fontPreviewLabel_);

    fontLayout->addWidget(new QLabel("Preview:", fontGroup));
    fontLayout->addWidget(previewBox);

    auto* fontButtonsLayout = new QHBoxLayout();
    resetFontBtn_ = new QPushButton("Reset to System Default Font", fontGroup);
    fontButtonsLayout->addWidget(resetFontBtn_);
    fontButtonsLayout->addStretch();
    fontLayout->addLayout(fontButtonsLayout);

    tabLayout->addWidget(fontGroup);
    tabLayout->addStretch();

    return tab;
}

QWidget* PreferencesDialog::createGeneralTab()
{
    auto* tab = new QWidget(this);
    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(10, 12, 10, 10);
    tabLayout->setSpacing(12);

    // Auto-Save Group
    auto* autoSaveGroup = new QGroupBox("Auto-Save & Database Persistence", tab);
    auto* autoSaveLayout = new QVBoxLayout(autoSaveGroup);
    autoSaveLayout->setSpacing(10);

    autoSaveCheck_ = new QCheckBox("Enable Auto-Save (automatically saves layout changes to database)", autoSaveGroup);
    autoSaveLayout->addWidget(autoSaveCheck_);

    auto* delayFormLayout = new QHBoxLayout();
    auto* delayLabel = new QLabel("Auto-Save debounce delay:", autoSaveGroup);
    autoSaveDelayCombo_ = new QComboBox(autoSaveGroup);
    autoSaveDelayCombo_->addItem("250 ms (Fast)", 250);
    autoSaveDelayCombo_->addItem("500 ms (Recommended)", 500);
    autoSaveDelayCombo_->addItem("1.0 second", 1000);
    autoSaveDelayCombo_->addItem("2.0 seconds", 2000);
    delayFormLayout->addWidget(delayLabel);
    delayFormLayout->addWidget(autoSaveDelayCombo_);
    delayFormLayout->addStretch();
    autoSaveLayout->addLayout(delayFormLayout);

    auto* autoSaveHint = new QLabel(
        "<small style='color: gray;'>"
        "When auto-save is enabled, layout edits are debounced and committed automatically in the background.<br>"
        "When auto-save is disabled, layout modifications remain in memory until manually committed "
        "using <b>File &rarr; Save Layout (Ctrl+S)</b> or by clicking the unsaved changes status badge."
        "</small>", autoSaveGroup);
    autoSaveHint->setWordWrap(true);
    autoSaveLayout->addWidget(autoSaveHint);

    tabLayout->addWidget(autoSaveGroup);

    // Confirmation & Safety Group
    auto* safetyGroup = new QGroupBox("Safety & Prompts", tab);
    auto* safetyLayout = new QVBoxLayout(safetyGroup);
    safetyLayout->setSpacing(8);

    auto* safetyLabel = new QLabel(
        "OpenArch safeguards against accidental data loss. If auto-save is turned off and unsaved "
        "layout changes exist, OpenArch will prompt for confirmation before opening another database "
        "or closing the application.", safetyGroup);
    safetyLabel->setWordWrap(true);
    safetyLayout->addWidget(safetyLabel);

    tabLayout->addWidget(safetyGroup);
    tabLayout->addStretch();

    return tab;
}

QWidget* PreferencesDialog::createShortcutsTab()
{
    auto* tab = new QWidget(this);
    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(10, 12, 10, 10);
    tabLayout->setSpacing(12);

    auto* shortcutGroup = new QGroupBox("Keyboard Shortcuts Configuration", tab);
    auto* groupLayout = new QVBoxLayout(shortcutGroup);
    groupLayout->setSpacing(12);

    auto* descLabel = new QLabel(
        "OpenArch supports configurable keyboard shortcuts for canvas navigation, element positioning, "
        "and editor modes. Core OS standards (Cut <b>Ctrl+X</b>, Copy <b>Ctrl+C</b>, Paste <b>Ctrl+V</b>) "
        "are protected against conflicts.", shortcutGroup);
    descLabel->setWordWrap(true);
    groupLayout->addWidget(descLabel);

    auto* tableFrame = new QFrame(shortcutGroup);
    tableFrame->setFrameShape(QFrame::StyledPanel);
    auto* tableLayout = new QFormLayout(tableFrame);
    tableLayout->setSpacing(6);
    tableLayout->addRow("View Mode:", new QLabel("<b>V</b>"));
    tableLayout->addRow("Edit Mode:", new QLabel("<b>L</b>"));
    tableLayout->addRow("Save Layout:", new QLabel("<b>Ctrl+S</b>"));
    tableLayout->addRow("Duplicate Node:", new QLabel("<b>Ctrl+D</b>"));
    tableLayout->addRow("Connect Selected:", new QLabel("<b>Ctrl+K</b>"));
    tableLayout->addRow("Preferences:", new QLabel("<b>Ctrl+,</b>"));
    groupLayout->addWidget(tableFrame);

    auto* btnLayout = new QHBoxLayout();
    openShortcutsBtn_ = new QPushButton("Configure Keyboard Shortcuts...", shortcutGroup);
    btnLayout->addWidget(openShortcutsBtn_);
    btnLayout->addStretch();
    groupLayout->addLayout(btnLayout);

    tabLayout->addWidget(shortcutGroup);
    tabLayout->addStretch();

    return tab;
}

void PreferencesDialog::onConfigureShortcutsClicked()
{
    ShortcutConfigDialog dlg(this);
    dlg.exec();
}

void PreferencesDialog::populateThemes()
{
    themeCombo_->blockSignals(true);
    themeCombo_->clear();

    const auto themes = ApplicationThemeManager::availableThemes();
    const QString currentId = selectedThemeId_.isEmpty()
        ? ApplicationThemeManager::currentThemeId()
        : selectedThemeId_;

    int selectIndex = 0;
    bool hasCustomSeparator = false;

    for (int i = 0; i < themes.size(); ++i)
    {
        const auto& th = themes[i];
        if (!th.isBuiltIn && !hasCustomSeparator)
        {
            themeCombo_->insertSeparator(themeCombo_->count());
            hasCustomSeparator = true;
        }

        QString displayName = th.name;
        if (!th.isBuiltIn)
        {
            displayName += " (Custom)";
        }
        themeCombo_->addItem(displayName, th.id);

        if (th.id.compare(currentId, Qt::CaseInsensitive) == 0)
        {
            selectIndex = themeCombo_->count() - 1;
        }
    }

    themeCombo_->setCurrentIndex(selectIndex);
    themeCombo_->blockSignals(false);

    updateThemePreview();
}

void PreferencesDialog::updateThemePreview()
{
    QString themeId = themeCombo_->currentData().toString();
    if (themeId.isEmpty())
        themeId = ApplicationThemeManager::currentThemeId();

    selectedThemeId_ = themeId;

    const auto colors = ApplicationThemeManager::colorsForTheme(themeId);

    // Clear existing color swatches
    if (themeColorSwatches_ && themeColorSwatches_->layout())
    {
        QLayoutItem* child;
        while ((child = themeColorSwatches_->layout()->takeAt(0)) != nullptr)
        {
            delete child->widget();
            delete child;
        }

        auto addSwatch = [this](const QString& name, const QColor& color) {
            auto* pill = new QLabel(themeColorSwatches_);
            pill->setText(QString(" %1 ").arg(name));
            pill->setStyleSheet(QString(
                "background-color: %1; "
                "color: %2; "
                "border: 1px solid rgba(128,128,128,0.5); "
                "border-radius: 4px; "
                "padding: 2px 6px; "
                "font-size: 10px; font-weight: bold;"
            ).arg(color.name(), (color.lightness() > 140 ? "#000000" : "#ffffff")));
            themeColorSwatches_->layout()->addWidget(pill);
        };

        addSwatch("Window", colors.window);
        addSwatch("Base", colors.base);
        addSwatch("Button", colors.button);
        addSwatch("Accent", colors.accent);
        addSwatch("Text", colors.text);
        static_cast<QHBoxLayout*>(themeColorSwatches_->layout())->addStretch();
    }

    if (themeInfoLabel_)
    {
        themeInfoLabel_->setText(QString("Mode: %1 | Accent: %2 | Window: %3")
            .arg(colors.isDark ? "Dark" : "Light")
            .arg(colors.accent.name())
            .arg(colors.window.name()));
    }
}

void PreferencesDialog::loadCurrentValues()
{
    const auto& cfg = ApplicationConfig::instance();

    // 1. Auto-Save
    autoSaveCheck_->setChecked(cfg.isAutoSaveEnabled());
    int delay = cfg.autoSaveDelayMs();
    int idx = autoSaveDelayCombo_->findData(delay);
    if (idx >= 0)
    {
        autoSaveDelayCombo_->setCurrentIndex(idx);
    }
    else
    {
        autoSaveDelayCombo_->addItem(QString("%1 ms").arg(delay), delay);
        autoSaveDelayCombo_->setCurrentIndex(autoSaveDelayCombo_->count() - 1);
    }
    autoSaveDelayCombo_->setEnabled(cfg.isAutoSaveEnabled());

    // 2. Font
    previewFont_ = cfg.applicationFont();
    fontComboBox_->setCurrentFont(previewFont_);
    int pt = previewFont_.pointSize();
    if (pt <= 0)
        pt = 10;
    fontSizeSpinBox_->setValue(pt);
    updateFontPreview();

    // 3. Theme
    initialThemeId_ = ApplicationThemeManager::currentThemeId();
    selectedThemeId_ = initialThemeId_;
    populateThemes();
}

void PreferencesDialog::updateFontPreview()
{
    if (fontPreviewLabel_)
    {
        fontPreviewLabel_->setFont(previewFont_);
    }
}

void PreferencesDialog::onThemeSelectionChanged(int)
{
    updateThemePreview();
}

void PreferencesDialog::onLoadCustomThemeClicked()
{
    const QString customDir = ApplicationThemeManager::customThemesDirectory();
    QString file = QFileDialog::getOpenFileName(
        this,
        "Load Custom Application UI Theme",
        customDir,
        "UI Theme Files (*.json *.qss);;JSON Theme (*.json);;QSS Stylesheet (*.qss);;All Files (*.*)"
    );

    if (file.isEmpty())
        return;

    QString error;
    if (ApplicationThemeManager::loadCustomThemeFile(file, &error))
    {
        selectedThemeId_ = ApplicationThemeManager::currentThemeId();
        populateThemes();
    }
    else
    {
        QMessageBox::warning(this, "Theme Error", error.isEmpty() ? "Failed to load custom UI theme." : error);
    }
}

void PreferencesDialog::onOpenCustomFolderClicked()
{
    const QString dir = ApplicationThemeManager::customThemesDirectory();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void PreferencesDialog::onFontSelectionChanged()
{
    previewFont_.setFamily(fontComboBox_->currentFont().family());
    previewFont_.setPointSize(fontSizeSpinBox_->value());
    updateFontPreview();
}

void PreferencesDialog::onChooseFontClicked()
{
    bool ok = false;
    QFont selected = QFontDialog::getFont(&ok, previewFont_, this, "Choose Application Font");
    if (ok)
    {
        previewFont_ = selected;
        fontComboBox_->setCurrentFont(previewFont_);
        int pt = previewFont_.pointSize();
        if (pt <= 0)
            pt = 10;
        fontSizeSpinBox_->setValue(pt);
        updateFontPreview();
    }
}

void PreferencesDialog::onResetFontClicked()
{
    previewFont_ = ApplicationConfig::instance().systemDefaultFont();
    fontComboBox_->setCurrentFont(previewFont_);
    int pt = previewFont_.pointSize();
    if (pt <= 0)
        pt = 10;
    fontSizeSpinBox_->setValue(pt);
    updateFontPreview();
}

void PreferencesDialog::onApply()
{
    // 1. Auto-Save
    auto& cfg = ApplicationConfig::instance();
    cfg.setAutoSaveEnabled(autoSaveCheck_->isChecked());
    cfg.setAutoSaveDelayMs(autoSaveDelayCombo_->currentData().toInt());

    // 2. Font
    cfg.setApplicationFont(previewFont_);

    // 3. Application UI Theme
    QString chosenThemeId = themeCombo_->currentData().toString();
    if (!chosenThemeId.isEmpty())
    {
        ApplicationThemeManager::setThemeById(chosenThemeId);
        selectedThemeId_ = chosenThemeId;
    }
}

void PreferencesDialog::onAccepted()
{
    onApply();
    accept();
}
