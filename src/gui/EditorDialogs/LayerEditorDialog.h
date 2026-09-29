#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <unordered_set>

class QLabel;

#include "JsonTreeEditor.h"
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

    ArchitectureModel* model_{nullptr};
    LayerId layerId_{0};

    QLineEdit* nameEdit_{nullptr};
    QLineEdit* kindEdit_{nullptr};

    JsonTreeEditor* metadataEditor_{nullptr};
    JsonTreeEditor* attributesEditor_{nullptr};

    QLineEdit* filterEdit_{nullptr};
    QListWidget* availableNodes_{nullptr};
    QListWidget* currentNodes_{nullptr};

    std::unordered_set<NodeId> stagedNodes_;
    std::unordered_set<NodeId> originalNodes_;

    // Governance
    QComboBox* statusCombo_{nullptr};
    QLineEdit* reviewerEdit_{nullptr};
    QLabel* checksumValLabel_{nullptr};
    QLabel* integrityBadgeLabel_{nullptr};
};