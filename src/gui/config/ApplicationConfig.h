#pragma once

#include <QObject>
#include <QFont>
#include <QString>

class QApplication;

class ApplicationConfig : public QObject
{
    Q_OBJECT

public:
    static ApplicationConfig& instance();

    // Call once in main() after constructing QApplication to load and apply initial configuration
    static void initialize(QApplication* app);

    // Auto-Save Configuration
    bool isAutoSaveEnabled() const;
    void setAutoSaveEnabled(bool enabled);

    int autoSaveDelayMs() const;
    void setAutoSaveDelayMs(int ms);

    // Application Font Configuration
    QFont applicationFont() const;
    void setApplicationFont(const QFont& font);
    void resetDefaultFont();
    QFont systemDefaultFont() const;
    bool hasCustomFont() const { return hasCustomFont_; }

signals:
    void autoSaveChanged(bool enabled);
    void autoSaveDelayChanged(int ms);
    void fontChanged(const QFont& font);

private:
    explicit ApplicationConfig(QObject* parent = nullptr);
    ~ApplicationConfig() override = default;

    ApplicationConfig(const ApplicationConfig&) = delete;
    ApplicationConfig& operator=(const ApplicationConfig&) = delete;

    void loadSettings();
    void saveSettings();

    bool autoSaveEnabled_{true};
    int autoSaveDelayMs_{500};
    QFont currentFont_;
    QFont defaultSystemFont_;
    bool hasCustomFont_{false};
};

