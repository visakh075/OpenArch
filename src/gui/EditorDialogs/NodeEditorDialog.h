#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QTabWidget>
#include <unordered_set>
#include <optional>

#include "JsonTreeEditor.h"
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
    void populateParentContainerUI(std::optional<NodeId> currentParentId);

    ArchitectureModel* model_;
    NodeId nodeId_;
    std::optional<NodeId> initialParentId_{std::nullopt};

    QLineEdit* nameEdit_{nullptr};
    QLineEdit* typeEdit_{nullptr};
    QComboBox* parentContainerCombo_{nullptr};

    JsonTreeEditor* metadataEditor_{nullptr};
    JsonTreeEditor* attributesEditor_{nullptr};

    QLineEdit* filterEdit_{nullptr};
    QListWidget* availableLayers_{nullptr};
    QListWidget* currentLayers_{nullptr};

    std::unordered_set<LayerId> stagedLayers_;
    std::unordered_set<LayerId> originalLayers_;
};