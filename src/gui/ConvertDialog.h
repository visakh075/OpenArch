#pragma once

#include <QDialog>
#include <QString>
#include <memory>

class QLineEdit;
class QPushButton;
class QLabel;
class QCheckBox;

class ConvertDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConvertDialog(QWidget* parent = nullptr, const QString& initialSourcePath = QString());
    ~ConvertDialog() override = default;

    QString convertedPath() const { return targetPath_; }
    bool shouldOpenConverted() const;

signals:
    void requestOpenDatabase(const QString& filePath);

private slots:
    void onBrowseSource();
    void onBrowseTarget();
    void onSourceChanged(const QString& path);
    void onTargetChanged(const QString& path);
    void onConvertClicked();

private:
    void setupUi();
    void updateFormatDetection();
    void suggestTargetPath();

    QLineEdit* sourceEdit_{nullptr};
    QPushButton* browseSourceBtn_{nullptr};
    QLabel* sourceFormatLabel_{nullptr};

    QLineEdit* targetEdit_{nullptr};
    QPushButton* browseTargetBtn_{nullptr};
    QLabel* targetFormatLabel_{nullptr};

    QCheckBox* openAfterConvertCheck_{nullptr};
    QLabel* statusLabel_{nullptr};

    QPushButton* convertBtn_{nullptr};
    QPushButton* cancelBtn_{nullptr};

    QString targetPath_;
};
