#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QToolBar>

class JsonTreeEditor : public QWidget
{
    Q_OBJECT

public:
    explicit JsonTreeEditor(QWidget* parent = nullptr);

    void setJson(const QString& jsonString);
    void setJsonObject(const QJsonObject& object);

    QString toJsonString(QJsonDocument::JsonFormat format = QJsonDocument::Indented) const;
    QJsonObject toJsonObject() const;

signals:
    void dataChanged();

private slots:
    void onAddItem();
    void onDeleteItem();
    void onToggleView(bool checked);
    void syncTreeToRaw();
    void syncRawToTree();

private:
    enum Column {
        ColKey = 0,
        ColType = 1,
        ColValue = 2
    };

    enum ItemRole {
        TypeRole = Qt::UserRole + 1
    };

    void setupUi();
    void populateTree(const QJsonObject& obj);
    void populateItem(QTreeWidgetItem* parentItem, const QString& key, const QJsonValue& val);

    QJsonValue itemToJsonValue(QTreeWidgetItem* item) const;
    QJsonObject itemToJsonObject(QTreeWidgetItem* rootItem) const;

    QTreeWidget* treeWidget_{nullptr};
    QPlainTextEdit* rawEditor_{nullptr};
    QStackedWidget* stackWidget_{nullptr};

    QAction* actionAdd_{nullptr};
    QAction* actionDelete_{nullptr};
    QAction* actionToggleRaw_{nullptr};
};