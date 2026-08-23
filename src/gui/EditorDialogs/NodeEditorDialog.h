#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QListWidget>
#include <unordered_set>

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
    void onFilterChanged(const QString& text);
    void onSave();

private:
    void populateMembershipUI();

    ArchitectureModel* model_;
    NodeId nodeId_;

    QLineEdit* nameEdit_;
    QLineEdit* typeEdit_;
    QPlainTextEdit* metadataEdit_;
    QPlainTextEdit* attributesEdit_;

    QLineEdit* filterEdit_;
    QListWidget* availableLayers_;
    QListWidget* currentLayers_;

    std::unordered_set<LayerId> stagedLayers_;
    std::unordered_set<LayerId> originalLayers_;
};