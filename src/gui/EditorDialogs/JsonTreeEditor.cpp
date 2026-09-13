#include "JsonTreeEditor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QComboBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QStyle>

JsonTreeEditor::JsonTreeEditor(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void JsonTreeEditor::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // --- Toolbar ---
    auto* toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(16, 16));

    actionAdd_ = toolBar->addAction("+ Add Key", this, &JsonTreeEditor::onAddItem);
    actionDelete_ = toolBar->addAction("- Delete", this, &JsonTreeEditor::onDeleteItem);
    toolBar->addSeparator();

    actionToggleRaw_ = toolBar->addAction("Raw JSON");
    actionToggleRaw_->setCheckable(true);
    connect(actionToggleRaw_, &QAction::toggled, this, &JsonTreeEditor::onToggleView);

    layout->addWidget(toolBar);

    // --- Stack Container (Tree vs Raw) ---
    stackWidget_ = new QStackedWidget(this);

    // 1. Structured Tree View
    treeWidget_ = new QTreeWidget(this);
    treeWidget_->setColumnCount(3);
    treeWidget_->setHeaderLabels({"Key / Index", "Type", "Value"});
    treeWidget_->header()->setSectionResizeMode(ColKey, QHeaderView::Interactive);
    treeWidget_->header()->setSectionResizeMode(ColType, QHeaderView::ResizeToContents);
    treeWidget_->header()->setSectionResizeMode(ColValue, QHeaderView::Stretch);
    treeWidget_->setColumnWidth(ColKey, 140);
    treeWidget_->setAlternatingRowColors(true);
    treeWidget_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);

    connect(treeWidget_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem*, int) {
        emit dataChanged();
    });

    stackWidget_->addWidget(treeWidget_);

    // 2. Raw Plaintext Fallback
    rawEditor_ = new QPlainTextEdit(this);
    rawEditor_->setStyleSheet("font-family: monospace; font-size: 12px;");
    stackWidget_->addWidget(rawEditor_);

    layout->addWidget(stackWidget_);
}

void JsonTreeEditor::setJson(const QString& jsonString)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &err);

    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        populateTree(doc.object());
        rawEditor_->setPlainText(jsonString);
    } else {
        // If empty or plain array/primitive, wrap into basic object or fallback to raw
        QJsonObject root;
        populateTree(root);
        rawEditor_->setPlainText(jsonString.isEmpty() ? "{}" : jsonString);
    }
}

void JsonTreeEditor::setJsonObject(const QJsonObject& object)
{
    populateTree(object);
    rawEditor_->setPlainText(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

void JsonTreeEditor::populateTree(const QJsonObject& obj)
{
    treeWidget_->blockSignals(true);
    treeWidget_->clear();

    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        populateItem(nullptr, it.key(), it.value());
    }

    treeWidget_->expandAll();
    treeWidget_->blockSignals(false);
}

void JsonTreeEditor::populateItem(QTreeWidgetItem* parentItem, const QString& key, const QJsonValue& val)
{
    auto* item = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(treeWidget_);
    item->setText(ColKey, key);
    item->setFlags(item->flags() | Qt::ItemIsEditable);

    int typeId = static_cast<int>(val.type());
    item->setData(ColType, TypeRole, typeId);

    switch (val.type()) {
    case QJsonValue::Bool:
        item->setText(ColType, "Boolean");
        item->setText(ColValue, val.toBool() ? "true" : "false");
        break;
    case QJsonValue::Double:
        item->setText(ColType, "Number");
        item->setText(ColValue, QString::number(val.toDouble()));
        break;
    case QJsonValue::String:
        item->setText(ColType, "String");
        item->setText(ColValue, val.toString());
        break;
    case QJsonValue::Array: {
        item->setText(ColType, "Array");
        item->setFlags(item->flags() & ~Qt::ItemIsEditable); // Value column not directly editable for lists
        QJsonArray arr = val.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            populateItem(item, QString("[%1]").arg(i), arr[i]);
        }
        break;
    }
    case QJsonValue::Object: {
        item->setText(ColType, "Object");
        item->setFlags(item->flags() & ~Qt::ItemIsEditable); // Value column not directly editable for dicts
        QJsonObject childObj = val.toObject();
        for (auto it = childObj.constBegin(); it != childObj.constEnd(); ++it) {
            populateItem(item, it.key(), it.value());
        }
        break;
    }
    case QJsonValue::Null:
    default:
        item->setText(ColType, "Null");
        item->setText(ColValue, "null");
        break;
    }
}

QJsonValue JsonTreeEditor::itemToJsonValue(QTreeWidgetItem* item) const
{
    int typeId = item->data(ColType, TypeRole).toInt();
    QString valStr = item->text(ColValue);

    switch (static_cast<QJsonValue::Type>(typeId)) {
    case QJsonValue::Bool:
        return QJsonValue(valStr.trimmed().toLower() == "true");
    case QJsonValue::Double:
        return QJsonValue(valStr.toDouble());
    case QJsonValue::String:
        return QJsonValue(valStr);
    case QJsonValue::Array: {
        QJsonArray arr;
        for (int i = 0; i < item->childCount(); ++i) {
            arr.append(itemToJsonValue(item->child(i)));
        }
        return arr;
    }
    case QJsonValue::Object: {
        QJsonObject obj;
        for (int i = 0; i < item->childCount(); ++i) {
            QTreeWidgetItem* child = item->child(i);
            obj.insert(child->text(ColKey), itemToJsonValue(child));
        }
        return obj;
    }
    case QJsonValue::Null:
    default:
        return QJsonValue(QJsonValue::Null);
    }
}

QJsonObject JsonTreeEditor::toJsonObject() const
{
    if (stackWidget_->currentWidget() == rawEditor_) {
        QJsonDocument doc = QJsonDocument::fromJson(rawEditor_->toPlainText().toUtf8());
        return doc.object();
    }

    QJsonObject root;
    for (int i = 0; i < treeWidget_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* top = treeWidget_->topLevelItem(i);
        root.insert(top->text(ColKey), itemToJsonValue(top));
    }
    return root;
}

QString JsonTreeEditor::toJsonString(QJsonDocument::JsonFormat format) const
{
    return QString::fromUtf8(QJsonDocument(toJsonObject()).toJson(format));
}

void JsonTreeEditor::onAddItem()
{
    bool ok = false;
    QString key = QInputDialog::getText(this, "Add Field", "Key Name:", QLineEdit::Normal, "", &ok);
    if (!ok || key.trimmed().isEmpty())
        return;

    QStringList types = {"String", "Number", "Boolean", "Object", "Array"};
    QString type = QInputDialog::getItem(this, "Field Type", "Select Type:", types, 0, false, &ok);
    if (!ok)
        return;

    QTreeWidgetItem* targetParent = treeWidget_->currentItem();
    if (targetParent && targetParent->data(ColType, TypeRole).toInt() != static_cast<int>(QJsonValue::Object)
                    && targetParent->data(ColType, TypeRole).toInt() != static_cast<int>(QJsonValue::Array)) {
        targetParent = targetParent->parent();
    }

    QJsonValue val;
    if (type == "String") val = "";
    else if (type == "Number") val = 0.0;
    else if (type == "Boolean") val = false;
    else if (type == "Object") val = QJsonObject();
    else if (type == "Array") val = QJsonArray();

    populateItem(targetParent, key.trimmed(), val);
    emit dataChanged();
}

void JsonTreeEditor::onDeleteItem()
{
    auto* item = treeWidget_->currentItem();
    if (!item)
        return;

    delete item;
    emit dataChanged();
}

void JsonTreeEditor::onToggleView(bool checked)
{
    if (checked) {
        syncTreeToRaw();
        stackWidget_->setCurrentWidget(rawEditor_);
        actionAdd_->setEnabled(false);
        actionDelete_->setEnabled(false);
    } else {
        syncRawToTree();
        stackWidget_->setCurrentWidget(treeWidget_);
        actionAdd_->setEnabled(true);
        actionDelete_->setEnabled(true);
    }
}

void JsonTreeEditor::syncTreeToRaw()
{
    rawEditor_->setPlainText(toJsonString(QJsonDocument::Indented));
}

void JsonTreeEditor::syncRawToTree()
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(rawEditor_->toPlainText().toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        populateTree(doc.object());
    } else {
        QMessageBox::warning(this, "Parse Error", "Invalid JSON syntax. Reverting tree changes.");
    }
}