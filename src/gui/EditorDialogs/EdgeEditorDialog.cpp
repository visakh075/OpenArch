#include "EdgeEditorDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QTabWidget>

EdgeEditorDialog::EdgeEditorDialog(ArchitectureModel* model,
                                   EdgeId edgeId,
                                   QWidget* parent)
    : QDialog(parent),
      model_(model),
      edgeId_(edgeId)
{
    setWindowTitle(edgeId_ == 0 ? "New Edge" : "Edit Edge");
    resize(520, 480);

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

    accept();
}