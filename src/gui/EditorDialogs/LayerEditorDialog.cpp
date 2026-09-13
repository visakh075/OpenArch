#include "LayerEditorDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QTabWidget>

LayerEditorDialog::LayerEditorDialog(ArchitectureModel* model,
                                     LayerId layerId,
                                     QWidget* parent)
    : QDialog(parent),
      model_(model),
      layerId_(layerId)
{
    setWindowTitle(layerId_ == 0 ? "New Layer" : "Edit Layer");
    resize(580, 600);

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
    basicForm->addRow("Name", nameEdit_);
    basicForm->addRow("Kind", kindEdit_);

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