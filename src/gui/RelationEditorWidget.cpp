#include "RelationEditorWidget.h"

#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

RelationEditorWidget::RelationEditorWidget(QWidget* parent)
    : QWidget(parent) {

    searchEdit_  = new QLineEdit;
    searchList_  = new QListWidget;
    currentList_ = new QListWidget;
    addBtn_      = new QPushButton("Add");
    removeBtn_   = new QPushButton("Remove");

    searchEdit_->setPlaceholderText("Search by name...");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(searchEdit_);
    mainLayout->addWidget(searchList_);

    auto* btnLayout = new QHBoxLayout;
    btnLayout->addWidget(addBtn_);
    btnLayout->addWidget(removeBtn_);
    mainLayout->addLayout(btnLayout);

    mainLayout->addWidget(currentList_);

    connect(searchEdit_, &QLineEdit::textChanged,
            this, &RelationEditorWidget::onFilterTextChanged);

    connect(addBtn_, &QPushButton::clicked,
            this, &RelationEditorWidget::onAdd);

    connect(removeBtn_, &QPushButton::clicked,
            this, &RelationEditorWidget::onRemove);
}

void RelationEditorWidget::setAllItems(const QStringList& names) {
    allItems_ = names;
    searchList_->clear();
    searchList_->addItems(allItems_);
}

void RelationEditorWidget::setCurrentItems(const QStringList& names) {
    currentList_->clear();
    currentList_->addItems(names);
}

void RelationEditorWidget::onFilterTextChanged(const QString& text) {
    searchList_->clear();
    for (const auto& name : allItems_) {
        if (name.contains(text, Qt::CaseInsensitive)) {
            searchList_->addItem(name);
        }
    }
}

void RelationEditorWidget::onAdd() {
    auto* item = searchList_->currentItem();
    if (!item) return;
    emit addRequested(item->text());
}

void RelationEditorWidget::onRemove() {
    auto* item = currentList_->currentItem();
    if (!item) return;
    emit removeRequested(item->text());
}
