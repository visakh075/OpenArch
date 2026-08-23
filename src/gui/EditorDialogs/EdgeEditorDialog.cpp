#include "EdgeEditorDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QMessageBox>

EdgeEditorDialog::EdgeEditorDialog(ArchitectureModel* model,
                                   EdgeId edgeId,
                                   QWidget* parent)
    : QDialog(parent),
      model_(model),
      edgeId_(edgeId)
{
    setWindowTitle(edgeId_ == 0 ? "New Edge" : "Edit Edge");

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

    typeEdit_ = new QLineEdit(QString::fromStdString(edge.edgeType));
    metadataEdit_ = new QPlainTextEdit(QString::fromStdString(edge.metadata));
    attributesEdit_ = new QPlainTextEdit(QString::fromStdString(edge.attributes));

    auto* form = new QFormLayout;
    form->addRow("Type", typeEdit_);
    form->addRow("Metadata", metadataEdit_);
    form->addRow("Attributes", attributesEdit_);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel);

    connect(buttons, &QDialogButtonBox::accepted, this, &EdgeEditorDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* main = new QVBoxLayout(this);
    main->addLayout(form);
    main->addWidget(buttons);
}

void EdgeEditorDialog::onSave()
{
    auto opt = model_->getEdgeById(edgeId_);
    if (!opt) {
        QMessageBox::critical(this, "Error", "Edge not found");
        return;
    }

    EdgeData edge = *opt;
    edge.edgeType = typeEdit_->text().toStdString();
    edge.metadata = metadataEdit_->toPlainText().toStdString();
    edge.attributes = attributesEdit_->toPlainText().toStdString();

    Result r = model_->updateEdge(edge);
    if (!r.ok) {
        QMessageBox::critical(this, "Error", QString::fromStdString(r.message));
        return;
    }

    accept();
}