#pragma once

#include <QObject>
#include <QString>
#include <QKeySequence>
#include <QAction>
#include <QPointer>
#include <vector>

struct ShortcutItem
{
    QString id;                 // Unique identifier, e.g. "mode.view"
    QString name;               // Display name, e.g. "View Mode"
    QString category;           // Category: "Mode", "Edit", "Layout", "Alignment", "Export", "Standard"
    QString description;        // Explanatory description
    QKeySequence defaultKey;    // Default key sequence
    QKeySequence currentKey;    // Currently assigned key sequence
    bool isStandard{false};     // True if standard / immutable system shortcut
    QPointer<QAction> action{nullptr};   // Associated QAction, if any
};

class ShortcutManager : public QObject
{
    Q_OBJECT

public:
    static ShortcutManager* instance();

    // Registration
    void registerAction(const QString& id,
                        const QString& name,
                        const QString& category,
                        const QKeySequence& defaultKey,
                        QAction* action = nullptr,
                        const QString& description = QString());

    void registerStandardShortcut(const QString& id,
                                 const QString& name,
                                 const QString& category,
                                 const QKeySequence& standardKey,
                                 const QString& description = QString());

    void setAction(const QString& id, QAction* action);

    // Queries
    const std::vector<ShortcutItem>& items() const { return items_; }
    const ShortcutItem* getItem(const QString& id) const;
    ShortcutItem* getItem(const QString& id);
    QKeySequence getShortcut(const QString& id) const;
    bool isStandard(const QString& id) const;

    // Checks if a given key sequence is reserved by a standard shortcut
    bool isStandardKeySequence(const QKeySequence& seq, QString* outStandardName = nullptr) const;

    // Checks if a key sequence conflicts with any other action
    QString findConflict(const QString& actionId, const QKeySequence& seq) const;

    // Modifications
    bool setShortcut(const QString& id, const QKeySequence& seq);
    void resetToDefault(const QString& id);
    void resetAllToDefaults();

    // Persistence
    void loadSettings();
    void saveSettings() const;

    // Apply currently configured shortcuts to registered QActions
    void applyShortcuts();

signals:
    void shortcutsChanged();

private:
    explicit ShortcutManager(QObject* parent = nullptr);
    std::vector<ShortcutItem> items_;
};
