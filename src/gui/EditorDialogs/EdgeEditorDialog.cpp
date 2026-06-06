#include "EdgeEditorDialog.h"
#include "ArchitectureModel.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>

EdgeEditorDialog::EdgeEditorDialog(
    ArchitectureModel* model,
    unsigned long long edgeId,
    QWidget* parent)
    : QDialog(parent),
      model_(model),
      edgeId_(edgeId)
{
    setWindowTitle("Edit Edge");

    auto* layout = new QVBoxLayout(this);

    typeEdit_ = new QLineEdit(this);
    metadataEdit_ = new QTextEdit(this);

    auto* saveBtn = new QPushButton("Save", this);

    layout->addWidget(typeEdit_);
    layout->addWidget(metadataEdit_);
    layout->addWidget(saveBtn);

    connect(saveBtn, &QPushButton::clicked,
            this, &EdgeEditorDialog::saveEdge);

    loadEdge();
}

void EdgeEditorDialog::loadEdge()
{
    for (auto& e : model_->edges()) {
        if (e.id == edgeId_) {
            typeEdit_->setText(
                QString::fromStdString(e.edgeType));
            metadataEdit_->setText(
                QString::fromStdString(e.metadata));
            break;
        }
    }
}

void EdgeEditorDialog::saveEdge()
{
    for (const auto& edge : model_->edges())
    {
        if (edge.id == edgeId_)
        {
            //
            // create editable copy
            //
            EdgeData e = edge;

            e.edgeType =
                typeEdit_->text().toStdString();

            e.metadata =
                metadataEdit_->toPlainText().toStdString();

            model_->updateEdge(e);

            break;
        }
    }

    accept();
}