#pragma once
#include <QWidget>
#include "core/Types.h"

class QLineEdit;
class QTextEdit;
class QPushButton;
class RelationEditorWidget;

class NodeEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit NodeEditorWidget(QWidget* parent = nullptr);

    void setNode(const NodeData& node,
                 const QStringList& allLayers,
                 const QStringList& currentLayers);

signals:
    void saveNodeRequested(const NodeData& node);
    void addLayerRequested(const QString& layerName);
    void removeLayerRequested(const QString& layerName);

private:
    NodeData current_;

    QLineEdit* nameEdit_;
    QLineEdit* typeEdit_;
    QTextEdit* metadataEdit_;
    QTextEdit* attributesEdit_;
    QPushButton* saveBtn_;

    RelationEditorWidget* layerEditor_;
};
