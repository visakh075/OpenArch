#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QListWidget>

#include "ArchitectureModel.h"

class NodeEditorDialog : public QDialog {
    Q_OBJECT
public:
    NodeEditorDialog(ArchitectureModel* model,
                     NodeId nodeId,
                     QWidget* parent = nullptr);

private slots:
    void onAddLayer();
    void onRemoveLayer();
    void onSave();

private:
    void loadMembership();

    ArchitectureModel* model_;
    NodeId nodeId_;

    QLineEdit* nameEdit_;
    QLineEdit* typeEdit_;
    QPlainTextEdit* metadataEdit_;
    QPlainTextEdit* attributesEdit_;

    QListWidget* availableLayers_;
    QListWidget* currentLayers_;
};