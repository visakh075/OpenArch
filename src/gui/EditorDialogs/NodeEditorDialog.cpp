#include "NodeEditorDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <unordered_set>
NodeEditorDialog::NodeEditorDialog(ArchitectureModel* model,
                                   NodeId nodeId,
                                   QWidget* parent)
    : QDialog(parent),
      model_(model),
      nodeId_(nodeId)
{
    setWindowTitle(nodeId_ == 0 ? "New Node" : "Edit Node");

    NodeData node{};
    if (nodeId_ != 0) {
        auto opt = model_->getNodeById(nodeId_);
        if (!opt) {
            QMessageBox::critical(this, "Error", "Node not found");
            reject();
            return;
        }
        node = *opt;
    }

    nameEdit_ = new QLineEdit(QString::fromStdString(node.name));
    typeEdit_ = new QLineEdit(QString::fromStdString(node.type));
    metadataEdit_ = new QPlainTextEdit(QString::fromStdString(node.metadata));
    attributesEdit_ = new QPlainTextEdit(QString::fromStdString(node.attributes));

    availableLayers_ = new QListWidget;
    currentLayers_ = new QListWidget;

    loadMembership();

    auto* addBtn = new QPushButton(">>");
    auto* rmBtn  = new QPushButton("<<");

    connect(addBtn, &QPushButton::clicked, this, &NodeEditorDialog::onAddLayer);
    connect(rmBtn,  &QPushButton::clicked, this, &NodeEditorDialog::onRemoveLayer);

    auto* listLayout = new QHBoxLayout;
    listLayout->addWidget(availableLayers_);

    auto* mid = new QVBoxLayout;
    mid->addWidget(addBtn);
    mid->addWidget(rmBtn);
    mid->addStretch();

    listLayout->addLayout(mid);
    listLayout->addWidget(currentLayers_);

    auto* form = new QFormLayout;
    form->addRow("Name", nameEdit_);
    form->addRow("Type", typeEdit_);
    form->addRow("Metadata", metadataEdit_);
    form->addRow("Attributes", attributesEdit_);
    form->addRow("Layers", listLayout);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel);

    connect(buttons, &QDialogButtonBox::accepted,
            this, &NodeEditorDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    auto* main = new QVBoxLayout(this);
    main->addLayout(form);
    main->addWidget(buttons);
}

void NodeEditorDialog::loadMembership()
{
    availableLayers_->clear();
    currentLayers_->clear();

    std::unordered_set<LayerId> memberLayerIds;

    if (nodeId_ != 0) {
        for (const auto& nl : model_->layersForNode(nodeId_)) {
            memberLayerIds.insert(nl.layerId);
        }
    }

    for (const auto& l : model_->layers()) {
        auto* item = new QListWidgetItem(
            QString::fromStdString(l.name));
        item->setData(Qt::UserRole,
                      static_cast<qulonglong>(l.id));

        if (memberLayerIds.count(l.id)) {
            currentLayers_->addItem(item);
        } else {
            availableLayers_->addItem(item);
        }
    }
}


void NodeEditorDialog::onAddLayer()
{
    if (!nodeId_) return;
    auto* item = availableLayers_->currentItem();
    if (!item) return;

    LayerId id = item->data(Qt::UserRole).toULongLong();
    model_->addNodeToLayer(nodeId_, id);
    loadMembership();
}

void NodeEditorDialog::onRemoveLayer()
{
    if (!nodeId_) return;
    auto* item = currentLayers_->currentItem();
    if (!item) return;

    LayerId id = item->data(Qt::UserRole).toULongLong();
    model_->removeNodeFromLayer(nodeId_, id);
    loadMembership();
}

void NodeEditorDialog::onSave()
{
    NodeData node;
    node.id = nodeId_;
    node.name = nameEdit_->text().toStdString();
    node.type = typeEdit_->text().toStdString();
    node.metadata = metadataEdit_->toPlainText().toStdString();
    node.attributes = attributesEdit_->toPlainText().toStdString();

    Result r;
    if (!nodeId_) {
        NodeId newId = 0;
        r = model_->addNode(node, newId);
        nodeId_ = newId;
    } else {
        r = model_->updateNode(node);
    }

    if (!r.ok) {
        QMessageBox::critical(this, "Error",
                              QString::fromStdString(r.message));
        return;
    }
    accept();
}