#include "ContainerSizeDialog.h"
#include "core/ArchitectureModel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

ContainerSizeDialog::ContainerSizeDialog(ArchitectureModel* model, NodeId containerId, QWidget* parent)
    : QDialog(parent),
      model_(model),
      containerId_(containerId)
{
    setupUi();
    loadCurrentValues();
}

void ContainerSizeDialog::setupUi()
{
    setWindowTitle("Manage Container Size & Layout");
    setMinimumWidth(440);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto n = model_ ? model_->getNodeById(containerId_) : std::nullopt;
    QString name = n ? QString::fromStdString(n->name) : QString("Container #%1").arg(containerId_);

    titleLabel_ = new QLabel(QString("<b>Configure Dimensions for:</b> %1").arg(name), this);
    mainLayout->addWidget(titleLabel_);

    // Sizing Mode Group
    auto* modeGroup = new QGroupBox("Sizing Behavior", this);
    auto* modeLayout = new QVBoxLayout(modeGroup);

    radioAutoFit_ = new QRadioButton("Auto-Fit to Children (Dynamic)", modeGroup);
    radioAutoFit_->setToolTip("Container automatically expands to enclose all children with padding");
    radioLocked_ = new QRadioButton("Fixed / Locked Dimensions (Absolute)", modeGroup);
    radioLocked_->setToolTip("Container retains fixed absolute width & height without expanding");

    modeLayout->addWidget(radioAutoFit_);
    modeLayout->addWidget(radioLocked_);
    mainLayout->addWidget(modeGroup);

    // Dimension Settings Group
    auto* dimGroup = new QGroupBox("Dimensions & Padding", this);
    auto* dimForm = new QFormLayout(dimGroup);
    dimForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    checkEnableMinDim_ = new QCheckBox("Enforce Minimum Dimensions", dimGroup);
    checkEnableMinDim_->setChecked(true);
    dimForm->addRow(checkEnableMinDim_);

    spinWidth_ = new QDoubleSpinBox(dimGroup);
    spinWidth_->setRange(100.0, 10000.0);
    spinWidth_->setSingleStep(20.0);
    spinWidth_->setSuffix(" px");
    dimForm->addRow("Width:", spinWidth_);

    spinHeight_ = new QDoubleSpinBox(dimGroup);
    spinHeight_->setRange(60.0, 10000.0);
    spinHeight_->setSingleStep(20.0);
    spinHeight_->setSuffix(" px");
    dimForm->addRow("Height:", spinHeight_);

    spinPadding_ = new QDoubleSpinBox(dimGroup);
    spinPadding_->setRange(5.0, 150.0);
    spinPadding_->setSingleStep(5.0);
    spinPadding_->setValue(20.0);
    spinPadding_->setSuffix(" px");
    dimForm->addRow("Internal Padding:", spinPadding_);

    mainLayout->addWidget(dimGroup);

    // Helper Buttons Row
    auto* btnRow = new QHBoxLayout;
    btnFitChildren_ = new QPushButton("⤢ Fit to Current Children", this);
    btnFitChildren_->setToolTip("Automatically calculates bounds to snugly wrap all child nodes");
    btnReset_ = new QPushButton("↺ Reset to Default Auto-Fit", this);

    btnRow->addWidget(btnFitChildren_);
    btnRow->addWidget(btnReset_);
    mainLayout->addLayout(btnRow);

    // Dialog Button Box
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(radioAutoFit_, &QRadioButton::toggled, this, &ContainerSizeDialog::onModeToggled);
    connect(radioLocked_, &QRadioButton::toggled, this, &ContainerSizeDialog::onModeToggled);
    connect(checkEnableMinDim_, &QCheckBox::toggled, this, &ContainerSizeDialog::onModeToggled);
    connect(btnFitChildren_, &QPushButton::clicked, this, &ContainerSizeDialog::onFitToChildren);
    connect(btnReset_, &QPushButton::clicked, this, &ContainerSizeDialog::onResetDefaults);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ContainerSizeDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ContainerSizeDialog::loadCurrentValues()
{
    if (!model_) return;

    auto n = model_->getNodeById(containerId_);
    if (!n) return;

    bool dimLock = false;
    std::optional<qreal> curW;
    std::optional<qreal> curH;
    qreal pad = 20.0;

    if (!n->metadata.empty())
    {
        QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(n->metadata).toUtf8());
        if (doc.isObject())
        {
            QJsonObject obj = doc.object();
            if (obj.contains("_dim_lock"))
            {
                dimLock = obj["_dim_lock"].toBool(false);
            }
            if (obj.contains("_width"))
            {
                curW = obj["_width"].toDouble();
            }
            if (obj.contains("_height"))
            {
                curH = obj["_height"].toDouble();
            }
            if (obj.contains("_padding"))
            {
                pad = obj["_padding"].toDouble(20.0);
            }
        }
    }

    if (dimLock)
    {
        radioLocked_->setChecked(true);
        checkEnableMinDim_->setChecked(true);
        checkEnableMinDim_->setEnabled(false);
    }
    else
    {
        radioAutoFit_->setChecked(true);
        checkEnableMinDim_->setChecked(curW.has_value() || curH.has_value());
    }

    spinWidth_->setValue(curW.value_or(400.0));
    spinHeight_->setValue(curH.value_or(300.0));
    spinPadding_->setValue(pad);

    onModeToggled();
}

void ContainerSizeDialog::onModeToggled()
{
    bool isLocked = radioLocked_->isChecked();

    if (isLocked)
    {
        checkEnableMinDim_->setEnabled(false);
        spinWidth_->setEnabled(true);
        spinHeight_->setEnabled(true);
    }
    else
    {
        checkEnableMinDim_->setEnabled(true);
        bool enabled = checkEnableMinDim_->isChecked();
        spinWidth_->setEnabled(enabled);
        spinHeight_->setEnabled(enabled);
    }
}

void ContainerSizeDialog::onFitToChildren()
{
    if (!model_) return;

    qreal maxX = 100.0;
    qreal maxY = 60.0;
    bool foundChild = false;

    for (const auto& node : model_->nodes())
    {
        if (node.parentId.has_value() && *node.parentId == containerId_)
        {
            foundChild = true;
            qreal nx = 20.0;
            qreal ny = 50.0;
            if (!node.metadata.empty())
            {
                QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(node.metadata).toUtf8());
                if (doc.isObject())
                {
                    QJsonObject obj = doc.object();
                    if (obj.contains("x")) nx = obj["x"].toDouble();
                    if (obj.contains("y")) ny = obj["y"].toDouble();
                }
            }
            // Standard node bounds approximation
            maxX = std::max(maxX, nx + 160.0);
            maxY = std::max(maxY, ny + 90.0);
        }
    }

    qreal pad = spinPadding_->value();
    if (foundChild)
    {
        spinWidth_->setValue(maxX + pad);
        spinHeight_->setValue(maxY + pad);
    }
    else
    {
        spinWidth_->setValue(300.0);
        spinHeight_->setValue(200.0);
    }

    if (radioAutoFit_->isChecked())
    {
        checkEnableMinDim_->setChecked(true);
    }
    onModeToggled();
}

void ContainerSizeDialog::onResetDefaults()
{
    radioAutoFit_->setChecked(true);
    checkEnableMinDim_->setChecked(false);
    spinWidth_->setValue(400.0);
    spinHeight_->setValue(300.0);
    spinPadding_->setValue(20.0);
    onModeToggled();
}

void ContainerSizeDialog::onAccept()
{
    if (!model_)
    {
        reject();
        return;
    }

    auto n = model_->getNodeById(containerId_);
    if (!n)
    {
        reject();
        return;
    }

    QJsonObject obj;
    if (!n->metadata.empty())
    {
        QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(n->metadata).toUtf8());
        if (doc.isObject())
        {
            obj = doc.object();
        }
    }

    if (radioLocked_->isChecked())
    {
        obj["_dim_lock"] = true;
        obj["_width"] = spinWidth_->value();
        obj["_height"] = spinHeight_->value();
    }
    else
    {
        obj["_dim_lock"] = false;
        if (checkEnableMinDim_->isChecked())
        {
            obj["_width"] = spinWidth_->value();
            obj["_height"] = spinHeight_->value();
        }
        else
        {
            obj.remove("_width");
            obj.remove("_height");
        }
    }

    if (spinPadding_->value() != 20.0)
    {
        obj["_padding"] = spinPadding_->value();
    }
    else
    {
        obj.remove("_padding");
    }

    QJsonDocument outDoc(obj);
    model_->setNodeMetadata(containerId_, outDoc.toJson(QJsonDocument::Compact).toStdString());

    accept();
}

bool ContainerSizeDialog::isDimLocked() const
{
    return radioLocked_->isChecked();
}

std::optional<qreal> ContainerSizeDialog::customWidth() const
{
    if (radioLocked_->isChecked() || checkEnableMinDim_->isChecked())
    {
        return spinWidth_->value();
    }
    return std::nullopt;
}

std::optional<qreal> ContainerSizeDialog::customHeight() const
{
    if (radioLocked_->isChecked() || checkEnableMinDim_->isChecked())
    {
        return spinHeight_->value();
    }
    return std::nullopt;
}

qreal ContainerSizeDialog::padding() const
{
    return spinPadding_->value();
}
