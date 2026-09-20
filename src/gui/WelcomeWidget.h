#pragma once

#include <QWidget>
#include <QStringList>

class QPushButton;
class QListWidget;

class WelcomeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeWidget(QWidget* parent = nullptr);
    void updateRecentFiles(const QStringList& files);

signals:
    void createNewClicked();
    void openFileClicked();
    void convertClicked();
    void recentFileSelected(const QString& filePath);

private:
    void setupUi();

    QListWidget* recentList_{nullptr};
    QPushButton* btnNew_{nullptr};
    QPushButton* btnOpen_{nullptr};
    QPushButton* btnConvert_{nullptr};
};