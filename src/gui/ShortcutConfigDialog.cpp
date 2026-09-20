#include "ShortcutConfigDialog.h"
#include "ShortcutManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QComboBox>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QFont>
#include <QColor>

ShortcutConfigDialog::ShortcutConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    populateTable();

    if (table_->rowCount() > 0) {
        table_->selectRow(0);
    }
}

void ShortcutConfigDialog::setupUi()
{
    setWindowTitle("Configure Shortcuts");
    resize(720, 560);
    setMinimumSize(600, 440);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    // --- Header ---
    auto* headerWidget = new QWidget(this);
    auto* headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);

    auto* titleLabel = new QLabel("Keyboard Shortcuts", headerWidget);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* subtitleLabel = new QLabel(
        "Customize application shortcuts. Standard shortcuts (such as Cut, Copy, Paste, Delete, and Find) are system-standard and cannot be modified.",
        headerWidget);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setStyleSheet("color: #888888; font-size: 12px;");

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subtitleLabel);
    mainLayout->addWidget(headerWidget);

    // --- Filter Row ---
    auto* filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText("Search actions, shortcuts, or categories...");
    searchEdit_->setClearButtonEnabled(true);

    categoryCombo_ = new QComboBox(this);
    categoryCombo_->addItem("All Categories", "all");
    categoryCombo_->addItem("Configurable Only", "configurable");
    categoryCombo_->addItem("Standard (Locked) Only", "standard");
    categoryCombo_->addItem("Mode", "Mode");
    categoryCombo_->addItem("Edit", "Edit");
    categoryCombo_->addItem("Layout", "Layout");
    categoryCombo_->addItem("Alignment", "Alignment");
    categoryCombo_->addItem("Export", "Export");

    filterLayout->addWidget(new QLabel("Filter:", this));
    filterLayout->addWidget(searchEdit_, 2);
    filterLayout->addWidget(categoryCombo_, 1);
    mainLayout->addLayout(filterLayout);

    // --- Table ---
    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({"Action", "Category", "Shortcut", "Type"});
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    mainLayout->addWidget(table_, 1);

    // --- Editor Group Box ---
    editGroup_ = new QGroupBox("Shortcut Configuration", this);
    auto* editLayout = new QVBoxLayout(editGroup_);
    editLayout->setSpacing(8);

    selectedActionLabel_ = new QLabel("Select an action to view or configure its shortcut.", editGroup_);
    editLayout->addWidget(selectedActionLabel_);

    auto* inputRow = new QHBoxLayout();
    inputRow->setSpacing(8);

    inputRow->addWidget(new QLabel("New Shortcut:", editGroup_));

    keySequenceEdit_ = new QKeySequenceEdit(editGroup_);
    keySequenceEdit_->setEnabled(false);
    inputRow->addWidget(keySequenceEdit_, 1);

    clearBtn_ = new QPushButton("Clear", editGroup_);
    clearBtn_->setToolTip("Remove current shortcut");
    clearBtn_->setEnabled(false);
    inputRow->addWidget(clearBtn_);

    resetSelectedBtn_ = new QPushButton("Reset to Default", editGroup_);
    resetSelectedBtn_->setToolTip("Reset this action to its default shortcut");
    resetSelectedBtn_->setEnabled(false);
    inputRow->addWidget(resetSelectedBtn_);

    editLayout->addLayout(inputRow);

    warningLabel_ = new QLabel(editGroup_);
    warningLabel_->setWordWrap(true);
    warningLabel_->setStyleSheet("color: #e06c75; font-size: 11px;");
    editLayout->addWidget(warningLabel_);

    mainLayout->addWidget(editGroup_);

    // --- Bottom Controls ---
    auto* bottomLayout = new QHBoxLayout();
    resetAllBtn_ = new QPushButton("Reset All to Defaults", this);
    resetAllBtn_->setToolTip("Reset all configurable shortcuts to their initial defaults");
    bottomLayout->addWidget(resetAllBtn_);

    bottomLayout->addStretch(1);

    applyBtn_ = new QPushButton("Apply", this);
    applyBtn_->setEnabled(false);
    bottomLayout->addWidget(applyBtn_);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bottomLayout->addWidget(buttonBox_);

    mainLayout->addLayout(bottomLayout);

    // Connections
    connect(searchEdit_, &QLineEdit::textChanged, this, &ShortcutConfigDialog::onSearchTextChanged);
    connect(categoryCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShortcutConfigDialog::onCategoryFilterChanged);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &ShortcutConfigDialog::onTableSelectionChanged);
    connect(keySequenceEdit_, &QKeySequenceEdit::keySequenceChanged, this, &ShortcutConfigDialog::onKeySequenceChanged);
    connect(clearBtn_, &QPushButton::clicked, this, &ShortcutConfigDialog::onClearShortcutClicked);
    connect(resetSelectedBtn_, &QPushButton::clicked, this, &ShortcutConfigDialog::onResetSelectedClicked);
    connect(resetAllBtn_, &QPushButton::clicked, this, &ShortcutConfigDialog::onResetAllClicked);
    connect(applyBtn_, &QPushButton::clicked, this, &ShortcutConfigDialog::onApply);
    connect(buttonBox_, &QDialogButtonBox::accepted, this, &ShortcutConfigDialog::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ShortcutConfigDialog::populateTable()
{
    isUpdatingUi_ = true;
    table_->setRowCount(0);
    pendingShortcuts_.clear();

    const auto& items = ShortcutManager::instance()->items();
    table_->setRowCount(static_cast<int>(items.size()));

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const auto& item = items[i];
        pendingShortcuts_[item.id.toStdString()] = item.currentKey;

        // Column 0: Action Name
        auto* nameItem = new QTableWidgetItem(item.name);
        nameItem->setData(Qt::UserRole, item.id);
        if (!item.description.isEmpty()) {
            nameItem->setToolTip(item.description);
        }

        // Column 1: Category
        auto* catItem = new QTableWidgetItem(item.category);

        // Column 2: Shortcut
        QString shortcutStr = item.currentKey.toString(QKeySequence::NativeText);
        auto* keyItem = new QTableWidgetItem(shortcutStr.isEmpty() ? "(None)" : shortcutStr);

        // Column 3: Type
        auto* typeItem = new QTableWidgetItem();
        if (item.isStandard) {
            typeItem->setText("Standard (Locked)");
            typeItem->setToolTip("Standard system shortcut. Preserved and not configurable.");

            QColor mutedColor(140, 140, 140);
            nameItem->setForeground(mutedColor);
            catItem->setForeground(mutedColor);
            keyItem->setForeground(mutedColor);
            typeItem->setForeground(mutedColor);
        } else {
            typeItem->setText("Configurable");
            typeItem->setForeground(QColor(77, 170, 252));
        }

        table_->setItem(i, 0, nameItem);
        table_->setItem(i, 1, catItem);
        table_->setItem(i, 2, keyItem);
        table_->setItem(i, 3, typeItem);
    }

    isUpdatingUi_ = false;
    filterRows();
}

int ShortcutConfigDialog::findRowByActionId(const QString& actionId) const
{
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* item = table_->item(row, 0);
        if (item && item->data(Qt::UserRole).toString() == actionId) {
            return row;
        }
    }
    return -1;
}

void ShortcutConfigDialog::updateTableRowShortcut(const QString& actionId, const QKeySequence& seq)
{
    int row = findRowByActionId(actionId);
    if (row >= 0) {
        auto* item = table_->item(row, 2);
        if (item) {
            QString str = seq.toString(QKeySequence::NativeText);
            item->setText(str.isEmpty() ? "(None)" : str);
        }
    }
}

void ShortcutConfigDialog::filterRows()
{
    QString searchText = searchEdit_->text().trimmed();
    QString catFilter = categoryCombo_->currentData().toString();

    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* nameItem = table_->item(row, 0);
        auto* catItem  = table_->item(row, 1);
        auto* keyItem  = table_->item(row, 2);
        auto* typeItem = table_->item(row, 3);

        if (!nameItem || !catItem || !keyItem || !typeItem) continue;

        QString actionId = nameItem->data(Qt::UserRole).toString();
        const auto* item = ShortcutManager::instance()->getItem(actionId);
        bool isStandard = item ? item->isStandard : false;

        // Category filter
        bool catMatch = true;
        if (catFilter == "configurable") {
            catMatch = !isStandard;
        } else if (catFilter == "standard") {
            catMatch = isStandard;
        } else if (catFilter != "all") {
            catMatch = (catItem->text().compare(catFilter, Qt::CaseInsensitive) == 0);
        }

        // Text search filter
        bool textMatch = true;
        if (!searchText.isEmpty()) {
            textMatch = nameItem->text().contains(searchText, Qt::CaseInsensitive) ||
                        catItem->text().contains(searchText, Qt::CaseInsensitive) ||
                        keyItem->text().contains(searchText, Qt::CaseInsensitive) ||
                        typeItem->text().contains(searchText, Qt::CaseInsensitive);
        }

        table_->setRowHidden(row, !(catMatch && textMatch));
    }
}

void ShortcutConfigDialog::onSearchTextChanged(const QString&)
{
    filterRows();
}

void ShortcutConfigDialog::onCategoryFilterChanged(int)
{
    filterRows();
}

void ShortcutConfigDialog::onTableSelectionChanged()
{
    updateEditorState();
}

void ShortcutConfigDialog::updateEditorState()
{
    auto selectedRows = table_->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        currentlyEditingId_.clear();
        selectedActionLabel_->setText("Select an action above to configure its shortcut.");
        keySequenceEdit_->setEnabled(false);
        clearBtn_->setEnabled(false);
        resetSelectedBtn_->setEnabled(false);
        warningLabel_->clear();
        return;
    }

    int row = selectedRows.first().row();
    auto* nameItem = table_->item(row, 0);
    if (!nameItem) return;

    currentlyEditingId_ = nameItem->data(Qt::UserRole).toString();
    const auto* item = ShortcutManager::instance()->getItem(currentlyEditingId_);
    if (!item) return;

    isUpdatingUi_ = true;

    if (item->isStandard) {
        editGroup_->setTitle("Shortcut Configuration (Standard - Locked)");
        selectedActionLabel_->setText(QString("<b>%1</b> &nbsp;·&nbsp; <i>Standard System Shortcut</i>").arg(item->name));
        keySequenceEdit_->setEnabled(false);
        clearBtn_->setEnabled(false);
        resetSelectedBtn_->setEnabled(false);
        keySequenceEdit_->setKeySequence(item->currentKey);
        warningLabel_->setText("🔒 Standard shortcuts (Cut, Copy, Paste, Delete, Find) are system-reserved and cannot be modified.");
        warningLabel_->setStyleSheet("color: #888888; font-size: 11px;");
    } else {
        editGroup_->setTitle("Shortcut Configuration");
        QString defaultStr = item->defaultKey.toString(QKeySequence::NativeText);
        selectedActionLabel_->setText(QString("<b>%1</b> &nbsp;·&nbsp; Category: %2 &nbsp;·&nbsp; Default: %3")
            .arg(item->name, item->category, defaultStr.isEmpty() ? "(None)" : defaultStr));
        keySequenceEdit_->setEnabled(true);
        clearBtn_->setEnabled(true);
        resetSelectedBtn_->setEnabled(true);

        auto it = pendingShortcuts_.find(currentlyEditingId_.toStdString());
        QKeySequence currentPending = (it != pendingShortcuts_.end()) ? it->second : item->currentKey;
        keySequenceEdit_->setKeySequence(currentPending);
        warningLabel_->clear();
    }

    isUpdatingUi_ = false;
}

void ShortcutConfigDialog::onKeySequenceChanged(const QKeySequence& seq)
{
    if (isUpdatingUi_ || currentlyEditingId_.isEmpty()) return;

    const auto* currentItem = ShortcutManager::instance()->getItem(currentlyEditingId_);
    if (!currentItem || currentItem->isStandard) return;

    if (seq.isEmpty()) {
        pendingShortcuts_[currentlyEditingId_.toStdString()] = QKeySequence();
        updateTableRowShortcut(currentlyEditingId_, QKeySequence());
        warningLabel_->clear();
        applyBtn_->setEnabled(true);
        return;
    }

    // 1. Validation: Prevent setting any standard shortcut
    QString stdName;
    if (ShortcutManager::instance()->isStandardKeySequence(seq, &stdName)) {
        warningLabel_->setText(QString("❌ '%1' is a standard shortcut reserved for '%2' and cannot be used.")
            .arg(seq.toString(QKeySequence::NativeText), stdName));
        warningLabel_->setStyleSheet("color: #e06c75; font-size: 11px; font-weight: bold;");

        isUpdatingUi_ = true;
        keySequenceEdit_->setKeySequence(pendingShortcuts_[currentlyEditingId_.toStdString()]);
        isUpdatingUi_ = false;
        return;
    }

    // 2. Validation: Check for conflict with another action
    QString conflictingId;
    QString conflictingName;
    for (const auto& otherItem : ShortcutManager::instance()->items()) {
        if (otherItem.id != currentlyEditingId_) {
            auto it = pendingShortcuts_.find(otherItem.id.toStdString());
            if (it != pendingShortcuts_.end() && !it->second.isEmpty() && it->second == seq) {
                conflictingId = otherItem.id;
                conflictingName = otherItem.name;
                break;
            }
        }
    }

    if (!conflictingId.isEmpty()) {
        auto reply = QMessageBox::question(
            this,
            "Shortcut Conflict",
            QString("The shortcut '%1' is already assigned to '%2'.\n\nDo you want to reassign it to '%3'?")
                .arg(seq.toString(QKeySequence::NativeText), conflictingName, currentItem->name),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // Unassign from conflicting action
            pendingShortcuts_[conflictingId.toStdString()] = QKeySequence();
            updateTableRowShortcut(conflictingId, QKeySequence());
        } else {
            // Revert
            isUpdatingUi_ = true;
            keySequenceEdit_->setKeySequence(pendingShortcuts_[currentlyEditingId_.toStdString()]);
            isUpdatingUi_ = false;
            return;
        }
    }

    // Apply to pending
    pendingShortcuts_[currentlyEditingId_.toStdString()] = seq;
    updateTableRowShortcut(currentlyEditingId_, seq);
    warningLabel_->clear();
    applyBtn_->setEnabled(true);
}

void ShortcutConfigDialog::onClearShortcutClicked()
{
    if (currentlyEditingId_.isEmpty()) return;

    const auto* item = ShortcutManager::instance()->getItem(currentlyEditingId_);
    if (!item || item->isStandard) return;

    pendingShortcuts_[currentlyEditingId_.toStdString()] = QKeySequence();
    isUpdatingUi_ = true;
    keySequenceEdit_->clear();
    isUpdatingUi_ = false;
    updateTableRowShortcut(currentlyEditingId_, QKeySequence());
    warningLabel_->clear();
    applyBtn_->setEnabled(true);
}

void ShortcutConfigDialog::onResetSelectedClicked()
{
    if (currentlyEditingId_.isEmpty()) return;

    const auto* item = ShortcutManager::instance()->getItem(currentlyEditingId_);
    if (!item || item->isStandard) return;

    QKeySequence defKey = item->defaultKey;

    if (!defKey.isEmpty()) {
        // Check standard conflict
        QString stdName;
        if (ShortcutManager::instance()->isStandardKeySequence(defKey, &stdName)) {
            warningLabel_->setText(QString("❌ Default shortcut '%1' conflicts with standard shortcut '%2'.")
                .arg(defKey.toString(QKeySequence::NativeText), stdName));
            warningLabel_->setStyleSheet("color: #e06c75; font-size: 11px; font-weight: bold;");
            return;
        }

        // Check configurable conflict
        for (const auto& otherItem : ShortcutManager::instance()->items()) {
            if (otherItem.id != currentlyEditingId_) {
                auto it = pendingShortcuts_.find(otherItem.id.toStdString());
                if (it != pendingShortcuts_.end() && !it->second.isEmpty() && it->second == defKey) {
                    pendingShortcuts_[otherItem.id.toStdString()] = QKeySequence();
                    updateTableRowShortcut(otherItem.id, QKeySequence());
                }
            }
        }
    }

    pendingShortcuts_[currentlyEditingId_.toStdString()] = defKey;
    isUpdatingUi_ = true;
    keySequenceEdit_->setKeySequence(defKey);
    isUpdatingUi_ = false;
    updateTableRowShortcut(currentlyEditingId_, defKey);
    warningLabel_->clear();
    applyBtn_->setEnabled(true);
}

void ShortcutConfigDialog::onResetAllClicked()
{
    auto reply = QMessageBox::question(
        this,
        "Reset All Shortcuts",
        "Are you sure you want to reset all configurable shortcuts to their default values?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    for (const auto& item : ShortcutManager::instance()->items()) {
        if (!item.isStandard) {
            pendingShortcuts_[item.id.toStdString()] = item.defaultKey;
            updateTableRowShortcut(item.id, item.defaultKey);
        }
    }

    if (!currentlyEditingId_.isEmpty()) {
        const auto* item = ShortcutManager::instance()->getItem(currentlyEditingId_);
        if (item && !item->isStandard) {
            isUpdatingUi_ = true;
            keySequenceEdit_->setKeySequence(item->defaultKey);
            isUpdatingUi_ = false;
        }
    }

    warningLabel_->clear();
    applyBtn_->setEnabled(true);
}

void ShortcutConfigDialog::onApply()
{
    auto* sm = ShortcutManager::instance();
    for (const auto& pair : pendingShortcuts_) {
        sm->setShortcut(QString::fromStdString(pair.first), pair.second);
    }
    sm->saveSettings();
    sm->applyShortcuts();
    applyBtn_->setEnabled(false);
}

void ShortcutConfigDialog::accept()
{
    onApply();
    QDialog::accept();
}
