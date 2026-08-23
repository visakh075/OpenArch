#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QListWidget>
#include <unordered_set>

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
    void onFilterChanged(const QString& text);
    void onSave();

private:
    void populateMembershipUI();

    ArchitectureModel* model_;
    LayerId layerId_;

    QLineEdit* nameEdit_;
    QLineEdit* kindEdit_;
    QPlainTextEdit* metadataEdit_;
    QPlainTextEdit* attributesEdit_;

    QLineEdit* filterEdit_;
    QListWidget* availableNodes_;
    QListWidget* currentNodes_;

    std::unordered_set<NodeId> stagedNodes_;
    std::unordered_set<NodeId> originalNodes_;
};