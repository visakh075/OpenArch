#include "EdgeEditorDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QTabWidget>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>

EdgeEditorDialog::EdgeEditorDialog(ArchitectureModel* model,
                                   EdgeId edgeId,
                                   QWidget* parent)
    : QDialog(parent),
      model_(model),
      edgeId_(edgeId)
{
    setWindowTitle(edgeId_ == 0 ? "New Edge" : "Edit Edge");
    resize(540, 520);

    EdgeData edge{};
    if (edgeId_ != 0) {
        auto opt = model_->getEdgeById(edgeId_);
        if (!opt) {
            QMessageBox::critical(this, "Error", "Edge not found");
            reject();
            return;
        }
        edge = *opt;
    }

    // General Type Form
    typeEdit_ = new QLineEdit(QString::fromStdString(edge.edgeType));
    auto* form = new QFormLayout;
    form->addRow("Connection / Protocol Type", typeEdit_);

    auto* generalTab = new QWidget(this);
    generalTab->setLayout(form);

    // JSON Tree Editors
    attributesEditor_ = new JsonTreeEditor(this);
    attributesEditor_->setJson(QString::fromStdString(edge.attributes));

    metadataEditor_ = new JsonTreeEditor(this);
    metadataEditor_->setJson(QString::fromStdString(edge.metadata));

    // Tab Widget
    auto* tabWidget = new QTabWidget(this);
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

    int statusIdx = statusCombo_->findData(static_cast<int>(edge.status));
    if (statusIdx >= 0) statusCombo_->setCurrentIndex(statusIdx);
    govForm->addRow("Lifecycle Status:", statusCombo_);

    auto* revRow = new QWidget(this);
    auto* revLayout = new QHBoxLayout(revRow);
    revLayout->setContentsMargins(0, 0, 0, 0);
    reviewerEdit_ = new QLineEdit(QString::fromStdString(edge.reviewer), this);
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
    uint32_t currentComputed = (edgeId_ != 0) ? model_->computeEdgeChecksum(edge) : 0;
    bool isIntegrityValid = (edgeId_ == 0) || (currentComputed == edge.checksum);

    checksumValLabel_->setText(QString("0x%1 (%2)")
        .arg(QString::number(edge.checksum, 16).toUpper(), QString::number(edge.checksum)));
    checksumValLabel_->setStyleSheet("font-family: monospace; font-weight: bold;");

    integrityBadgeLabel_ = new QLabel(this);
    if (isIntegrityValid) {
        integrityBadgeLabel_->setText("✓ Checksum Valid (Tamper-Free)");
        integrityBadgeLabel_->setStyleSheet("color: #a3be8c; font-weight: bold;");
    } else {
        integrityBadgeLabel_->setText(QString("⚠ Checksum Mismatch (Stored: 0x%1, Expected: 0x%2)")
            .arg(QString::number(edge.checksum, 16).toUpper(), QString::number(currentComputed, 16).toUpper()));
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

    connect(buttons, &QDialogButtonBox::accepted, this, &EdgeEditorDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(buttons);
}

void EdgeEditorDialog::onSave()
{
    auto opt = model_->getEdgeById(edgeId_);
    if (!opt) {
        QMessageBox::critical(this, "Error", "Edge not found");
        return;
    }

    EdgeData edge = *opt;
    edge.edgeType   = typeEdit_->text().toStdString();
    edge.attributes = attributesEditor_->toJsonString(QJsonDocument::Compact).toStdString();
    edge.metadata   = metadataEditor_->toJsonString(QJsonDocument::Compact).toStdString();

    Result r = model_->updateEdge(edge);
    if (!r.ok) {
        QMessageBox::critical(this, "Error", QString::fromStdString(r.message));
        return;
    }

    // Apply governance settings
    if (statusCombo_) {
        auto chosenStatus = static_cast<Status>(statusCombo_->currentData().toInt());
        std::string chosenReviewer = reviewerEdit_ ? reviewerEdit_->text().trimmed().toStdString() : "";
        model_->setEdgeGovernance(edgeId_, chosenStatus, chosenReviewer);
    }

    accept();
}