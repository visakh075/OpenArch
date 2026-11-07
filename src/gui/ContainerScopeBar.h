#pragma once

#include <QWidget>
#include <QString>
#include <vector>
#include <utility>
#include "core/Types.h"

class QPushButton;
class QLabel;
class QHBoxLayout;

class ContainerScopeBar : public QWidget
{
    Q_OBJECT

public:
    explicit ContainerScopeBar(QWidget* parent = nullptr);

    void setBreadcrumbTrail(const std::vector<std::pair<NodeId, QString>>& trail);
    void setScopeInfo(int nodeCount, int internalEdgeCount, int externalEdgeCount = 0);
    void clearScope();
    bool isScoped() const { return isScoped_; }

    void updateThemeStyle();

signals:
    void backClicked();
    void rootClicked();
    void crumbClicked(NodeId containerId);
    void exitClicked();
    void addChildNodeClicked();
    void frameViewClicked();
    void manageSizeClicked();

private:
    void clearCrumbsLayout();

    QHBoxLayout* mainLayout_{nullptr};
    QPushButton* btnBack_{nullptr};
    QWidget* crumbsContainer_{nullptr};
    QHBoxLayout* crumbsLayout_{nullptr};
    QLabel* infoLabel_{nullptr};
    QPushButton* btnManageSize_{nullptr};
    QPushButton* btnAddChild_{nullptr};
    QPushButton* btnFrameView_{nullptr};
    QPushButton* btnExit_{nullptr};

    bool isScoped_{false};
    std::vector<std::pair<NodeId, QString>> currentTrail_;
};

