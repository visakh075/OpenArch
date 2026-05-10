#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QListWidget>

#include "ArchitectureModel.h"

class LayerEditorDialog : public QDialog {
    Q_OBJECT
public:
    LayerEditorDialog(ArchitectureModel* model,
                      LayerId layerId,
                      QWidget* parent = nullptr);

private slots:
    void onAddNode();
    void onRemoveNode();
    void onSave();

private:
    void loadMembership();

    ArchitectureModel* model_;
    LayerId layerId_;

    QLineEdit* nameEdit_;
    QLineEdit* kindEdit_;
    QPlainTextEdit* metadataEdit_;
    QPlainTextEdit* attributesEdit_;

    QListWidget* availableNodes_;
    QListWidget* currentNodes_;
};