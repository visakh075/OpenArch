#include "ApplicationConfig.h"
#include <QApplication>
#include <QSettings>
#include <algorithm>

ApplicationConfig& ApplicationConfig::instance()
{
    static ApplicationConfig configInstance;
    return configInstance;
}

ApplicationConfig::ApplicationConfig(QObject* parent)
    : QObject(parent)
{
    if (qApp)
    {
        defaultSystemFont_ = qApp->font();
        currentFont_ = defaultSystemFont_;
    }
    loadSettings();
}

void ApplicationConfig::initialize(QApplication* app)
{
    auto& cfg = instance();
    if (app && cfg.hasCustomFont_)
    {
        app->setFont(cfg.currentFont_);
    }
}

bool ApplicationConfig::isAutoSaveEnabled() const
{
    return autoSaveEnabled_;
}

void ApplicationConfig::setAutoSaveEnabled(bool enabled)
{
    if (autoSaveEnabled_ != enabled)
    {
        autoSaveEnabled_ = enabled;
        saveSettings();
        emit autoSaveChanged(autoSaveEnabled_);
    }
}

int ApplicationConfig::autoSaveDelayMs() const
{
    return autoSaveDelayMs_;
}

void ApplicationConfig::setAutoSaveDelayMs(int ms)
{
    ms = std::clamp(ms, 100, 10000);
    if (autoSaveDelayMs_ != ms)
    {
        autoSaveDelayMs_ = ms;
        saveSettings();
        emit autoSaveDelayChanged(autoSaveDelayMs_);
    }
}

QFont ApplicationConfig::applicationFont() const
{
    return currentFont_;
}

void ApplicationConfig::setApplicationFont(const QFont& font)
{
    currentFont_ = font;
    hasCustomFont_ = true;
    saveSettings();

    if (qApp)
    {
        qApp->setFont(currentFont_);
    }

    emit fontChanged(currentFont_);
}

void ApplicationConfig::resetDefaultFont()
{
    hasCustomFont_ = false;
    currentFont_ = defaultSystemFont_;
    saveSettings();

    if (qApp)
    {
        qApp->setFont(currentFont_);
    }

    emit fontChanged(currentFont_);
}

QFont ApplicationConfig::systemDefaultFont() const
{
    return defaultSystemFont_;
}

void ApplicationConfig::loadSettings()
{
    QSettings settings;
    autoSaveEnabled_ = settings.value("General/AutoSaveEnabled", true).toBool();
    autoSaveDelayMs_ = settings.value("General/AutoSaveDelayMs", 500).toInt();
    autoSaveDelayMs_ = std::clamp(autoSaveDelayMs_, 100, 10000);

    hasCustomFont_ = settings.value("Appearance/HasCustomFont", false).toBool();
    if (hasCustomFont_)
    {
        QString family = settings.value("Appearance/FontFamily", defaultSystemFont_.family()).toString();
        int size = settings.value("Appearance/FontSize", defaultSystemFont_.pointSize()).toInt();
        int weight = settings.value("Appearance/FontWeight", defaultSystemFont_.weight()).toInt();
        bool italic = settings.value("Appearance/FontItalic", defaultSystemFont_.italic()).toBool();

        QFont customFont(family, size, weight, italic);
        currentFont_ = customFont;
    }
    else
    {
        currentFont_ = defaultSystemFont_;
    }
}

void ApplicationConfig::saveSettings()
{
    QSettings settings;
    settings.setValue("General/AutoSaveEnabled", autoSaveEnabled_);
    settings.setValue("General/AutoSaveDelayMs", autoSaveDelayMs_);

    settings.setValue("Appearance/HasCustomFont", hasCustomFont_);
    if (hasCustomFont_)
    {
        settings.setValue("Appearance/FontFamily", currentFont_.family());
        settings.setValue("Appearance/FontSize", currentFont_.pointSize());
        settings.setValue("Appearance/FontWeight", currentFont_.weight());
        settings.setValue("Appearance/FontItalic", currentFont_.italic());
    }
    else
    {
        settings.remove("Appearance/FontFamily");
        settings.remove("Appearance/FontSize");
        settings.remove("Appearance/FontWeight");
        settings.remove("Appearance/FontItalic");
    }
}

