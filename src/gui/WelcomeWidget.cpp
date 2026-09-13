#include "WelcomeWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QFrame>
#include <QFileInfo>

WelcomeWidget::WelcomeWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void WelcomeWidget::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(60, 60, 60, 60);
    mainLayout->setSpacing(50);

    // Left Column: Branding & Actions
    auto* leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(16);

    auto* titleLabel = new QLabel("OpenArch", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(28);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* subLabel = new QLabel("Architecture Diagramming & System Modeling", this);
    subLabel->setStyleSheet("color: gray; font-size: 13px; margin-bottom: 20px;");

    btnNew_ = new QPushButton("Create New Project...", this);
    btnNew_->setFixedHeight(38);
    btnNew_->setStyleSheet("text-align: left; padding-left: 15px; font-weight: bold;");

    btnOpen_ = new QPushButton("Open Existing Database / JSON...", this);
    btnOpen_->setFixedHeight(38);
    btnOpen_->setStyleSheet("text-align: left; padding-left: 15px; font-weight: bold;");

    connect(btnNew_, &QPushButton::clicked, this, &WelcomeWidget::createNewClicked);
    connect(btnOpen_, &QPushButton::clicked, this, &WelcomeWidget::openFileClicked);

    leftLayout->addWidget(titleLabel);
    leftLayout->addWidget(subLabel);
    leftLayout->addWidget(btnNew_);
    leftLayout->addWidget(btnOpen_);
    leftLayout->addStretch();

    // Divider
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);

    // Right Column: Recent Files
    auto* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(10);

    auto* recentTitle = new QLabel("Recent Projects", this);
    QFont rFont = recentTitle->font();
    rFont.setPointSize(14);
    rFont.setBold(true);
    recentTitle->setFont(rFont);

    recentList_ = new QListWidget(this);
    recentList_->setAlternatingRowColors(true);
    recentList_->setStyleSheet("QListWidget::item { padding: 8px; }");

    connect(recentList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (item)
            emit recentFileSelected(item->data(Qt::UserRole).toString());
    });

    rightLayout->addWidget(recentTitle);
    rightLayout->addWidget(recentList_);

    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addWidget(line);
    mainLayout->addLayout(rightLayout, 2);
}

void WelcomeWidget::updateRecentFiles(const QStringList& files)
{
    recentList_->clear();
    for (const QString& path : files)
    {
        auto* item = new QListWidgetItem(recentList_);
        item->setText(QFileInfo(path).fileName() + "\n" + path);
        item->setData(Qt::UserRole, path);
    }
}