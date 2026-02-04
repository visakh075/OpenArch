#pragma once

#include <QMainWindow>
#include <QTreeView>
#include <QSplitter>
#include <QStandardItemModel>

#include "core/ArchitectureModel.h"
#include "db/DbManagerSQLite.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void openDatabase();

private:
    void setupUi();
    void setupMenu();

    // UI
    QTreeView* navigator_{nullptr};
    QWidget*   graphView_{nullptr};
    QStandardItemModel* navModel_{nullptr};

    // Core
    DbManagerSQLite db_;
    ArchitectureModel* model_{nullptr};
};
