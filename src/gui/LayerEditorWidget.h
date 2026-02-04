#pragma once
#include <QWidget>
#include "core/Types.h"

class QLineEdit;
class QTextEdit;
class QPushButton;
class RelationEditorWidget;

class LayerEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit LayerEditorWidget(QWidget* parent = nullptr);

    void setLayer(const LayerData& layer,
                  const QStringList& allNodes,
                  const QStringList& currentNodes);

signals:
    void saveLayerRequested(const LayerData& layer);
    void addNodeRequested(const QString& nodeName);
    void removeNodeRequested(const QString& nodeName);

private:
    LayerData current_;

    QLineEdit* nameEdit_;
    QLineEdit* kindEdit_;
    QTextEdit* metadataEdit_;
    QTextEdit* attributesEdit_;
    QPushButton* saveBtn_;

    RelationEditorWidget* nodeEditor_;
};
