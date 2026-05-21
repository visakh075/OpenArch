
#pragma once

#include <QDockWidget>
#include <QColor>

#include <functional>
#include "GraphThemeManager.h"
class QPushButton;
class QSpinBox;
class QLabel;
class QDoubleSpinBox;
class QCheckBox;
class QGroupBox;
class QVBoxLayout;

class ThemeEditorDock : public QDockWidget
{
    Q_OBJECT

public:

    explicit ThemeEditorDock(
        QWidget* parent = nullptr);

private:

    void buildNodeSection(QVBoxLayout* parentLayout);
    void buildEdgeSection(QVBoxLayout* parentLayout);
    void buildArrowSection(QVBoxLayout* parentLayout);
    void buildGridSection(QVBoxLayout* parentLayout);
    void buildSceneSection(QVBoxLayout* parentLayout);
    void buildTextSection(QVBoxLayout* parentLayout);

    QGroupBox* createSection(
        const QString& title,
        QVBoxLayout*& outLayout);

    QWidget* createColorEditor(
        const QString& title,
        QColor initial,
        std::function<void(const QColor&)> onChanged);

    QWidget* createIntEditor(
        const QString& title,
        int value,
        int min,
        int max,
        std::function<void(int)> onChanged);

    QWidget* createDoubleEditor(
        const QString& title,
        double value,
        double min,
        double max,
        double step,
        std::function<void(double)> onChanged);

    QWidget* createBoolEditor(
        const QString& title,
        bool value,
        std::function<void(bool)> onChanged);

    QPushButton* makeColorButton(
        QColor initial,
        std::function<void(const QColor&)> onChanged);

    template<typename T>
    void updateTheme(T updater)
    {
        updater();

        GraphThemeManager::instance()
            ->notifyThemeChanged();
    }
};