#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>

#include "ArchitectureModel.h"

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
    ArchitectureModel* model_;
    EdgeId edgeId_;

    QLineEdit* typeEdit_;
    QPlainTextEdit* metadataEdit_;
    QPlainTextEdit* attributesEdit_;
};