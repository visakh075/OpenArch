#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QTabWidget>

#include "JsonTreeEditor.h"
#include "ArchitectureModel.h"

class QComboBox;
class QLabel;

class EdgeEditorDialog : public QDialog
{
    Q_OBJECT

public:
    EdgeEditorDialog(ArchitectureModel* model,
                     EdgeId edgeId,
                     QWidget* parent = nullptr);

private slots:
    void onSave();

private:
    ArchitectureModel* model_{nullptr};
    EdgeId edgeId_{0};

    QLineEdit* typeEdit_{nullptr};
    QComboBox* routingCombo_{nullptr};
    JsonTreeEditor* metadataEditor_{nullptr};
    JsonTreeEditor* attributesEditor_{nullptr};

    // Governance
    QComboBox* statusCombo_{nullptr};
    QLineEdit* reviewerEdit_{nullptr};
    QLabel* checksumValLabel_{nullptr};
    QLabel* integrityBadgeLabel_{nullptr};
};