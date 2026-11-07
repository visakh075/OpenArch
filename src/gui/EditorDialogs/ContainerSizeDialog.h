#pragma once

#include <QDialog>
#include <optional>
#include "core/Types.h"

class ArchitectureModel;
class QRadioButton;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;

class ContainerSizeDialog : public QDialog
{
    Q_OBJECT

public:
    ContainerSizeDialog(ArchitectureModel* model, NodeId containerId, QWidget* parent = nullptr);

    bool isDimLocked() const;
    std::optional<qreal> customWidth() const;
    std::optional<qreal> customHeight() const;
    qreal padding() const;

private slots:
    void onModeToggled();
    void onFitToChildren();
    void onResetDefaults();
    void onAccept();

private:
    void setupUi();
    void loadCurrentValues();

    ArchitectureModel* model_{nullptr};
    NodeId containerId_{0};

    QLabel* titleLabel_{nullptr};
    QRadioButton* radioAutoFit_{nullptr};
    QRadioButton* radioLocked_{nullptr};

    QCheckBox* checkEnableMinDim_{nullptr};
    QDoubleSpinBox* spinWidth_{nullptr};
    QDoubleSpinBox* spinHeight_{nullptr};
    QDoubleSpinBox* spinPadding_{nullptr};

    QPushButton* btnFitChildren_{nullptr};
    QPushButton* btnReset_{nullptr};
};
