#include "LayerEditorDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QTabWidget>
#include <QLabel>
#include <QGroupBox>

LayerEditorDialog::LayerEditorDialog(ArchitectureModel* model,
                                     LayerId layerId,
                                     QWidget* parent)
    : QDialog(parent),
      model_(model),
      layerId_(layerId)
{
    setWindowTitle(layerId_ == 0 ? "New Layer" : "Edit Layer");
    resize(580, 620);

    LayerData layer{};
    if (layerId_ != 0) {
        auto opt = model_->getLayerById(layerId_);
        if (!opt) {
            QMessageBox::critical(this, "Error", "Layer not found");
            reject();
            return;
        }
        layer = *opt;

        for (const auto& nl : model_->nodesInLayer(layerId_)) {
            stagedNodes_.insert(nl.nodeId);
        }
        originalNodes_ = stagedNodes_;
    }

    // Basic Fields
    nameEdit_ = new QLineEdit(QString::fromStdString(layer.name));
    kindEdit_ = new QLineEdit(QString::fromStdString(layer.kind));

    auto* basicForm = new QFormLayout;
    basicForm->addRow("Layer Name", nameEdit_);
    basicForm->addRow("Kind / Category", kindEdit_);

    // Node Membership Dual-List
    filterEdit_ = new QLineEdit;
    filterEdit_->setPlaceholderText("Filter available nodes...");

    availableNodes_ = new QListWidget;
    currentNodes_   = new QListWidget;

    populateMembershipUI();

    auto* addBtn = new QPushButton(">>");
    auto* rmBtn  = new QPushButton("<<");

    connect(addBtn, &QPushButton::clicked, this, &LayerEditorDialog::onAddNode);
    connect(rmBtn,  &QPushButton::clicked, this, &LayerEditorDialog::onRemoveNode);
    connect(availableNodes_, &QListWidget::itemDoubleClicked, this, &LayerEditorDialog::onAddNode);
    connect(currentNodes_, &QListWidget::itemDoubleClicked, this, &LayerEditorDialog::onRemoveNode);
    connect(filterEdit_, &QLineEdit::textChanged, this, &LayerEditorDialog::onFilterChanged);

    auto* leftBox = new QVBoxLayout;
    leftBox->addWidget(filterEdit_);
    leftBox->addWidget(availableNodes_);

    auto* mid = new QVBoxLayout;
    mid->addStretch();
    mid->addWidget(addBtn);
    mid->addWidget(rmBtn);
    mid->addStretch();

    auto* listLayout = new QHBoxLayout;
    listLayout->addLayout(leftBox);
    listLayout->addLayout(mid);
    listLayout->addWidget(currentNodes_);

    basicForm->addRow("Nodes", listLayout);

    // JSON Tree Editors
    attributesEditor_ = new JsonTreeEditor(this);
    attributesEditor_->setJson(QString::fromStdString(layer.attributes));

    metadataEditor_ = new JsonTreeEditor(this);
    metadataEditor_->setJson(QString::fromStdString(layer.metadata));

    // Tab Widget
    auto* tabWidget = new QTabWidget(this);

    auto* generalTab = new QWidget(this);
    generalTab->setLayout(basicForm);

    tabWidget->addTab(generalTab, "General");
    tabWidget->addTab(attributesEditor_, "Attributes");
    tabWidget->addTab(metadataEditor_, "Metadata");

    // Governance Tab
    auto* govTab = new QWidget(this);
    auto* govLayout = new QVBoxLayout(govTab);
    auto* govForm = new QFormLayout;

    statusCombo_ = new QComboBox(this);
    statusCombo_->addItem("New", static_cast<int>(Status::New));
    statusCombo_->addItem("Changed", static_cast<int>(Status::Changed));
    statusCombo_->addItem("Reviewed", static_cast<int>(Status::Reviewed));
    statusCombo_->addItem("Approved", static_cast<int>(Status::Approved));
    statusCombo_->addItem("Invalid", static_cast<int>(Status::Invalid));
    statusCombo_->addItem("Deleted", static_cast<int>(Status::Deleted));

    int statusIdx = statusCombo_->findData(static_cast<int>(layer.status));
    if (statusIdx >= 0) statusCombo_->setCurrentIndex(statusIdx);
    govForm->addRow("Lifecycle Status:", statusCombo_);

    auto* revRow = new QWidget(this);
    auto* revLayout = new QHBoxLayout(revRow);
    revLayout->setContentsMargins(0, 0, 0, 0);
    reviewerEdit_ = new QLineEdit(QString::fromStdString(layer.reviewer), this);
    reviewerEdit_->setPlaceholderText("Reviewer ID or name...");
    auto* currentUserBtn = new QPushButton("Current User", this);
    revLayout->addWidget(reviewerEdit_, 1);
    revLayout->addWidget(currentUserBtn);

    connect(currentUserBtn, &QPushButton::clicked, this, [this]() {
        QString user = QString::fromLocal8Bit(qgetenv("USER"));
        if (user.isEmpty()) user = QString::fromLocal8Bit(qgetenv("USERNAME"));
        if (user.isEmpty()) user = "architect";
        reviewerEdit_->setText(user);
    });
    govForm->addRow("Reviewer:", revRow);

    checksumValLabel_ = new QLabel(this);
    uint32_t currentComputed = (layerId_ != 0) ? model_->computeLayerChecksum(layer) : 0;
    bool isIntegrityValid = (layerId_ == 0) || (currentComputed == layer.checksum);

    checksumValLabel_->setText(QString("0x%1 (%2)")
        .arg(QString::number(layer.checksum, 16).toUpper(), QString::number(layer.checksum)));
    checksumValLabel_->setStyleSheet("font-family: monospace; font-weight: bold;");

    integrityBadgeLabel_ = new QLabel(this);
    if (isIntegrityValid) {
        integrityBadgeLabel_->setText("✓ Checksum Valid (Tamper-Free)");
        integrityBadgeLabel_->setStyleSheet("color: #a3be8c; font-weight: bold;");
    } else {
        integrityBadgeLabel_->setText(QString("⚠ Checksum Mismatch (Stored: 0x%1, Expected: 0x%2)")
            .arg(QString::number(layer.checksum, 16).toUpper(), QString::number(currentComputed, 16).toUpper()));
        integrityBadgeLabel_->setStyleSheet("color: #bf616a; font-weight: bold;");
    }

    govForm->addRow("Stored Checksum:", checksumValLabel_);
    govForm->addRow("Integrity Status:", integrityBadgeLabel_);
    govLayout->addLayout(govForm);

    auto* quickBox = new QGroupBox("Quick Actions", this);
    auto* quickLayout = new QHBoxLayout(quickBox);
    auto* reviewBtn = new QPushButton("Mark as Reviewed", this);
    auto* approveBtn = new QPushButton("Mark as Approved", this);
    auto* changedBtn = new QPushButton("Mark as Changed", this);
    quickLayout->addWidget(reviewBtn);
    quickLayout->addWidget(approveBtn);
    quickLayout->addWidget(changedBtn);
    govLayout->addWidget(quickBox);
    govLayout->addStretch(1);

    auto ensureReviewer = [this]() {
        if (reviewerEdit_->text().trimmed().isEmpty()) {
            QString user = QString::fromLocal8Bit(qgetenv("USER"));
            if (user.isEmpty()) user = QString::fromLocal8Bit(qgetenv("USERNAME"));
            if (user.isEmpty()) user = "architect";
            reviewerEdit_->setText(user);
        }
    };

    connect(reviewBtn, &QPushButton::clicked, this, [this, ensureReviewer]() {
        ensureReviewer();
        int idx = statusCombo_->findData(static_cast<int>(Status::Reviewed));
        if (idx >= 0) statusCombo_->setCurrentIndex(idx);
    });

    connect(approveBtn, &QPushButton::clicked, this, [this, ensureReviewer]() {
        ensureReviewer();
        int idx = statusCombo_->findData(static_cast<int>(Status::Approved));
        if (idx >= 0) statusCombo_->setCurrentIndex(idx);
    });

    connect(changedBtn, &QPushButton::clicked, this, [this]() {
        reviewerEdit_->clear();
        int idx = statusCombo_->findData(static_cast<int>(Status::Changed));
        if (idx >= 0) statusCombo_->setCurrentIndex(idx);
    });

    tabWidget->addTab(govTab, "Governance");

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel);

    connect(buttons, &QDialogButtonBox::accepted, this, &LayerEditorDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(buttons);
}

void LayerEditorDialog::populateMembershipUI()
{
    availableNodes_->clear();
    currentNodes_->clear();

    const QString filterText = filterEdit_ ? filterEdit_->text().trimmed().toLower() : QString();

    for (const auto& n : model_->nodes()) {
        auto* item = new QListWidgetItem(QString::fromStdString(n.name));
        item->setData(Qt::UserRole, static_cast<qulonglong>(n.id));

        if (stagedNodes_.count(n.id)) {
            currentNodes_->addItem(item);
        } else {
            if (filterText.isEmpty() || item->text().toLower().contains(filterText)) {
                availableNodes_->addItem(item);
            }
        }
    }
}

void LayerEditorDialog::onFilterChanged(const QString&)
{
    populateMembershipUI();
}

void LayerEditorDialog::onAddNode()
{
    auto* item = availableNodes_->currentItem();
    if (!item) return;

    NodeId id = item->data(Qt::UserRole).toULongLong();
    stagedNodes_.insert(id);
    populateMembershipUI();
}

void LayerEditorDialog::onRemoveNode()
{
    auto* item = currentNodes_->currentItem();
    if (!item) return;

    NodeId id = item->data(Qt::UserRole).toULongLong();
    stagedNodes_.erase(id);
    populateMembershipUI();
}

void LayerEditorDialog::onSave()
{
    LayerData layer;
    layer.id = layerId_;
    layer.name = nameEdit_->text().toStdString();
    layer.kind = kindEdit_->text().toStdString();

    // Export JSON directly from visual tree editors
    layer.attributes = attributesEditor_->toJsonString(QJsonDocument::Compact).toStdString();
    layer.metadata   = metadataEditor_->toJsonString(QJsonDocument::Compact).toStdString();

    Result r;
    if (!layerId_) {
        LayerId newId = 0;
        r = model_->addLayer(layer, newId);
        layerId_ = newId;
    } else {
        r = model_->updateLayer(layer);
    }

    if (!r.ok) {
        QMessageBox::critical(this, "Error", QString::fromStdString(r.message));
        return;
    }

    // Apply governance settings
    if (statusCombo_) {
        auto chosenStatus = static_cast<Status>(statusCombo_->currentData().toInt());
        std::string chosenReviewer = reviewerEdit_ ? reviewerEdit_->text().trimmed().toStdString() : "";
        model_->setLayerGovernance(layerId_, chosenStatus, chosenReviewer);
    }

    // Commit staged node memberships
    for (NodeId nid : stagedNodes_) {
        if (!originalNodes_.count(nid)) {
            model_->addNodeToLayer(nid, layerId_);
        }
    }
    for (NodeId nid : originalNodes_) {
        if (!stagedNodes_.count(nid)) {
            model_->removeNodeFromLayer(nid, layerId_);
        }
    }

    accept();
}