#include "LayerEditorWidget.h"
#include "RelationEditorWidget.h"

#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>

LayerEditorWidget::LayerEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    nameEdit_ = new QLineEdit(this);
    kindEdit_ = new QLineEdit(this);
    metadataEdit_ = new QTextEdit(this);
    attributesEdit_ = new QTextEdit(this);
    saveBtn_ = new QPushButton("Save Layer", this);

    nodeEditor_ = new RelationEditorWidget(this);

    auto* form = new QFormLayout;
    form->addRow("Name", nameEdit_);
    form->addRow("Kind", kindEdit_);
    form->addRow("Metadata", metadataEdit_);
    form->addRow("Attributes", attributesEdit_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(nodeEditor_);
    layout->addWidget(saveBtn_);
    setLayout(layout);

    /* -------- Save Layer -------- */
    connect(saveBtn_, &QPushButton::clicked, this, [this]() {
        current_.name       = nameEdit_->text().toStdString();
        current_.kind       = kindEdit_->text().toStdString();
        current_.metadata   = metadataEdit_->toPlainText().toStdString();
        current_.attributes = attributesEdit_->toPlainText().toStdString();

        emit saveLayerRequested(current_);
    });

    /* -------- Node ↔ Layer relations -------- */
    connect(nodeEditor_, &RelationEditorWidget::addRequested,
            this, &LayerEditorWidget::addNodeRequested);

    connect(nodeEditor_, &RelationEditorWidget::removeRequested,
            this, &LayerEditorWidget::removeNodeRequested);
}

/* ============================================================
   Single entry point to load a layer (Option A)
   ============================================================ */
void LayerEditorWidget::setLayer(const LayerData& layer,
                                 const QStringList& allNodes,
                                 const QStringList& currentNodes)
{
    current_ = layer;

    nameEdit_->setText(QString::fromStdString(layer.name));
    kindEdit_->setText(QString::fromStdString(layer.kind));
    metadataEdit_->setPlainText(QString::fromStdString(layer.metadata));
    attributesEdit_->setPlainText(QString::fromStdString(layer.attributes));

    nodeEditor_->setAllItems(allNodes);
    nodeEditor_->setCurrentItems(currentNodes);

    show();
}
