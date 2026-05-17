#pragma once

#include <QDockWidget>

class QPushButton;
class QSpinBox;
class QLabel;

class ThemeEditorDock : public QDockWidget
{
    Q_OBJECT

public:

    explicit ThemeEditorDock(
        QWidget* parent = nullptr);

private:

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

    QPushButton* makeColorButton(
        QColor initial,
        std::function<void(const QColor&)> onChanged);
};