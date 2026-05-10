#pragma once

#include <QDialog>

class QLineEdit;
class QTextEdit;
class ArchitectureModel;

class EdgeEditorDialog : public QDialog
{
    Q_OBJECT

public:
    EdgeEditorDialog(ArchitectureModel* model,
                     unsigned long long edgeId,
                     QWidget* parent = nullptr);

private:
    void loadEdge();
    void saveEdge();

    ArchitectureModel* model_;
    unsigned long long edgeId_;

    QLineEdit* typeEdit_;
    QTextEdit* metadataEdit_;
};
