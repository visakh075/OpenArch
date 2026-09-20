#include "ConvertDialog.h"
#include "db/DbConverter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>

ConvertDialog::ConvertDialog(QWidget* parent, const QString& initialSourcePath)
    : QDialog(parent)
{
    setupUi();

    if (!initialSourcePath.isEmpty()) {
        sourceEdit_->setText(initialSourcePath);
    }
}

void ConvertDialog::setupUi()
{
    setWindowTitle("Convert Database Format (JSON ⇄ SQLite)");
    setMinimumWidth(560);
    resize(580, 360);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(18, 18, 18, 18);

    // Title / Description
    auto* headerLabel = new QLabel("Convert Architecture File Format", this);
    QFont hFont = headerLabel->font();
    hFont.setPointSize(13);
    hFont.setBold(true);
    headerLabel->setFont(hFont);
    mainLayout->addWidget(headerLabel);

    auto* descLabel = new QLabel(
        "Convert seamlessly between JSON architecture files and SQLite database files while "
        "preserving all nodes, layers, edges, attributes, and canvas layouts.", this);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #888888; font-size: 12px;");
    mainLayout->addWidget(descLabel);

    // Source Group
    auto* sourceGroup = new QGroupBox("Source File", this);
    auto* sourceLayout = new QGridLayout(sourceGroup);
    sourceLayout->setSpacing(8);

    sourceEdit_ = new QLineEdit(sourceGroup);
    sourceEdit_->setPlaceholderText("Select source .json or .db file...");
    browseSourceBtn_ = new QPushButton("Browse...", sourceGroup);

    sourceFormatLabel_ = new QLabel("Format: Not selected", sourceGroup);
    sourceFormatLabel_->setStyleSheet("color: #888888; font-size: 11px;");

    sourceLayout->addWidget(sourceEdit_, 0, 0);
    sourceLayout->addWidget(browseSourceBtn_, 0, 1);
    sourceLayout->addWidget(sourceFormatLabel_, 1, 0, 1, 2);
    mainLayout->addWidget(sourceGroup);

    // Target Group
    auto* targetGroup = new QGroupBox("Target File", this);
    auto* targetLayout = new QGridLayout(targetGroup);
    targetLayout->setSpacing(8);

    targetEdit_ = new QLineEdit(targetGroup);
    targetEdit_->setPlaceholderText("Select destination file...");
    browseTargetBtn_ = new QPushButton("Browse...", targetGroup);

    targetFormatLabel_ = new QLabel("Format: Not selected", targetGroup);
    targetFormatLabel_->setStyleSheet("color: #888888; font-size: 11px;");

    targetLayout->addWidget(targetEdit_, 0, 0);
    targetLayout->addWidget(browseTargetBtn_, 0, 1);
    targetLayout->addWidget(targetFormatLabel_, 1, 0, 1, 2);
    mainLayout->addWidget(targetGroup);

    // Options
    openAfterConvertCheck_ = new QCheckBox("Open converted database in OpenArch after conversion", this);
    openAfterConvertCheck_->setChecked(true);
    mainLayout->addWidget(openAfterConvertCheck_);

    // Status Label
    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet("font-size: 11px;");
    mainLayout->addWidget(statusLabel_);

    // Buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    cancelBtn_ = new QPushButton("Cancel", this);
    convertBtn_ = new QPushButton("Convert", this);
    convertBtn_->setDefault(true);
    convertBtn_->setEnabled(false);

    btnLayout->addWidget(cancelBtn_);
    btnLayout->addWidget(convertBtn_);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(browseSourceBtn_, &QPushButton::clicked, this, &ConvertDialog::onBrowseSource);
    connect(browseTargetBtn_, &QPushButton::clicked, this, &ConvertDialog::onBrowseTarget);
    connect(sourceEdit_, &QLineEdit::textChanged, this, &ConvertDialog::onSourceChanged);
    connect(targetEdit_, &QLineEdit::textChanged, this, &ConvertDialog::onTargetChanged);
    connect(convertBtn_, &QPushButton::clicked, this, &ConvertDialog::onConvertClicked);
    connect(cancelBtn_, &QPushButton::clicked, this, &QDialog::reject);
}

bool ConvertDialog::shouldOpenConverted() const
{
    return openAfterConvertCheck_ && openAfterConvertCheck_->isChecked();
}

void ConvertDialog::onBrowseSource()
{
    QString file = QFileDialog::getOpenFileName(
        this,
        "Select Source Architecture File",
        sourceEdit_->text().isEmpty() ? QString() : QFileInfo(sourceEdit_->text()).absolutePath(),
        "Supported Files (*.json *.db *.sqlite *.sqlite3);;Architecture JSON (*.json);;SQLite Database (*.db *.sqlite *.sqlite3);;All Files (*.*)");

    if (!file.isEmpty()) {
        sourceEdit_->setText(file);
    }
}

void ConvertDialog::onBrowseTarget()
{
    QString src = sourceEdit_->text().trimmed();
    auto srcFmt = DbConverter::detectFormat(src.toStdString());

    QString filter;
    if (srcFmt == DbConverter::Format::Json) {
        filter = "SQLite Database (*.db *.sqlite);;All Files (*.*)";
    } else if (srcFmt == DbConverter::Format::SQLite) {
        filter = "Architecture JSON (*.json);;All Files (*.*)";
    } else {
        filter = "All Supported Files (*.db *.sqlite *.json);;All Files (*.*)";
    }

    QString file = QFileDialog::getSaveFileName(
        this,
        "Select Target File",
        targetEdit_->text().isEmpty() ? QString() : targetEdit_->text(),
        filter);

    if (!file.isEmpty()) {
        targetEdit_->setText(file);
    }
}

void ConvertDialog::onSourceChanged(const QString& path)
{
    updateFormatDetection();
    suggestTargetPath();
}

void ConvertDialog::onTargetChanged(const QString& path)
{
    updateFormatDetection();
}

void ConvertDialog::suggestTargetPath()
{
    QString src = sourceEdit_->text().trimmed();
    if (src.isEmpty()) return;

    QFileInfo fi(src);
    auto srcFmt = DbConverter::detectFormat(src.toStdString());

    QString newExt;
    if (srcFmt == DbConverter::Format::Json) {
        newExt = ".db";
    } else if (srcFmt == DbConverter::Format::SQLite) {
        newExt = ".json";
    } else {
        return;
    }

    QString suggested = fi.path() + "/" + fi.completeBaseName() + newExt;
    targetEdit_->setText(suggested);
}

void ConvertDialog::updateFormatDetection()
{
    QString src = sourceEdit_->text().trimmed();
    QString dst = targetEdit_->text().trimmed();

    auto srcFmt = DbConverter::detectFormat(src.toStdString());
    auto dstFmt = DbConverter::detectFormat(dst.toStdString());

    if (src.isEmpty()) {
        sourceFormatLabel_->setText("Format: Not selected");
        sourceFormatLabel_->setStyleSheet("color: #888888; font-size: 11px;");
    } else if (srcFmt == DbConverter::Format::Json) {
        sourceFormatLabel_->setText("Format: <b>JSON Architecture</b>");
        sourceFormatLabel_->setStyleSheet("color: #4daafc; font-size: 11px;");
    } else if (srcFmt == DbConverter::Format::SQLite) {
        sourceFormatLabel_->setText("Format: <b>SQLite Database</b>");
        sourceFormatLabel_->setStyleSheet("color: #a3be8c; font-size: 11px;");
    } else {
        sourceFormatLabel_->setText("Format: <i>Unknown / Unsupported</i>");
        sourceFormatLabel_->setStyleSheet("color: #bf616a; font-size: 11px;");
    }

    if (dst.isEmpty()) {
        targetFormatLabel_->setText("Format: Not selected");
        targetFormatLabel_->setStyleSheet("color: #888888; font-size: 11px;");
    } else if (dstFmt == DbConverter::Format::Json) {
        targetFormatLabel_->setText("Format: <b>JSON Architecture</b>");
        targetFormatLabel_->setStyleSheet("color: #4daafc; font-size: 11px;");
    } else if (dstFmt == DbConverter::Format::SQLite) {
        targetFormatLabel_->setText("Format: <b>SQLite Database</b>");
        targetFormatLabel_->setStyleSheet("color: #a3be8c; font-size: 11px;");
    } else {
        targetFormatLabel_->setText("Format: <i>Unknown / Unsupported</i>");
        targetFormatLabel_->setStyleSheet("color: #bf616a; font-size: 11px;");
    }

    bool valid = (!src.isEmpty() && !dst.isEmpty() &&
                  srcFmt != DbConverter::Format::Unknown &&
                  dstFmt != DbConverter::Format::Unknown &&
                  srcFmt != dstFmt);

    convertBtn_->setEnabled(valid);
}

void ConvertDialog::onConvertClicked()
{
    QString src = sourceEdit_->text().trimmed();
    QString dst = targetEdit_->text().trimmed();

    if (!QFile::exists(src)) {
        QMessageBox::warning(this, "Source File Missing", "The source file does not exist: " + src);
        return;
    }

    if (src == dst) {
        QMessageBox::warning(this, "Invalid Target", "Source and target cannot be the same file.");
        return;
    }

    if (QFile::exists(dst)) {
        auto ans = QMessageBox::question(
            this,
            "Overwrite Target File?",
            "The destination file already exists:\n" + dst + "\n\nDo you want to overwrite it?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (ans != QMessageBox::Yes) {
            return;
        }
    }

    statusLabel_->setText("Converting...");
    statusLabel_->setStyleSheet("color: #888888; font-size: 11px;");

    Result res = DbConverter::convert(src.toStdString(), dst.toStdString(), true);

    if (res.ok) {
        targetPath_ = dst;
        statusLabel_->setText("Conversion succeeded.");
        statusLabel_->setStyleSheet("color: #a3be8c; font-size: 11px; font-weight: bold;");

        QMessageBox::information(this, "Conversion Successful", QString::fromStdString(res.message));

        if (shouldOpenConverted()) {
            emit requestOpenDatabase(targetPath_);
        }
        accept();
    } else {
        statusLabel_->setText("Conversion failed: " + QString::fromStdString(res.message));
        statusLabel_->setStyleSheet("color: #bf616a; font-size: 11px; font-weight: bold;");
        QMessageBox::critical(this, "Conversion Failed", QString::fromStdString(res.message));
    }
}
