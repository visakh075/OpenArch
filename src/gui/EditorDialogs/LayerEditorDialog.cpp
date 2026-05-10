#include "LayerEditorDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <unordered_set>
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
    }

    nameEdit_ = new QLineEdit(QString::fromStdString(layer.name));
    kindEdit_ = new QLineEdit(QString::fromStdString(layer.kind));
    metadataEdit_ = new QPlainTextEdit(QString::fromStdString(layer.metadata));
    attributesEdit_ = new QPlainTextEdit(QString::fromStdString(layer.attributes));

    availableNodes_ = new QListWidget;
    currentNodes_   = new QListWidget;

    loadMembership();

    auto* addBtn = new QPushButton(">>");
    auto* rmBtn  = new QPushButton("<<");

    connect(addBtn, &QPushButton::clicked, this, &LayerEditorDialog::onAddNode);
    connect(rmBtn,  &QPushButton::clicked, this, &LayerEditorDialog::onRemoveNode);

    auto* listLayout = new QHBoxLayout;
    listLayout->addWidget(availableNodes_);

    auto* mid = new QVBoxLayout;
    mid->addWidget(addBtn);
    mid->addWidget(rmBtn);
    mid->addStretch();

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

    connect(buttons, &QDialogButtonBox::accepted,
            this, &LayerEditorDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    auto* main = new QVBoxLayout(this);
    main->addLayout(form);
    main->addWidget(buttons);
}

void LayerEditorDialog::loadMembership()
{
    availableNodes_->clear();
    currentNodes_->clear();

    std::unordered_set<NodeId> memberIds;

    if (layerId_ != 0) {
        for (const auto& nl : model_->nodesInLayer(layerId_)) {
            memberIds.insert(nl.nodeId);
        }
    }

    for (const auto& n : model_->nodes()) {
        auto* item = new QListWidgetItem(
            QString::fromStdString(n.name));
        item->setData(Qt::UserRole,
                      static_cast<qulonglong>(n.id));

        if (memberIds.count(n.id)) {
            currentNodes_->addItem(item);
        } else {
            availableNodes_->addItem(item);
        }
    }
}


void LayerEditorDialog::onAddNode()
{
    if (!layerId_) return;
    auto* item = availableNodes_->currentItem();
    if (!item) return;

    NodeId id = item->data(Qt::UserRole).toULongLong();
    model_->addNodeToLayer(id, layerId_);
    loadMembership();
}

void LayerEditorDialog::onRemoveNode()
{
    if (!layerId_) return;
    auto* item = currentNodes_->currentItem();
    if (!item) return;

    NodeId id = item->data(Qt::UserRole).toULongLong();
    model_->removeNodeFromLayer(id, layerId_);
    loadMembership();
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
        QMessageBox::critical(this, "Error",
                              QString::fromStdString(r.message));
        return;
    }
    accept();
}