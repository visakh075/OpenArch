#pragma once

#include <QWidget>
#include <QStringList>

class QLineEdit;
class QListWidget;
class QPushButton;

/*
 * Generic search + add/remove editor.
 * - User searches by NAME
 * - Emits add/remove requests by NAME
 * - Does NOT talk to DB or ArchitectureModel
 */
class RelationEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit RelationEditorWidget(QWidget* parent = nullptr);

    // All possible items (names only)
    void setAllItems(const QStringList& names);

    // Currently associated items (names only)
    void setCurrentItems(const QStringList& names);

signals:
    void addRequested(const QString& name);
    void removeRequested(const QString& name);

private slots:
    void onFilterTextChanged(const QString& text);
    void onAdd();
    void onRemove();

private:
    QLineEdit*   searchEdit_;
    QListWidget* searchList_;
    QListWidget* currentList_;
    QPushButton* addBtn_;
    QPushButton* removeBtn_;

    QStringList allItems_;
};
