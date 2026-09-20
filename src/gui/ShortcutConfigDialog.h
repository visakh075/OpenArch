#pragma once

#include <QDialog>
#include <QString>
#include <QKeySequence>
#include <unordered_map>
#include <string>

class QTableWidget;
class QLineEdit;
class QComboBox;
class QKeySequenceEdit;
class QPushButton;
class QLabel;
class QGroupBox;
class QDialogButtonBox;

class ShortcutConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutConfigDialog(QWidget* parent = nullptr);
    ~ShortcutConfigDialog() override = default;

private slots:
    void onSearchTextChanged(const QString& text);
    void onCategoryFilterChanged(int index);
    void onTableSelectionChanged();
    void onKeySequenceChanged(const QKeySequence& seq);
    void onClearShortcutClicked();
    void onResetSelectedClicked();
    void onResetAllClicked();
    void onApply();
    void accept() override;

private:
    void setupUi();
    void populateTable();
    void filterRows();
    void updateEditorState();
    void updateTableRowShortcut(const QString& actionId, const QKeySequence& seq);
    int findRowByActionId(const QString& actionId) const;

    QLineEdit* searchEdit_{nullptr};
    QComboBox* categoryCombo_{nullptr};
    QTableWidget* table_{nullptr};

    QGroupBox* editGroup_{nullptr};
    QLabel* selectedActionLabel_{nullptr};
    QKeySequenceEdit* keySequenceEdit_{nullptr};
    QPushButton* clearBtn_{nullptr};
    QPushButton* resetSelectedBtn_{nullptr};
    QLabel* warningLabel_{nullptr};

    QPushButton* resetAllBtn_{nullptr};
    QPushButton* applyBtn_{nullptr};
    QDialogButtonBox* buttonBox_{nullptr};

    // Staging map: actionId -> pending QKeySequence
    std::unordered_map<std::string, QKeySequence> pendingShortcuts_;
    QString currentlyEditingId_;
    bool isUpdatingUi_{false};
};
