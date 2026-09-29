#include "NodeEditorDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QTabWidget>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>

NodeEditorDialog::NodeEditorDialog(ArchitectureModel* model,
                                   NodeId nodeId,
                                   QWidget* parent)
    : QDialog(parent),
      model_(model),
      nodeId_(nodeId)
{
    setWindowTitle(nodeId_ == 0 ? "New Node" : "Edit Node");
    resize(580, 620);

    NodeData node{};
    if (nodeId_ != 0) {
        auto opt = model_->getNodeById(nodeId_);
        if (!opt) {
            QMessageBox::critical(this, "Error", "Node not found");
            reject();
            return;
        }
        node = *opt;
        initialParentId_ = node.parentId;

        for (const auto& nl : model_->layersForNode(nodeId_)) {
            stagedLayers_.insert(nl.layerId);
        }
        originalLayers_ = stagedLayers_;
    }

    // Basic Fields
    nameEdit_ = new QLineEdit(QString::fromStdString(node.name));
    typeEdit_ = new QLineEdit(QString::fromStdString(node.type));
    parentContainerCombo_ = new QComboBox();
    populateParentContainerUI(node.parentId);

    auto* basicForm = new QFormLayout;
    basicForm->addRow("Name", nameEdit_);
    basicForm->addRow("Type", typeEdit_);
    basicForm->addRow("Parent Container", parentContainerCombo_);

    // Layer Membership Dual-List
    filterEdit_ = new QLineEdit;
    filterEdit_->setPlaceholderText("Filter available layers...");

    availableLayers_ = new QListWidget;
    currentLayers_ = new QListWidget;

    populateMembershipUI();

    auto* addBtn = new QPushButton(">>");
    auto* rmBtn  = new QPushButton("<<");

    connect(addBtn, &QPushButton::clicked, this, &NodeEditorDialog::onAddLayer);
    connect(rmBtn,  &QPushButton::clicked, this, &NodeEditorDialog::onRemoveLayer);
    connect(availableLayers_, &QListWidget::itemDoubleClicked, this, &NodeEditorDialog::onAddLayer);
    connect(currentLayers_, &QListWidget::itemDoubleClicked, this, &NodeEditorDialog::onRemoveLayer);
    connect(filterEdit_, &QLineEdit::textChanged, this, &NodeEditorDialog::onFilterChanged);

    auto* leftBox = new QVBoxLayout;
    leftBox->addWidget(filterEdit_);
    leftBox->addWidget(availableLayers_);

    auto* mid = new QVBoxLayout;
    mid->addStretch();
    mid->addWidget(addBtn);
    mid->addWidget(rmBtn);
    mid->addStretch();

    auto* listLayout = new QHBoxLayout;
    listLayout->addLayout(leftBox);
    listLayout->addLayout(mid);
    listLayout->addWidget(currentLayers_);

    basicForm->addRow("Layers", listLayout);

    // JSON Tree Editors
    attributesEditor_ = new JsonTreeEditor(this);
    attributesEditor_->setJson(QString::fromStdString(node.attributes));

    metadataEditor_ = new JsonTreeEditor(this);
    metadataEditor_->setJson(QString::fromStdString(node.metadata));

    // Tab Interface
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

    int statusIdx = statusCombo_->findData(static_cast<int>(node.status));
    if (statusIdx >= 0) statusCombo_->setCurrentIndex(statusIdx);
    govForm->addRow("Lifecycle Status:", statusCombo_);

    auto* revRow = new QWidget(this);
    auto* revLayout = new QHBoxLayout(revRow);
    revLayout->setContentsMargins(0, 0, 0, 0);
    reviewerEdit_ = new QLineEdit(QString::fromStdString(node.reviewer), this);
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
    uint32_t currentComputed = (nodeId_ != 0) ? model_->computeNodeChecksum(node) : 0;
    bool isIntegrityValid = (nodeId_ == 0) || (currentComputed == node.checksum);

    checksumValLabel_->setText(QString("0x%1 (%2)")
        .arg(QString::number(node.checksum, 16).toUpper(), QString::number(node.checksum)));
    checksumValLabel_->setStyleSheet("font-family: monospace; font-weight: bold;");

    integrityBadgeLabel_ = new QLabel(this);
    if (isIntegrityValid) {
        integrityBadgeLabel_->setText("✓ Checksum Valid (Tamper-Free)");
        integrityBadgeLabel_->setStyleSheet("color: #a3be8c; font-weight: bold;");
    } else {
        integrityBadgeLabel_->setText(QString("⚠ Checksum Mismatch (Stored: 0x%1, Expected: 0x%2)")
            .arg(QString::number(node.checksum, 16).toUpper(), QString::number(currentComputed, 16).toUpper()));
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

    auto ensureReviewer = [this]() {
        if (reviewerEdit_->text().trimmed().isEmpty()) {
            QString user = QString::fromLocal8Bit(qgetenv("USER"));
            if (user.isEmpty()) user = QString::fromLocal8Bit(qgetenv("USERNAME"));
            if (user.isEmpty()) user = "architect";
            reviewerEdit_->setText(user);
        }
    };

    if (nodeId_ != 0) {
        auto summary = model_->getContainerGovernanceSummary(nodeId_);
        if (summary.totalChildren > 0) {
            auto* childGovBox = new QGroupBox(QString("Child Components Governance (%1 Elements)").arg(summary.totalChildren + summary.totalInternalEdges), this);
            auto* childGovLayout = new QVBoxLayout(childGovBox);

            QString statusColor = summary.hasIssues ? "#ebcb8b" : "#a3be8c";
            QString summaryHtml = QString(
                "<b>Hierarchy Rollup Status:</b> <span style='font-weight:bold;color:%1;'>%2</span><br>"
                "• Child Nodes (%3): %4 approved, %5 changed, %6 new, %7 invalid<br>"
                "• Internal Edges (%8): %9 approved, %10 changed, %11 new")
                .arg(statusColor)
                .arg(QString::fromStdString(to_string(summary.rollupStatus)).toUpper())
                .arg(summary.totalChildren).arg(summary.approvedChildren).arg(summary.changedChildren).arg(summary.newChildren).arg(summary.invalidChildren)
                .arg(summary.totalInternalEdges).arg(summary.approvedEdges).arg(summary.changedEdges).arg(summary.newEdges);

            auto* childSummaryLabel = new QLabel(summaryHtml, this);
            childGovLayout->addWidget(childSummaryLabel);

            cascadeChildrenCheck_ = new QCheckBox(
                QString("Cascade status and reviewer to %1 child node(s) and %2 internal edge(s) on save")
                    .arg(summary.totalChildren).arg(summary.totalInternalEdges), this);
            childGovLayout->addWidget(cascadeChildrenCheck_);

            auto* childBtnLayout = new QHBoxLayout;
            auto* cascadeApproveBtn = new QPushButton(QString("Cascade Approve All (%1)").arg(summary.totalChildren + summary.totalInternalEdges), this);
            auto* cascadeReviewBtn = new QPushButton(QString("Cascade Review All (%1)").arg(summary.totalChildren + summary.totalInternalEdges), this);
            childBtnLayout->addWidget(cascadeApproveBtn);
            childBtnLayout->addWidget(cascadeReviewBtn);
            childGovLayout->addLayout(childBtnLayout);

            connect(cascadeApproveBtn, &QPushButton::clicked, this, [this, ensureReviewer]() {
                ensureReviewer();
                int idx = statusCombo_->findData(static_cast<int>(Status::Approved));
                if (idx >= 0) statusCombo_->setCurrentIndex(idx);
                if (cascadeChildrenCheck_) cascadeChildrenCheck_->setChecked(true);
            });

            connect(cascadeReviewBtn, &QPushButton::clicked, this, [this, ensureReviewer]() {
                ensureReviewer();
                int idx = statusCombo_->findData(static_cast<int>(Status::Reviewed));
                if (idx >= 0) statusCombo_->setCurrentIndex(idx);
                if (cascadeChildrenCheck_) cascadeChildrenCheck_->setChecked(true);
            });

            govLayout->addWidget(childGovBox);
        }
    }

    govLayout->addStretch(1);

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

    connect(buttons, &QDialogButtonBox::accepted, this, &NodeEditorDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(buttons);
}

void NodeEditorDialog::populateParentContainerUI(std::optional<NodeId> currentParentId)
{
    parentContainerCombo_->clear();
    parentContainerCombo_->addItem("None (Root Canvas)", QVariant::fromValue<qulonglong>(0));

    int selectedIdx = 0;

    for (const auto& n : model_->nodes()) {
        if (nodeId_ != 0 && n.id == nodeId_)
            continue;

        QString nameStr = QString::fromStdString(n.name).trimmed();
        QString typeStr = QString::fromStdString(n.type).trimmed();
        
        QString label;
        if (!nameStr.isEmpty()) {
            label = QString("%1 [%2] (#%3)").arg(nameStr, typeStr.isEmpty() ? "Node" : typeStr).arg(n.id);
        } else {
            label = QString("Node #%1 [%2]").arg(n.id).arg(typeStr.isEmpty() ? "Node" : typeStr);
        }

        parentContainerCombo_->addItem(label, QVariant::fromValue<qulonglong>(n.id));

        if (currentParentId && *currentParentId == n.id) {
            selectedIdx = parentContainerCombo_->count() - 1;
        }
    }

    parentContainerCombo_->setCurrentIndex(selectedIdx);
}

void NodeEditorDialog::populateMembershipUI()
{
    availableLayers_->clear();
    currentLayers_->clear();

    const QString filterText = filterEdit_ ? filterEdit_->text().trimmed().toLower() : QString();

    for (const auto& l : model_->layers()) {
        auto* item = new QListWidgetItem(QString::fromStdString(l.name));
        item->setData(Qt::UserRole, static_cast<qulonglong>(l.id));

        if (stagedLayers_.count(l.id)) {
            currentLayers_->addItem(item);
        } else {
            if (filterText.isEmpty() || item->text().toLower().contains(filterText)) {
                availableLayers_->addItem(item);
            }
        }
    }
}

void NodeEditorDialog::onFilterChanged(const QString&)
{
    populateMembershipUI();
}

void NodeEditorDialog::onAddLayer()
{
    auto* item = availableLayers_->currentItem();
    if (!item) return;

    LayerId id = item->data(Qt::UserRole).toULongLong();
    stagedLayers_.insert(id);
    populateMembershipUI();
}

void NodeEditorDialog::onRemoveLayer()
{
    auto* item = currentLayers_->currentItem();
    if (!item) return;

    LayerId id = item->data(Qt::UserRole).toULongLong();
    stagedLayers_.erase(id);
    populateMembershipUI();
}

void NodeEditorDialog::onSave()
{
    NodeData node;
    node.id = nodeId_;
    node.name = nameEdit_->text().toStdString();
    node.type = typeEdit_->text().toStdString();

    // Export serialized JSON directly from visual tree editors
    node.attributes = attributesEditor_->toJsonString(QJsonDocument::Compact).toStdString();
    node.metadata   = metadataEditor_->toJsonString(QJsonDocument::Compact).toStdString();

    // Extract selected parent container
    qulonglong selId = parentContainerCombo_->currentData().toULongLong();
    if (selId == 0) {
        node.parentId = std::nullopt;
    } else {
        node.parentId = static_cast<NodeId>(selId);
    }

    Result r;
    if (!nodeId_) {
        NodeId newId = 0;
        r = model_->addNode(node, newId);
        nodeId_ = newId;
    } else {
        r = model_->updateNode(node);
    }

    if (!r.ok) {
        QMessageBox::critical(this, "Error", QString::fromStdString(r.message));
        return;
    }

    // Apply governance settings
    if (statusCombo_) {
        auto chosenStatus = static_cast<Status>(statusCombo_->currentData().toInt());
        std::string chosenReviewer = reviewerEdit_ ? reviewerEdit_->text().trimmed().toStdString() : "";
        if (cascadeChildrenCheck_ && cascadeChildrenCheck_->isChecked()) {
            model_->cascadeNodeGovernance(nodeId_, chosenStatus, chosenReviewer);
        } else {
            model_->setNodeGovernance(nodeId_, chosenStatus, chosenReviewer);
        }
    }

    // Commit staged layer memberships
    for (LayerId lid : stagedLayers_) {
        if (!originalLayers_.count(lid)) {
            model_->addNodeToLayer(nodeId_, lid);
        }
    }
    for (LayerId lid : originalLayers_) {
        if (!stagedLayers_.count(lid)) {
            model_->removeNodeFromLayer(nodeId_, lid);
        }
    }

    accept();
}