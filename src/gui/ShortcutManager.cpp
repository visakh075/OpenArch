#include "ShortcutManager.h"

#include <QSettings>
#include <algorithm>

ShortcutManager::ShortcutManager(QObject* parent)
    : QObject(parent)
{
}

ShortcutManager* ShortcutManager::instance()
{
    static ShortcutManager inst;
    return &inst;
}

void ShortcutManager::registerAction(const QString& id,
                                    const QString& name,
                                    const QString& category,
                                    const QKeySequence& defaultKey,
                                    QAction* action,
                                    const QString& description)
{
    for (auto& item : items_) {
        if (item.id == id) {
            item.name = name;
            item.category = category;
            item.defaultKey = defaultKey;
            item.description = description;
            item.isStandard = false;
            if (action) {
                item.action = action;
                item.action->setShortcut(item.currentKey);
            }
            return;
        }
    }

    ShortcutItem newItem;
    newItem.id = id;
    newItem.name = name;
    newItem.category = category;
    newItem.defaultKey = defaultKey;
    newItem.currentKey = defaultKey;
    newItem.description = description;
    newItem.isStandard = false;
    newItem.action = action;
    if (action) {
        action->setShortcut(defaultKey);
    }
    items_.push_back(newItem);
}

void ShortcutManager::registerStandardShortcut(const QString& id,
                                              const QString& name,
                                              const QString& category,
                                              const QKeySequence& standardKey,
                                              const QString& description)
{
    for (auto& item : items_) {
        if (item.id == id) {
            item.name = name;
            item.category = category;
            item.defaultKey = standardKey;
            item.currentKey = standardKey;
            item.description = description;
            item.isStandard = true;
            return;
        }
    }

    ShortcutItem newItem;
    newItem.id = id;
    newItem.name = name;
    newItem.category = category;
    newItem.defaultKey = standardKey;
    newItem.currentKey = standardKey;
    newItem.description = description;
    newItem.isStandard = true;
    newItem.action = nullptr;
    items_.push_back(newItem);
}

void ShortcutManager::setAction(const QString& id, QAction* action)
{
    ShortcutItem* item = getItem(id);
    if (item) {
        item->action = action;
        if (action) {
            action->setShortcut(item->currentKey);
        }
    }
}

const ShortcutItem* ShortcutManager::getItem(const QString& id) const
{
    for (const auto& item : items_) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

ShortcutItem* ShortcutManager::getItem(const QString& id)
{
    for (auto& item : items_) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

QKeySequence ShortcutManager::getShortcut(const QString& id) const
{
    const ShortcutItem* item = getItem(id);
    return item ? item->currentKey : QKeySequence();
}

bool ShortcutManager::isStandard(const QString& id) const
{
    const ShortcutItem* item = getItem(id);
    return item ? item->isStandard : false;
}

bool ShortcutManager::isStandardKeySequence(const QKeySequence& seq, QString* outStandardName) const
{
    if (seq.isEmpty()) return false;

    struct StdKeyInfo {
        QKeySequence::StandardKey stdKey;
        const char* name;
    };

    static const std::vector<StdKeyInfo> standardKeys = {
        { QKeySequence::Cut, "Cut" },
        { QKeySequence::Copy, "Copy" },
        { QKeySequence::Paste, "Paste" },
        { QKeySequence::Delete, "Delete" },
        { QKeySequence::Find, "Find" },
        { QKeySequence::Undo, "Undo" },
        { QKeySequence::Redo, "Redo" },
        { QKeySequence::SelectAll, "Select All" },
        { QKeySequence::New, "New" },
        { QKeySequence::Open, "Open" },
        { QKeySequence::Save, "Save" },
        { QKeySequence::Quit, "Quit" }
    };

    for (const auto& info : standardKeys) {
        QKeySequence stdSeq(info.stdKey);
        if (!stdSeq.isEmpty()) {
            if (seq == stdSeq || seq.matches(stdSeq) == QKeySequence::ExactMatch) {
                if (outStandardName) *outStandardName = QString::fromUtf8(info.name);
                return true;
            }
        }
        const auto keyList = QKeySequence::keyBindings(info.stdKey);
        for (const auto& k : keyList) {
            if (seq == k || seq.matches(k) == QKeySequence::ExactMatch) {
                if (outStandardName) *outStandardName = QString::fromUtf8(info.name);
                return true;
            }
        }
    }

    if (seq == QKeySequence(Qt::Key_Backspace)) {
        if (outStandardName) *outStandardName = "Delete / Backspace";
        return true;
    }

    if (seq == QKeySequence(Qt::Key_Delete)) {
        if (outStandardName) *outStandardName = "Delete";
        return true;
    }

    for (const auto& item : items_) {
        if (item.isStandard && !item.currentKey.isEmpty()) {
            if (seq == item.currentKey || seq.matches(item.currentKey) == QKeySequence::ExactMatch) {
                if (outStandardName) *outStandardName = item.name;
                return true;
            }
        }
    }

    return false;
}

QString ShortcutManager::findConflict(const QString& actionId, const QKeySequence& seq) const
{
    if (seq.isEmpty()) return QString();

    for (const auto& item : items_) {
        if (item.id != actionId && !item.currentKey.isEmpty()) {
            if (seq == item.currentKey || seq.matches(item.currentKey) == QKeySequence::ExactMatch) {
                return item.name;
            }
        }
    }
    return QString();
}

bool ShortcutManager::setShortcut(const QString& id, const QKeySequence& seq)
{
    ShortcutItem* item = getItem(id);
    if (!item) return false;
    if (item->isStandard) {
        return false;
    }

    if (!seq.isEmpty()) {
        QString stdName;
        if (isStandardKeySequence(seq, &stdName)) {
            return false;
        }
    }

    item->currentKey = seq;
    if (item->action) {
        item->action->setShortcut(seq);
    }
    return true;
}

void ShortcutManager::resetToDefault(const QString& id)
{
    ShortcutItem* item = getItem(id);
    if (!item || item->isStandard) return;

    item->currentKey = item->defaultKey;
    if (item->action) {
        item->action->setShortcut(item->defaultKey);
    }
}

void ShortcutManager::resetAllToDefaults()
{
    for (auto& item : items_) {
        if (item.isStandard) continue;
        item.currentKey = item.defaultKey;
        if (item.action) {
            item.action->setShortcut(item.defaultKey);
        }
    }
    saveSettings();
    applyShortcuts();
}

void ShortcutManager::loadSettings()
{
    QSettings settings("OpenArch", "OpenArch");
    settings.beginGroup("Shortcuts");

    for (auto& item : items_) {
        if (item.isStandard) {
            item.currentKey = item.defaultKey;
            continue;
        }

        if (settings.contains(item.id)) {
            QString str = settings.value(item.id).toString();
            QKeySequence loadedSeq = str.isEmpty() ? QKeySequence() : QKeySequence(str, QKeySequence::PortableText);

            if (!loadedSeq.isEmpty() && isStandardKeySequence(loadedSeq)) {
                item.currentKey = item.defaultKey;
            } else {
                item.currentKey = loadedSeq;
            }
        } else {
            item.currentKey = item.defaultKey;
        }
    }
    settings.endGroup();
    applyShortcuts();
}

void ShortcutManager::saveSettings() const
{
    QSettings settings("OpenArch", "OpenArch");
    settings.beginGroup("Shortcuts");

    for (const auto& item : items_) {
        if (item.isStandard) continue;
        settings.setValue(item.id, item.currentKey.toString(QKeySequence::PortableText));
    }
    settings.endGroup();
}

void ShortcutManager::applyShortcuts()
{
    for (auto& item : items_) {
        if (item.action) {
            item.action->setShortcut(item.currentKey);
        }
    }
    emit shortcutsChanged();
}
