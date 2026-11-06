#pragma once

#include <QDialog>
#include <QFont>
#include <QString>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QSpinBox;
class QLabel;
class QPushButton;
class QWidget;
class QTabWidget;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override = default;

    // Accessors for verification and testing
    QTabWidget* tabWidget() const { return tabWidget_; }
    QWidget* appearanceTab() const { return appearanceTab_; }
    QWidget* generalTab() const { return generalTab_; }
    QWidget* shortcutsTab() const { return shortcutsTab_; }

    QComboBox* themeComboBox() const { return themeCombo_; }
    QFontComboBox* fontComboBox() const { return fontComboBox_; }
    QSpinBox* fontSizeSpinBox() const { return fontSizeSpinBox_; }
    QCheckBox* autoSaveCheckBox() const { return autoSaveCheck_; }
    QComboBox* autoSaveDelayComboBox() const { return autoSaveDelayCombo_; }

private slots:
    void onThemeSelectionChanged(int index);
    void onLoadCustomThemeClicked();
    void onOpenCustomFolderClicked();
    void onFontSelectionChanged();
    void onChooseFontClicked();
    void onResetFontClicked();
    void onConfigureShortcutsClicked();
    void onApply();
    void onAccepted();

private:
    void setupUi();
    QWidget* createAppearanceTab();
    QWidget* createGeneralTab();
    QWidget* createShortcutsTab();

    void loadCurrentValues();
    void populateThemes();
    void updateThemePreview();
    void updateFontPreview();

    QTabWidget* tabWidget_{nullptr};
    QWidget* appearanceTab_{nullptr};
    QWidget* generalTab_{nullptr};
    QWidget* shortcutsTab_{nullptr};

    // Theme (Application) controls
    QComboBox* themeCombo_{nullptr};
    QPushButton* loadCustomThemeBtn_{nullptr};
    QPushButton* openCustomFolderBtn_{nullptr};
    QLabel* themeInfoLabel_{nullptr};
    QWidget* themeColorSwatches_{nullptr};

    // Font controls
    QFontComboBox* fontComboBox_{nullptr};
    QSpinBox* fontSizeSpinBox_{nullptr};
    QLabel* fontPreviewLabel_{nullptr};
    QPushButton* chooseFontBtn_{nullptr};
    QPushButton* resetFontBtn_{nullptr};

    // Auto-Save controls
    QCheckBox* autoSaveCheck_{nullptr};
    QComboBox* autoSaveDelayCombo_{nullptr};

    // Shortcuts tab controls
    QPushButton* openShortcutsBtn_{nullptr};

    QFont previewFont_;
    QString selectedThemeId_;
    QString initialThemeId_;
};
