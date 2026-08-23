#include "LayerEditorDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>

LayerEditorDialog::LayerEditorDialog(ArchitectureModel* model,
                                     LayerId layerId,
                                     QWidget* parent)
    : QDialog(parent),
      model_(model),
      layerId_(layerId)
{
    setWindowTitle(layerId_ == 0 ? "New Layer" : "Edit Layer");

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

    nameEdit_ = new QLineEdit(QString::fromStdString(layer.name));
    kindEdit_ = new QLineEdit(QString::fromStdString(layer.kind));
    metadataEdit_ = new QPlainTextEdit(QString::fromStdString(layer.metadata));
    attributesEdit_ = new QPlainTextEdit(QString::fromStdString(layer.attributes));

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

    auto* form = new QFormLayout;
    form->addRow("Name", nameEdit_);
    form->addRow("Kind", kindEdit_);
    form->addRow("Metadata", metadataEdit_);
    form->addRow("Attributes", attributesEdit_);
    form->addRow("Nodes", listLayout);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel);

    connect(buttons, &QDialogButtonBox::accepted, this, &LayerEditorDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* main = new QVBoxLayout(this);
    main->addLayout(form);
    main->addWidget(buttons);
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
    layer.metadata = metadataEdit_->toPlainText().toStdString();
    layer.attributes = attributesEdit_->toPlainText().toStdString();

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