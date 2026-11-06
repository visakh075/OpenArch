#include "ContainerScopeBar.h"
#include "gui/theme/ApplicationThemeManager.h"
#include "gui/theme/GraphThemeManager.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QLayoutItem>

ContainerScopeBar::ContainerScopeBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(40);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    mainLayout_ = new QHBoxLayout(this);
    mainLayout_->setContentsMargins(10, 4, 10, 4);
    mainLayout_->setSpacing(8);

    // 1. Back button
    btnBack_ = new QPushButton(this);
    btnBack_->setText("⬅ Back (Esc)");
    btnBack_->setCursor(Qt::PointingHandCursor);
    btnBack_->setToolTip("Return to parent container or root architecture (Esc)");
    connect(btnBack_, &QPushButton::clicked, this, &ContainerScopeBar::backClicked);
    mainLayout_->addWidget(btnBack_);

    // 2. Breadcrumbs container
    crumbsContainer_ = new QWidget(this);
    crumbsLayout_ = new QHBoxLayout(crumbsContainer_);
    crumbsLayout_->setContentsMargins(0, 0, 0, 0);
    crumbsLayout_->setSpacing(4);
    mainLayout_->addWidget(crumbsContainer_);

    mainLayout_->addStretch(1);

    // 3. Info label
    infoLabel_ = new QLabel(this);
    infoLabel_->setAlignment(Qt::AlignCenter);
    mainLayout_->addWidget(infoLabel_);

    // 4. Quick Action Buttons
    btnAddChild_ = new QPushButton("+ Add Node Here", this);
    btnAddChild_->setCursor(Qt::PointingHandCursor);
    btnAddChild_->setToolTip("Add a new node directly inside this container");
    connect(btnAddChild_, &QPushButton::clicked, this, &ContainerScopeBar::addChildNodeClicked);
    mainLayout_->addWidget(btnAddChild_);

    btnFrameView_ = new QPushButton("🔍 Frame View", this);
    btnFrameView_->setCursor(Qt::PointingHandCursor);
    btnFrameView_->setToolTip("Zoom to fit container contents");
    connect(btnFrameView_, &QPushButton::clicked, this, &ContainerScopeBar::frameViewClicked);
    mainLayout_->addWidget(btnFrameView_);

    btnExit_ = new QPushButton("✕ Exit Isolation", this);
    btnExit_->setCursor(Qt::PointingHandCursor);
    btnExit_->setToolTip("Exit container focus and return to full architecture diagram");
    connect(btnExit_, &QPushButton::clicked, this, &ContainerScopeBar::exitClicked);
    mainLayout_->addWidget(btnExit_);

    if (auto* am = ApplicationThemeManager::instance())
    {
        connect(am, &ApplicationThemeManager::themeChanged, this, &ContainerScopeBar::updateThemeStyle);
    }
    if (auto* tm = GraphThemeManager::instance())
    {
        connect(tm, &GraphThemeManager::themeChanged, this, &ContainerScopeBar::updateThemeStyle);
    }

    updateThemeStyle();
    clearScope();
}

void ContainerScopeBar::clearCrumbsLayout()
{
    while (QLayoutItem* item = crumbsLayout_->takeAt(0))
    {
        if (QWidget* w = item->widget())
        {
            w->deleteLater();
        }
        delete item;
    }
}

void ContainerScopeBar::setBreadcrumbTrail(const std::vector<std::pair<NodeId, QString>>& trail)
{
    currentTrail_ = trail;
    clearCrumbsLayout();

    if (trail.empty())
    {
        isScoped_ = false;
        setVisible(false);
        return;
    }

    isScoped_ = true;
    setVisible(true);

    const auto colors = ApplicationThemeManager::currentThemeColors();
    QString textCol = colors.text.name();
    QString accentCol = colors.accent.name();
    QString btnBg = colors.button.name();
    QString borderCol = colors.border.name();

    // Root button
    auto* rootBtn = new QPushButton("🏠 Root Architecture", crumbsContainer_);
    rootBtn->setCursor(Qt::PointingHandCursor);
    rootBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 3px 8px; font-weight: 500; font-size: 11px; }"
        "QPushButton:hover { background: %4; color: %5; }"
    ).arg(btnBg, textCol, borderCol, accentCol, colors.isDark ? "#ffffff" : "#000000"));
    connect(rootBtn, &QPushButton::clicked, this, &ContainerScopeBar::rootClicked);
    crumbsLayout_->addWidget(rootBtn);

    for (size_t i = 0; i < trail.size(); ++i)
    {
        auto* sep = new QLabel("›", crumbsContainer_);
        sep->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 12px; margin: 0 2px;").arg(colors.border.name()));
        crumbsLayout_->addWidget(sep);

        NodeId id = trail[i].first;
        QString name = trail[i].second;
        bool isCurrent = (i == trail.size() - 1);

        auto* crumbBtn = new QPushButton(QString("📦 %1").arg(name), crumbsContainer_);
        crumbBtn->setCursor(Qt::PointingHandCursor);

        if (isCurrent)
        {
            crumbBtn->setStyleSheet(QString(
                "QPushButton { background: %1; color: %2; border: 1px solid %1; border-radius: 4px; padding: 3px 10px; font-weight: bold; font-size: 11px; }"
            ).arg(accentCol, colors.isDark ? "#ffffff" : "#ffffff"));
            crumbBtn->setToolTip("Current active container scope");
        }
        else
        {
            crumbBtn->setStyleSheet(QString(
                "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 3px 8px; font-weight: 500; font-size: 11px; }"
                "QPushButton:hover { background: %4; color: %5; }"
            ).arg(btnBg, textCol, borderCol, accentCol, colors.isDark ? "#ffffff" : "#000000"));
            connect(crumbBtn, &QPushButton::clicked, this, [this, id]() {
                emit crumbClicked(id);
            });
        }
        crumbsLayout_->addWidget(crumbBtn);
    }
}

void ContainerScopeBar::setScopeInfo(int nodeCount, int internalEdgeCount)
{
    const auto colors = ApplicationThemeManager::currentThemeColors();
    infoLabel_->setText(QString("Scope: %1 Node%2, %3 Edge%4")
        .arg(nodeCount)
        .arg(nodeCount == 1 ? "" : "s")
        .arg(internalEdgeCount)
        .arg(internalEdgeCount == 1 ? "" : "s"));
    infoLabel_->setStyleSheet(QString("color: %1; font-size: 11px; padding: 0 8px; font-weight: 500;")
        .arg(colors.text.name()));
}

void ContainerScopeBar::clearScope()
{
    isScoped_ = false;
    currentTrail_.clear();
    clearCrumbsLayout();
    infoLabel_->clear();
    setVisible(false);
}

void ContainerScopeBar::updateThemeStyle()
{
    const auto colors = ApplicationThemeManager::currentThemeColors();

    QString barBg = colors.base.isValid() ? colors.base.name() : colors.window.name();
    QString borderCol = colors.border.name();
    QString textCol = colors.text.name();
    QString btnBg = colors.button.name();
    QString accentCol = colors.accent.name();

    setStyleSheet(QString(
        "ContainerScopeBar {"
        "  background-color: %1;"
        "  border-bottom: 2px solid %2;"
        "}"
    ).arg(barBg, accentCol));

    QString defaultBtnStyle = QString(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 4px;"
        "  padding: 4px 10px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  border-color: %4;"
        "  color: %4;"
        "}"
    ).arg(btnBg, textCol, borderCol, accentCol);

    btnBack_->setStyleSheet(defaultBtnStyle);
    btnAddChild_->setStyleSheet(defaultBtnStyle);
    btnFrameView_->setStyleSheet(defaultBtnStyle);

    btnExit_->setStyleSheet(QString(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 4px;"
        "  padding: 4px 10px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background-color: #d9534f;"
        "  border-color: #d9534f;"
        "  color: #ffffff;"
        "}"
    ).arg(btnBg, textCol, borderCol));

    if (!currentTrail_.empty())
    {
        setBreadcrumbTrail(currentTrail_);
    }
}

