#include "NodeEditorWidget.h"
#include "RelationEditorWidget.h"

#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>

NodeEditorWidget::NodeEditorWidget(QWidget* parent)
    : QWidget(parent) {

    nameEdit_ = new QLineEdit;
    typeEdit_ = new QLineEdit;
    metadataEdit_ = new QTextEdit;
    attributesEdit_ = new QTextEdit;
    saveBtn_ = new QPushButton("Save Node");

    layerEditor_ = new RelationEditorWidget;

    auto* form = new QFormLayout;
    form->addRow("Name", nameEdit_);
    form->addRow("Type", typeEdit_);
    form->addRow("Metadata", metadataEdit_);
    form->addRow("Attributes", attributesEdit_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(layerEditor_);
    layout->addWidget(saveBtn_);

    connect(saveBtn_, &QPushButton::clicked, this, [this]() {
        current_.name = nameEdit_->text().toStdString();
        current_.type = typeEdit_->text().toStdString();
        current_.metadata = metadataEdit_->toPlainText().toStdString();
        current_.attributes = attributesEdit_->toPlainText().toStdString();
        emit saveNodeRequested(current_);
    });
}

void NodeEditorWidget::setNode(const NodeData& node,
                               const QStringList& allLayers,
                               const QStringList& currentLayers) {
    current_ = node;

    nameEdit_->setText(QString::fromStdString(node.name));
    typeEdit_->setText(QString::fromStdString(node.type));
    metadataEdit_->setPlainText(QString::fromStdString(node.metadata));
    attributesEdit_->setPlainText(QString::fromStdString(node.attributes));

    layerEditor_->setAllItems(allLayers);
    layerEditor_->setCurrentItems(currentLayers);
}